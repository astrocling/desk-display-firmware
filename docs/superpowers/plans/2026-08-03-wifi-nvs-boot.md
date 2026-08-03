# Wi-Fi NVS Boot Connect Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dial boots, resolves Wi-Fi credentials (NVS with `config.h` seed / optional `WIFI_FORCE_CONFIG`), connects in STA mode, retries on disconnect, and README documents flash + verify over Serial.

**Architecture:** Pure credential/backoff policy lives in `lib/desk_display` (native-testable). Dial-only Arduino Wi-Fi + Preferences NVS live in `src/net/`. `main.cpp` calls `wifiSetup()` / `wifiLoop()`. Secrets stay in gitignored `include/config.h`.

**Tech Stack:** C++17, PlatformIO Arduino ESP32 (`env:dial`), `WiFi.h`, `Preferences.h`, Unity (`pio test -e native`)

**Spec:** `docs/superpowers/specs/2026-08-03-wifi-nvs-boot-design.md`

## Global Constraints

- Credentials never committed: only `include/config.h.example`; real values in gitignored `include/config.h`
- NVS namespace `wifi`, keys `ssid` / `pass` (do not use `radar` namespace)
- Precedence: NVS wins after first seed; `-DWIFI_FORCE_CONFIG` overwrites NVS from `config.h` at boot
- Placeholder SSID `your-ssid` (matching example) + empty NVS → skip `WiFi.begin`, clear Serial message
- Station mode only; Dial is 2.4 GHz only
- Connect wait ~20s; reconnect backoff starts at 5s, doubles, caps at 30s; Serial on state changes only
- No captive portal, no UI, no HTTP/NTP this pass
- Never log the Wi-Fi password

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/wifi_policy.hpp` | Placeholder check, retry backoff helper, NVS name/key constants |
| `lib/desk_display/src/wifi_policy.cpp` | Implementations |
| `src/net/wifi.hpp` | Dial Wi-Fi API: `wifiSetup`, `wifiLoop`, connected query |
| `src/net/wifi.cpp` | NVS load/save, resolve creds, `WiFi.begin`, retry loop |
| `src/main.cpp` | Call setup/loop hooks |
| `include/config.h.example` | Document force-config + placeholder contract |
| `platformio.ini` | Commented example `-DWIFI_FORCE_CONFIG` under `[env:dial]` |
| `README.md` | Flash + config + verify steps |
| `test/test_domain/test_main.cpp` | Native tests for policy helpers |

---

### Task 1: Wi-Fi policy helpers (native TDD)

**Files:**
- Create: `lib/desk_display/include/desk_display/wifi_policy.hpp`
- Create: `lib/desk_display/src/wifi_policy.cpp`
- Modify: `test/test_domain/test_main.cpp`

**Interfaces:**
- Produces:
  - `constexpr const char* kWifiNvsNamespace = "wifi";`
  - `constexpr const char* kWifiNvsKeySsid = "ssid";`
  - `constexpr const char* kWifiNvsKeyPass = "pass";`
  - `constexpr const char* kWifiPlaceholderSsid = "your-ssid";`
  - `constexpr uint32_t kWifiConnectTimeoutMs = 20000;`
  - `constexpr uint32_t kWifiRetryDelayInitialMs = 5000;`
  - `constexpr uint32_t kWifiRetryDelayMaxMs = 30000;`
  - `bool isPlaceholderWifiSsid(const char* ssid);` — true if `ssid` is null, empty, or equals `kWifiPlaceholderSsid`
  - `uint32_t nextWifiRetryDelayMs(uint32_t previousDelayMs);` — if `previousDelayMs == 0` return `kWifiRetryDelayInitialMs`; else return `min(previousDelayMs * 2, kWifiRetryDelayMaxMs)`

- [ ] **Step 1: Write failing tests** at the end of the test functions in `test/test_domain/test_main.cpp` (before `main`), and register them in `main`:

```cpp
#include "desk_display/wifi_policy.hpp"

void test_wifi_placeholder_ssid(void) {
  TEST_ASSERT_TRUE(desk_display::isPlaceholderWifiSsid(nullptr));
  TEST_ASSERT_TRUE(desk_display::isPlaceholderWifiSsid(""));
  TEST_ASSERT_TRUE(desk_display::isPlaceholderWifiSsid("your-ssid"));
  TEST_ASSERT_FALSE(desk_display::isPlaceholderWifiSsid("home-net"));
}

