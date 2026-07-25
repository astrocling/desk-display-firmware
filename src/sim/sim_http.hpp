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
 * Non-blocking HTTPS GET for a dedicated async channel.
 * Starts a background GET on first call for a URL; returns false while in
 * flight; returns true once with the body when complete. Failures return
 * false and clear the worker so the poller can retry / advance its timer.
 * Never blocks the UI/sweep thread.
 *
 * ADS-B and map-context MUST use separate entry points — they must not share
 * an in-flight slot or one poller will clobber the other's Ready/Failed state.
 */
bool simAdsbHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
                    void* user);

bool simMapContextHttpGet(const char* url, char* body, std::size_t bodyCap,
                          std::size_t& bodyLen, void* user);

}  // namespace sim
