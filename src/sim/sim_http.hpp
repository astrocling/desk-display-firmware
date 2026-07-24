#pragma once

#include <cstddef>

namespace sim {

/**
 * Blocking HTTPS GET via libcurl (~8s timeout). Writes up to `bodyCap - 1`
 * bytes into `body` and sets `bodyLen`; a response larger than the buffer
 * is treated as a failure (returns false) rather than silently truncated.
 */
bool simHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen);

/**
 * Non-blocking ADS-B transport for `AdsbPoller`.
 * Starts a background GET on first call for a URL; returns false while in
 * flight; returns true once with the body when complete. Failures return
 * false and clear the worker so the poller can retry / advance its timer.
 * Never blocks the UI/sweep thread.
 */
bool simAdsbHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
                    void* user);

}  // namespace sim