void test_wifi_retry_backoff(void) {
  TEST_ASSERT_EQUAL_UINT32(5000u, desk_display::nextWifiRetryDelayMs(0));
  TEST_ASSERT_EQUAL_UINT32(10000u, desk_display::nextWifiRetryDelayMs(5000));
  TEST_ASSERT_EQUAL_UINT32(20000u, desk_display::nextWifiRetryDelayMs(10000));
  TEST_ASSERT_EQUAL_UINT32(30000u, desk_display::nextWifiRetryDelayMs(20000));
  TEST_ASSERT_EQUAL_UINT32(30000u, desk_display::nextWifiRetryDelayMs(30000));
}
```

In `main`, add:

```cpp
  RUN_TEST(test_wifi_placeholder_ssid);
  RUN_TEST(test_wifi_retry_backoff);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_domain`

Expected: FAIL (missing `wifi_policy.hpp` or undefined symbols)

- [ ] **Step 3: Implement helpers**

`lib/desk_display/include/desk_display/wifi_policy.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace desk_display {

constexpr const char* kWifiNvsNamespace = "wifi";
constexpr const char* kWifiNvsKeySsid = "ssid";
constexpr const char* kWifiNvsKeyPass = "pass";
constexpr const char* kWifiPlaceholderSsid = "your-ssid";

constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiRetryDelayInitialMs = 5000;
constexpr uint32_t kWifiRetryDelayMaxMs = 30000;

bool isPlaceholderWifiSsid(const char* ssid);
uint32_t nextWifiRetryDelayMs(uint32_t previousDelayMs);

}  // namespace desk_display
```

`lib/desk_display/src/wifi_policy.cpp`:

```cpp
#include "desk_display/wifi_policy.hpp"

#include <cstring>

namespace desk_display {

bool isPlaceholderWifiSsid(const char* ssid) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return true;
  }
  return std::strcmp(ssid, kWifiPlaceholderSsid) == 0;
}

uint32_t nextWifiRetryDelayMs(uint32_t previousDelayMs) {
  if (previousDelayMs == 0) {
    return kWifiRetryDelayInitialMs;
  }
  const uint64_t doubled = static_cast<uint64_t>(previousDelayMs) * 2u;
  if (doubled >= kWifiRetryDelayMaxMs) {
    return kWifiRetryDelayMaxMs;
  }
  return static_cast<uint32_t>(doubled);
}

}  // namespace desk_display
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_domain`

Expected: PASS (including the two new tests)

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/wifi_policy.hpp \
  lib/desk_display/src/wifi_policy.cpp \
  test/test_domain/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add Wi-Fi credential policy helpers

Placeholder SSID detection and reconnect backoff for dial boot connect.
EOF
)"
```

---

### Task 2: Dial Wi-Fi module + main wiring

**Files:**
- Create: `src/net/wifi.hpp`
- Create: `src/net/wifi.cpp`
- Modify: `src/main.cpp`
- Modify: `platformio.ini` (commented force-config flag only)
- Local only (do not commit): `include/config.h` copied from example if missing

**Interfaces:**
- Consumes: `desk_display::isPlaceholderWifiSsid`, `nextWifiRetryDelayMs`, NVS constants, `kWifiConnectTimeoutMs`; `WIFI_SSID` / `WIFI_PASS` from `config.h`; optional `WIFI_FORCE_CONFIG`
- Produces:
  - `namespace desk_net { void wifiSetup(); void wifiLoop(); bool wifiIsConnected(); }`

- [ ] **Step 1: Ensure local `include/config.h` exists** (gitignored — never add to git)

```bash
test -f include/config.h || cp include/config.h.example include/config.h
```

For a real device flash, edit `WIFI_SSID` / `WIFI_PASS` to a **2.4 GHz** network. Placeholder values are fine for compile-only checks.

- [ ] **Step 2: Add `src/net/wifi.hpp`**

```cpp
#pragma once

namespace desk_net {

void wifiSetup();
void wifiLoop();
bool wifiIsConnected();

}  // namespace desk_net
```

- [ ] **Step 3: Implement `src/net/wifi.cpp`**

Requirements to implement (exact behavior):

1. Include `WiFi.h`, `Preferences.h`, `config.h`, `desk_display/wifi_policy.hpp`.
2. Buffers: SSID max 32 chars + NUL; password max 63 chars + NUL.
3. `loadNvs` / `saveNvs` using `desk_display::kWifiNvsNamespace` and key constants; empty ssid ⇒ treat as missing.
4. `resolveCredentials`:
   - If `#ifdef WIFI_FORCE_CONFIG` → copy `WIFI_SSID`/`WIFI_PASS` into buffers, `saveNvs`, Serial log `wifi: seeded from config.h (FORCE)`.
   - Else try NVS; if present and not placeholder → use it, log `wifi: credentials from NVS`.
   - Else copy from `config.h`, `saveNvs` if not placeholder, log `wifi: seeded from config.h`.
   - If still placeholder/empty → log `wifi: set WIFI_SSID/WIFI_PASS in include/config.h` and return false.
