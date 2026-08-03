#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** LVGL custom allocator backed by PSRAM (Dial). Falls back to internal heap. */
void* lv_psram_alloc(size_t size);
void* lv_psram_realloc(void* ptr, size_t new_size);
void lv_psram_free(void* ptr);

#ifdef __cplusplus
}
#endif
