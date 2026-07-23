#pragma once

#include <cstdio>
#include <cstring>

#ifndef FIXTURE_DIR
#define FIXTURE_DIR "fixtures"
#endif

/** Load fixture file into buf. Tries FIXTURE_DIR first, then relative fallbacks. */
inline bool loadFixture(const char* name, char* buf, std::size_t buflen,
                        std::size_t* outLen = nullptr) {
  if (!name || !buf || buflen == 0) {
    return false;
  }

  char path[512];
  const char* bases[] = {
      FIXTURE_DIR,
      "fixtures",
      "../fixtures",
      "../../fixtures",
      "../../../fixtures",
  };

  for (const char* base : bases) {
    std::snprintf(path, sizeof(path), "%s/%s", base, name);
    FILE* f = std::fopen(path, "rb");
    if (!f) {
      continue;
    }
    const std::size_t n = std::fread(buf, 1, buflen - 1, f);
    std::fclose(f);
    buf[n] = '\0';
    if (outLen) {
      *outLen = n;
    }
    return n > 0;
  }
  return false;
}