5. `wifiSetup()`: `WiFi.mode(WIFI_STA)`; if resolve fails, return; else `WiFi.begin`; wait until connected or `kWifiConnectTimeoutMs` elapsed (poll ~200ms, print `.` progress sparingly); on success log SSID, IP (`WiFi.localIP()`), RSSI; on failure log `WiFi.status()`; never print password.
6. `wifiLoop()`: if never configured (resolve failed / no begin), return; if connected, clear retry state; if disconnected, when `millis()` past next retry deadline call `WiFi.reconnect()` or `WiFi.begin` again, set next delay via `nextWifiRetryDelayMs`, log only on transition and when scheduling a retry.
7. `wifiIsConnected()` → `WiFi.status() == WL_CONNECTED`.

Suggested skeleton (fill control flow to match requirements above):

```cpp
#include "net/wifi.hpp"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "config.h"
#include "desk_display/wifi_policy.hpp"

namespace desk_net {
namespace {

constexpr size_t kSsidMax = 32;
constexpr size_t kPassMax = 63;

char gSsid[kSsidMax + 1];
char gPass[kPassMax + 1];
bool gConfigured = false;
bool gWasConnected = false;
uint32_t gRetryDelayMs = 0;
uint32_t gNextRetryAtMs = 0;

void copyCred(const char* ssid, const char* pass) {
  strncpy(gSsid, ssid ? ssid : "", kSsidMax);
  gSsid[kSsidMax] = '\0';
  strncpy(gPass, pass ? pass : "", kPassMax);
  gPass[kPassMax] = '\0';
}

bool loadNvs() {
  Preferences prefs;
  if (!prefs.begin(desk_display::kWifiNvsNamespace, true)) {
    return false;
  }
  if (!prefs.isKey(desk_display::kWifiNvsKeySsid)) {
    prefs.end();
    return false;
  }
  String ssid = prefs.getString(desk_display::kWifiNvsKeySsid, "");
  String pass = prefs.getString(desk_display::kWifiNvsKeyPass, "");
  prefs.end();
  if (ssid.length() == 0) {
    return false;
  }
  copyCred(ssid.c_str(), pass.c_str());
  return true;
}

bool saveNvs() {
  Preferences prefs;
  if (!prefs.begin(desk_display::kWifiNvsNamespace, false)) {
    return false;
  }
  const bool ok = prefs.putString(desk_display::kWifiNvsKeySsid, gSsid) > 0 &&
                  prefs.putString(desk_display::kWifiNvsKeyPass, gPass) > 0;
  prefs.end();
  return ok;
}

bool resolveCredentials() {
  // WIFI_FORCE_CONFIG / NVS / config.h per requirements
  return false;
}

bool connectBlocking() {
  WiFi.begin(gSsid, gPass);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < desk_display::kWifiConnectTimeoutMs) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

void logConnected() {
  Serial.printf("wifi: connected ssid=%s ip=%s rssi=%d\n", gSsid,
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

}  // namespace

void wifiSetup() {
  Serial.println("wifi: setup");
  WiFi.mode(WIFI_STA);
  if (!resolveCredentials()) {
    gConfigured = false;
    return;
  }
  gConfigured = true;
  if (connectBlocking()) {
    gWasConnected = true;
    gRetryDelayMs = 0;
    logConnected();
  } else {
    gWasConnected = false;
    gRetryDelayMs = desk_display::nextWifiRetryDelayMs(0);
    gNextRetryAtMs = millis() + gRetryDelayMs;
    Serial.printf("wifi: connect failed status=%d; retry in %u ms\n",
                  static_cast<int>(WiFi.status()), gRetryDelayMs);
  }
}

void wifiLoop() {
  if (!gConfigured) {
    return;
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    if (!gWasConnected) {
      logConnected();
    }
    gWasConnected = true;
    gRetryDelayMs = 0;
    return;
  }
  if (gWasConnected) {
    Serial.println("wifi: disconnected");
    gWasConnected = false;
    gRetryDelayMs = desk_display::nextWifiRetryDelayMs(0);
    gNextRetryAtMs = millis() + gRetryDelayMs;
  }
  if (static_cast<int32_t>(millis() - gNextRetryAtMs) < 0) {
    return;
  }
  Serial.println("wifi: retrying");
  WiFi.disconnect();
  WiFi.begin(gSsid, gPass);
  gRetryDelayMs = desk_display::nextWifiRetryDelayMs(gRetryDelayMs);
  gNextRetryAtMs = millis() + gRetryDelayMs;
}

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

}  // namespace desk_net
```

