#include "net/http_async.hpp"

#include "net/http.hpp"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace desk_net {
namespace {

enum class AsyncState : uint8_t { Idle, Busy, Ready, Failed };

struct AsyncSlot {
  SemaphoreHandle_t mu = nullptr;
  AsyncState state = AsyncState::Idle;
  char url[256]{};
  char* body = nullptr;
  std::size_t bodyLen = 0;
  std::size_t bodyCap = 0;
};

constexpr int kChannelCount = 4;
constexpr std::size_t kSlotBodyCap = 64 * 1024;
/** mbedTLS needs a deep stack; match the bumped Arduino loop size. */
constexpr uint32_t kWorkerStackBytes = 49152;
constexpr UBaseType_t kWorkerPriority = 1;

AsyncSlot g_slots[kChannelCount];
QueueHandle_t g_workQueue = nullptr;
TaskHandle_t g_worker = nullptr;
bool g_inited = false;

char* allocPsramOrHeap(std::size_t bytes) {
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) {
    p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return static_cast<char*>(p);
}

void workerTask(void* /*arg*/) {
  for (;;) {
    std::uint8_t ch = 0;
    if (xQueueReceive(g_workQueue, &ch, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (ch >= kChannelCount) {
      continue;
    }

    AsyncSlot& slot = g_slots[ch];
    char urlCopy[256];
    xSemaphoreTake(slot.mu, portMAX_DELAY);
    std::snprintf(urlCopy, sizeof(urlCopy), "%s", slot.url);
    xSemaphoreGive(slot.mu);

    char* local = allocPsramOrHeap(kSlotBodyCap);
    std::size_t localLen = 0;
    const bool ok =
        local != nullptr &&
        httpGet(urlCopy, local, kSlotBodyCap, localLen, nullptr);

    xSemaphoreTake(slot.mu, portMAX_DELAY);
    if (slot.state != AsyncState::Busy) {
      xSemaphoreGive(slot.mu);
      if (local) {
        heap_caps_free(local);
      }
      continue;
    }
    if (std::strcmp(slot.url, urlCopy) != 0) {
      // Re-aimed while in flight — free the slot for a fresh launch.
      slot.state = AsyncState::Idle;
      slot.bodyLen = 0;
      xSemaphoreGive(slot.mu);
      if (local) {
        heap_caps_free(local);
      }
      continue;
    }
    if (!ok || localLen == 0 || localLen >= slot.bodyCap || slot.body == nullptr) {
      slot.state = AsyncState::Failed;
      slot.bodyLen = 0;
      xSemaphoreGive(slot.mu);
      if (local) {
        heap_caps_free(local);
      }
      continue;
    }
    std::memcpy(slot.body, local, localLen);
    slot.body[localLen] = '\0';
    slot.bodyLen = localLen;
    slot.state = AsyncState::Ready;
    xSemaphoreGive(slot.mu);
    heap_caps_free(local);
  }
}

}  // namespace

bool httpAsyncInit() {
  if (g_inited) {
    return true;
  }

  for (int i = 0; i < kChannelCount; ++i) {
    g_slots[i].mu = xSemaphoreCreateMutex();
    g_slots[i].body = allocPsramOrHeap(kSlotBodyCap);
    if (g_slots[i].mu == nullptr || g_slots[i].body == nullptr) {
      Serial.println("http-async: slot alloc failed");
      return false;
    }
    g_slots[i].bodyCap = kSlotBodyCap;
    g_slots[i].bodyLen = 0;
    g_slots[i].state = AsyncState::Idle;
  }

  g_workQueue = xQueueCreate(kChannelCount, sizeof(std::uint8_t));
  if (g_workQueue == nullptr) {
    Serial.println("http-async: queue create failed");
    return false;
  }

  // Arduino-ESP32: stack depth is bytes.
  const BaseType_t created =
      xTaskCreate(workerTask, "http_async", kWorkerStackBytes, nullptr,
                  kWorkerPriority, &g_worker);
  if (created != pdPASS) {
    Serial.println("http-async: worker create failed");
    return false;
  }

  g_inited = true;
  Serial.println("http-async: ready");
  return true;
}

bool httpGetAsync(HttpAsyncChannel channel, const char* url, char* body,
                  std::size_t bodyCap, std::size_t& bodyLen, void* /*user*/) {
  bodyLen = 0;
  if (!g_inited || !url || !body || bodyCap == 0) {
    return false;
  }

  const int ch = static_cast<int>(channel);
  if (ch < 0 || ch >= kChannelCount) {
    return false;
  }

  AsyncSlot& slot = g_slots[ch];
  bool launch = false;
  std::uint8_t launchCh = static_cast<std::uint8_t>(ch);

  xSemaphoreTake(slot.mu, portMAX_DELAY);

  if (slot.state == AsyncState::Ready) {
    if (std::strcmp(slot.url, url) == 0) {
      if (slot.bodyLen >= bodyCap) {
        slot.state = AsyncState::Idle;
        slot.bodyLen = 0;
        xSemaphoreGive(slot.mu);
        return false;
      }
      std::memcpy(body, slot.body, slot.bodyLen);
      bodyLen = slot.bodyLen;
      slot.state = AsyncState::Idle;
      slot.bodyLen = 0;
      xSemaphoreGive(slot.mu);
      return true;
    }
    slot.state = AsyncState::Idle;
    slot.bodyLen = 0;
  }

  if (slot.state == AsyncState::Failed) {
    slot.state = AsyncState::Idle;
    slot.bodyLen = 0;
    bodyLen = 0;
    xSemaphoreGive(slot.mu);
    return true;  // terminal empty — map HardFail / adsb give-up path
  }

  if (slot.state == AsyncState::Busy) {
    if (std::strcmp(slot.url, url) != 0) {
      std::snprintf(slot.url, sizeof(slot.url), "%s", url);
      slot.bodyLen = 0;
    }
    xSemaphoreGive(slot.mu);
    return false;
  }

  // Idle → launch worker
  std::snprintf(slot.url, sizeof(slot.url), "%s", url);
  slot.bodyLen = 0;
  slot.state = AsyncState::Busy;
  launch = true;
  xSemaphoreGive(slot.mu);

  if (launch) {
    if (xQueueSend(g_workQueue, &launchCh, 0) != pdTRUE) {
      xSemaphoreTake(slot.mu, portMAX_DELAY);
      if (slot.state == AsyncState::Busy && std::strcmp(slot.url, url) == 0) {
        slot.state = AsyncState::Idle;
      }
      xSemaphoreGive(slot.mu);
      return false;
    }
  }
  return false;
}

}  // namespace desk_net
