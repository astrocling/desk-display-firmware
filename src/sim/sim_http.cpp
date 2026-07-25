#include "sim_http.hpp"

#include <curl/curl.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace sim {
namespace {

constexpr long kTimeoutSeconds = 8;
constexpr std::size_t kAsyncBodyCap = 256 * 1024;

struct WriteCtx {
  char* buf;
  std::size_t cap;
  std::size_t len;
  bool overflowed;
};

std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* ctx = static_cast<WriteCtx*>(userdata);
  const std::size_t total = size * nmemb;
  const std::size_t space = ctx->cap > ctx->len ? ctx->cap - ctx->len : 0;
  const std::size_t copy = total < space ? total : space;
  if (copy > 0) {
    std::memcpy(ctx->buf + ctx->len, ptr, copy);
    ctx->len += copy;
  }
  if (copy < total) {
    ctx->overflowed = true;
  }
  // Always report the full size consumed so libcurl doesn't abort the
  // transfer; overflow is surfaced to the caller via ctx.overflowed.
  return total;
}

void ensureGlobalInit() {
  static const bool kInitted = [] {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return true;
  }();
  (void)kInitted;
}

enum class AsyncState : uint8_t { Idle, Busy, Ready, Failed };

struct AsyncSlot {
  std::mutex mu;
  AsyncState state = AsyncState::Idle;
  char url[160]{};
  char body[kAsyncBodyCap]{};
  std::size_t bodyLen = 0;
};

// Heap-allocated and never deleted: detached curl workers may outlive main(),
// and must not touch a destroyed mutex/slot during static teardown (SIGSEGV).
AsyncSlot& adsbSlot() {
  static AsyncSlot* slot = new AsyncSlot();
  return *slot;
}

AsyncSlot& mapContextSlot() {
  static AsyncSlot* slot = new AsyncSlot();
  return *slot;
}

void runAsyncGet(AsyncSlot* slot, std::string urlCopy) {
  char localBody[kAsyncBodyCap];
  std::size_t localLen = 0;
  const bool ok = simHttpGet(urlCopy.c_str(), localBody, sizeof(localBody), localLen);

  std::lock_guard<std::mutex> lock(slot->mu);
  // Only publish if this worker still owns the in-flight URL.
  if (slot->state != AsyncState::Busy ||
      std::strcmp(slot->url, urlCopy.c_str()) != 0) {
    return;
  }
  if (!ok || localLen >= sizeof(slot->body)) {
    slot->state = AsyncState::Failed;
    slot->bodyLen = 0;
    return;
  }
  std::memcpy(slot->body, localBody, localLen);
  slot->bodyLen = localLen;
  slot->state = AsyncState::Ready;
}

bool simAsyncHttpGet(AsyncSlot& slot, const char* url, char* body, std::size_t bodyCap,
                     std::size_t& bodyLen) {
  bodyLen = 0;
  if (!url || !body || bodyCap == 0) {
    return false;
  }

  std::string launchUrl;
  bool launch = false;

  {
    std::lock_guard<std::mutex> lock(slot.mu);

    if (slot.state == AsyncState::Ready) {
      if (std::strcmp(slot.url, url) == 0) {
        if (slot.bodyLen >= bodyCap) {
          slot.state = AsyncState::Idle;
          slot.bodyLen = 0;
          return false;
        }
        std::memcpy(body, slot.body, slot.bodyLen);
        bodyLen = slot.bodyLen;
        slot.state = AsyncState::Idle;
        slot.bodyLen = 0;
        return true;
      }
      // Stale Ready for a previous URL (center/range changed) — drop it.
      slot.state = AsyncState::Idle;
      slot.bodyLen = 0;
    }

    if (slot.state == AsyncState::Failed) {
      // Consume the failure so the poller can budget another attempt / interval.
      slot.state = AsyncState::Idle;
      slot.bodyLen = 0;
      return false;
    }

    if (slot.state == AsyncState::Busy) {
      if (std::strcmp(slot.url, url) == 0) {
        return false;  // still in flight for this URL
      }
      // URL changed under an in-flight request: re-aim. The old worker will
      // no-op on URL mismatch when it finishes; launch a replacement fetch.
      std::snprintf(slot.url, sizeof(slot.url), "%s", url);
      slot.bodyLen = 0;
      launchUrl = slot.url;
      launch = true;
    } else {
      // Idle: record the URL and hand work to a detached background GET.
      std::snprintf(slot.url, sizeof(slot.url), "%s", url);
      slot.bodyLen = 0;
      slot.state = AsyncState::Busy;
      launchUrl = slot.url;
      launch = true;
    }
  }

  if (launch) {
    std::thread(runAsyncGet, &slot, std::move(launchUrl)).detach();
  }
  return false;
}

}  // namespace

bool simHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen) {
  bodyLen = 0;
  if (!url || !body || bodyCap == 0) {
    return false;
  }

  ensureGlobalInit();

  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  WriteCtx ctx{body, bodyCap, 0, false};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "desk-display-sim/1.0");
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  const CURLcode res = curl_easy_perform(curl);
  long httpStatus = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::fprintf(stderr, "sim_http: GET %s failed: %s\n", url, curl_easy_strerror(res));
    return false;
  }
  if (httpStatus < 200 || httpStatus >= 300) {
    std::fprintf(stderr, "sim_http: GET %s -> HTTP %ld\n", url, httpStatus);
    return false;
  }
  if (ctx.overflowed) {
    std::fprintf(stderr, "sim_http: response for %s exceeded buffer, dropping\n", url);
    return false;
  }

  bodyLen = ctx.len;
  return true;
}

bool simAdsbHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
                    void* /*user*/) {
  return simAsyncHttpGet(adsbSlot(), url, body, bodyCap, bodyLen);
}

bool simMapContextHttpGet(const char* url, char* body, std::size_t bodyCap,
                          std::size_t& bodyLen, void* /*user*/) {
  return simAsyncHttpGet(mapContextSlot(), url, body, bodyCap, bodyLen);
}

}  // namespace sim
