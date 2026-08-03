#pragma once

#include <cstddef>

namespace desk_net {

/**
 * Blocking HTTPS GET (WiFiClientSecure + HTTPClient).
 * Writes up to bodyCap-1 bytes; oversized responses fail.
 * Returns false if Wi‑Fi is down, request fails, or status is not 2xx.
 * Compatible with desk_display::AdsbHttpGetFn (user unused).
 */
bool httpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
             void* user = nullptr);

}  // namespace desk_net
