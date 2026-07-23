#pragma once

namespace desk_display {

/**
 * True when JSON looks like a backend 503 body: `{ "error": "..." }`.
 * Optional helper for treating cache-not-ready responses.
 */
bool isNotReadyError(const char* json);

}  // namespace desk_display
