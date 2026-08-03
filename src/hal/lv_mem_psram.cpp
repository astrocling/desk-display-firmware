#include "lv_mem_psram.h"

#include <esp_heap_caps.h>

void* lv_psram_alloc(size_t size) {
  if (size == 0) {
    return nullptr;
  }
  void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) {
    p = heap_caps_malloc(size, MALLOC_CAP_8BIT);
  }
  return p;
}

void* lv_psram_realloc(void* ptr, size_t new_size) {
  if (new_size == 0) {
    lv_psram_free(ptr);
    return nullptr;
  }
  if (ptr == nullptr) {
    return lv_psram_alloc(new_size);
  }
  void* p =
      heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) {
    p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_8BIT);
  }
  return p;
}

void lv_psram_free(void* ptr) {
  if (ptr != nullptr) {
    heap_caps_free(ptr);
  }
}