Complete `resolveCredentials()` fully (do not leave the stub `return false`). Include `#ifdef WIFI_FORCE_CONFIG` branch.

Note: PlatformIO `src/` layout means includes are typically `#include "net/wifi.hpp"` from `main.cpp` (header under `src/net/`).

- [ ] **Step 4: Wire `src/main.cpp`**

```cpp
/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "net/wifi.hpp"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
}

void loop() {
  desk_net::wifiLoop();
  delay(100);
}
```

- [ ] **Step 5: Comment force-config in `platformio.ini` under `[env:dial]` `build_flags`**

Add a commented line (do not enable by default):

```ini
	; -DWIFI_FORCE_CONFIG
```

- [ ] **Step 6: Compile dial env**

Run: `pio run -e dial`

Expected: SUCCESS (firmware builds). Upload is manual when hardware is attached:

```bash
pio run -e dial -t upload
pio device monitor -e dial
```

Manual verify on hardware (when available): Serial shows seed/NVS source then `wifi: connected` + IP on a 2.4 GHz AP; power-cycle without `WIFI_FORCE_CONFIG` logs NVS path; one flash with `-DWIFI_FORCE_CONFIG` re-seeds.

- [ ] **Step 7: Commit** (do **not** `git add include/config.h`)

```bash
git add src/net/wifi.hpp src/net/wifi.cpp src/main.cpp platformio.ini
git commit -m "$(cat <<'EOF'
feat: connect Dial Wi-Fi from NVS with config.h seed

Boot STA connect with optional WIFI_FORCE_CONFIG re-seed and Serial status.
EOF
)"
```

---

### Task 3: README + example config docs

**Files:**
- Modify: `README.md`
- Modify: `include/config.h.example`

**Interfaces:**
- Consumes: behaviors from Tasks 1–2
- Produces: operator-facing flash and credential instructions

- [ ] **Step 1: Update `include/config.h.example` comments** above `WIFI_SSID`:

```cpp
// Wi-Fi (2.4 GHz only on the Dial). Real secrets go in gitignored config.h.
// First boot seeds NVS namespace "wifi". Later boots use NVS unless you build with
// -DWIFI_FORCE_CONFIG (see platformio.ini [env:dial] commented flag).
// Leave WIFI_SSID as "your-ssid" only in the example — never commit real passwords.
```

Keep existing `#define WIFI_SSID "your-ssid"` / `WIFI_PASS` values unchanged.

- [ ] **Step 2: Replace README “Current status” + “Setup” + “Flashing notes”** with accurate hardware-arrived wording:

Current status blurb should say Tracks 0–C + sim remain; dial now has **Wi-Fi STA connect + NVS credentials** (display/HAL still later).

Setup section must include:

1. Install PlatformIO Core
2. `cp include/config.h.example include/config.h` and set `WIFI_SSID` / `WIFI_PASS` (2.4 GHz)
3. Note `config.h` is gitignored

Flashing section must include:

1. Prefer flashing Waveshare stock demo first to validate hardware
2. USB-C orientation / flip if “ESP32, not ESP32-S3”; prefer `usbmodem*`
3. `pio run -e dial -t upload` then `pio device monitor -e dial`
4. Success: Serial `wifi: credentials from NVS` or `seeded from config.h`, then `wifi: connected ssid=… ip=…`
5. Changing network later: edit `config.h`, enable `-DWIFI_FORCE_CONFIG` for one upload, then disable the flag

- [ ] **Step 3: Commit**

```bash
git add README.md include/config.h.example
git commit -m "$(cat <<'EOF'
docs: document Dial Wi-Fi config, NVS, and flashing

Explain 2.4 GHz config.h seeding, WIFI_FORCE_CONFIG, and Serial verify steps.
EOF
)"
```

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| gitignored `config.h` seed | 2, 3 |
| NVS `wifi` / `ssid` / `pass` | 2 |
| NVS wins; `WIFI_FORCE_CONFIG` re-seed | 2, 3 |
| Placeholder skip `begin` | 1, 2 |
| ~20s connect; 5s→30s backoff; Serial transitions | 1, 2 |
| No password in logs | 2 |
| README flash + verify | 3 |
| No portal / UI / HTTP | all (omitted) |

## Plan self-review

- No TBD/placeholder steps; dial `resolveCredentials` must be fully implemented in Task 2 (skeleton notes call that out).
- Types/names consistent: `desk_display::*` policy, `desk_net::wifiSetup/wifiLoop/wifiIsConnected`.
- Host tests cover only Arduino-free policy; device verify is manual as in the spec.
