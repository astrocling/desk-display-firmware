#pragma once

#include <cstddef>

namespace sim {

/**
 * Blocking HTTPS GET via libcurl (~8s timeout). Writes up to `bodyCap - 1`
 * bytes into `body` and sets `bodyLen`; a response larger than the buffer
 * is treated as a failure (returns false) rather than silently truncated.
 */
bool simHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen);

/** Trampoline matching `desk_display::AdsbHttpGetFn` (ignores `user`). */
bool simAdsbHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
                    void* user);

}  // namespace sim
