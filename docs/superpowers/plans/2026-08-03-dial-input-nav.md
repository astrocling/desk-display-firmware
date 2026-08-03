# Dial Input + Nav Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dial reads encoder + CST816, emits `Rotate`/`CenterTap`, drives `desk_display::Nav`, and shows mode/screen on Serial plus an LVGL text overlay on the existing green background.

**Architecture:** Pure quadrature decode, center-tap timing, and status formatting live in `lib/desk_display` (native-tested). Dial HAL (`encoder.*`, `touch.*`) polls hardware and feeds those helpers. `main.cpp` drains events into `Nav`, advances idle via `on_tick`, and refreshes Serial + one LVGL label on state change.

**Tech Stack:** C++17, PlatformIO Arduino ESP32 (`env:dial`), `Wire.h` I²C, LVGL 8.4 (existing), Unity (`pio test -e native`)

**Spec:** `docs/superpowers/specs/2026-08-03-dial-input-nav-design.md`

## Global Constraints

- Gestures this pass: `Rotate` + `CenterTap` only (no Tap/DoubleTap/LongPress)
- CenterTap = any short press/release anywhere (≤ ~400 ms); no center-circle gate
- Positive encoder delta = next carousel screen (same as sim Right / `D`)
- Keep Wi‑Fi + solid green (`0x1B5E20`) display bring-up behavior
- Pins (Knob 1.8): Encoder A=8 B=7; Touch SDA=11 SCL=12 CST816 `@0x15`
- `lib/desk_display` must not gain Arduino / LVGL / Dial GPIO deps
- No full carousel chrome, no screen ports, no LVGL indevs this pass
- Upload: `~/.platformio/penv311/bin/pio`, USB `usbmodem*` (flip cable if wrong chip)
- Never log Wi‑Fi password

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/encoder_decode.hpp` + `src/encoder_decode.cpp` | Pure AB quadrature → signed ticks |
| `lib/desk_display/include/desk_display/center_tap.hpp` + `src/center_tap.cpp` | Contact down/up timing → center tap |
| `lib/desk_display/include/desk_display/nav_status.hpp` + `src/nav_status.cpp` | Format overlay + Serial strings from mode/screen |
| `test/test_input/test_main.cpp` | Native tests for the three helpers |
| `src/hal/board_pins.hpp` | Add encoder + touch pin/address constants |
| `src/hal/encoder.hpp` / `encoder.cpp` | GPIO + ISR/poll → drain clamped delta |
| `src/hal/touch.hpp` / `touch.cpp` | Wire + CST816 poll → center_tap via helper |
| `src/hal/nav_overlay.hpp` / `nav_overlay.cpp` | Create/update LVGL status label |
| `src/main.cpp` | Init input + Nav; loop drain → Nav → overlay/Serial |
| `README.md` | Status + verify steps for input/nav |

---

### Task 1: Pure input helpers (native TDD)

**Files:**
- Create: `lib/desk_display/include/desk_display/encoder_decode.hpp`
- Create: `lib/desk_display/src/encoder_decode.cpp`
- Create: `lib/desk_display/include/desk_display/center_tap.hpp`
- Create: `lib/desk_display/src/center_tap.cpp`
- Create: `lib/desk_display/include/desk_display/nav_status.hpp`
- Create: `lib/desk_display/src/nav_status.cpp`
- Create: `test/test_input/test_main.cpp`

**Interfaces:**
- Produces:
  - `constexpr int8_t kEncoderDeltaClamp = 8;`
  - `class EncoderDecoder` with `void reset();`, `int8_t update(bool a_high, bool b_high);` — returns ticks produced by this sample (usually −1/0/+1); internal state tracks last AB. Use 2-bit gray transitions: valid neighbors only; ignore illegal jumps.
  - `int8_t clampEncoderDelta(int32_t delta);` — clamp to `[−kEncoderDeltaClamp, +kEncoderDeltaClamp]`
  - `constexpr uint32_t kCenterTapMaxMs = 400;`
  - `constexpr uint32_t kCenterTapRefractoryMs = 80;`
  - `class CenterTapDetector` with `void reset();`, `bool onContact(bool down, uint32_t now_ms);` — returns `true` once when a short down→up completes; ignores holds longer than `kCenterTapMaxMs`; after fire, ignore until `kCenterTapRefractoryMs` elapses
  - `const char* screenTitleUpper(Screen s);` — `"CLOCK"`, `"TIMEZONES"`, `"WEATHER"`, `"SPORTS"`, `"RADAR"`, else `""`
  - `const char* navModeUpper(NavMode m);` — `"FOCUSED"` / `"CAROUSEL"`
  - `void formatNavOverlay(NavMode mode, Screen screen, char* buf, size_t buf_len);` — e.g. `"FOCUSED  CLOCK"` / `"CAROUSEL WEATHER"` (two spaces between mode and title when Focused to align visually is optional; prefer single space: `"FOCUSED CLOCK"`)
  - `void formatNavSerial(NavMode mode, Screen screen, char* buf, size_t buf_len);` — e.g. `"nav: Focused Clock"` / `"nav: Carousel Weather"` (title case words for Serial)

- [ ] **Step 1: Write failing tests** in `test/test_input/test_main.cpp`

```cpp
#include <unity.h>

#include <cstring>

#include "desk_display/center_tap.hpp"
#include "desk_display/encoder_decode.hpp"
#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"

using desk_display::CenterTapDetector;
using desk_display::EncoderDecoder;
using desk_display::NavMode;
using desk_display::Screen;
using desk_display::clampEncoderDelta;
using desk_display::formatNavOverlay;
using desk_display::formatNavSerial;
using desk_display::kCenterTapMaxMs;
using desk_display::kEncoderDeltaClamp;
using desk_display::navModeUpper;
using desk_display::screenTitleUpper;

void test_encoder_forward_quarter_turns(void) {
  EncoderDecoder d;
  // Idle 00; CW gray: 00 → 01 → 11 → 10 → 00 = +1 per full cycle of 4 edges
  // Implementer: document which AB bit order is (A<<1)|B; tests assume A=bit1, B=bit0.
  int8_t sum = 0;
  sum += d.update(false, false);
  sum += d.update(false, true);   // 00 → 01
  sum += d.update(true, true);    // 01 → 11
  sum += d.update(true, false);   // 11 → 10
  sum += d.update(false, false);  // 10 → 00
  TEST_ASSERT_EQUAL_INT8(1, sum);  // one detent / full cycle → +1 (or +4 if counting edges — MUST be +1 detent; see Step 3)
}

void test_encoder_backward(void) {
  EncoderDecoder d;
  int8_t sum = 0;
  sum += d.update(false, false);
  sum += d.update(true, false);   // 00 → 10
  sum += d.update(true, true);    // 10 → 11
  sum += d.update(false, true);   // 11 → 01
  sum += d.update(false, false);  // 01 → 00
  TEST_ASSERT_EQUAL_INT8(-1, sum);
}

void test_encoder_clamp(void) {
  TEST_ASSERT_EQUAL_INT8(kEncoderDeltaClamp, clampEncoderDelta(100));
  TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(-kEncoderDeltaClamp),
                         clampEncoderDelta(-100));
  TEST_ASSERT_EQUAL_INT8(3, clampEncoderDelta(3));
}

void test_center_tap_short_press(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_TRUE(t.onContact(false, 1000 + 200));
}

void test_center_tap_long_hold_ignored(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_FALSE(t.onContact(false, 1000 + kCenterTapMaxMs + 1));
}

void test_center_tap_refractory(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_TRUE(t.onContact(false, 1100));
  TEST_ASSERT_FALSE(t.onContact(true, 1120));
  TEST_ASSERT_FALSE(t.onContact(false, 1200));  // still in refractory from 1100
}

void test_nav_status_strings(void) {
  TEST_ASSERT_EQUAL_STRING("CLOCK", screenTitleUpper(Screen::Clock));
  TEST_ASSERT_EQUAL_STRING("WEATHER", screenTitleUpper(Screen::Weather));
  TEST_ASSERT_EQUAL_STRING("FOCUSED", navModeUpper(NavMode::Focused));
  TEST_ASSERT_EQUAL_STRING("CAROUSEL", navModeUpper(NavMode::Carousel));

  char overlay[32];
  formatNavOverlay(NavMode::Focused, Screen::Clock, overlay, sizeof(overlay));
  TEST_ASSERT_EQUAL_STRING("FOCUSED CLOCK", overlay);
  formatNavOverlay(NavMode::Carousel, Screen::Weather, overlay, sizeof(overlay));
  TEST_ASSERT_EQUAL_STRING("CAROUSEL WEATHER", overlay);

  char serial[40];
  formatNavSerial(NavMode::Focused, Screen::Clock, serial, sizeof(serial));
  TEST_ASSERT_EQUAL_STRING("nav: Focused Clock", serial);
  formatNavSerial(NavMode::Carousel, Screen::Weather, serial, sizeof(serial));
  TEST_ASSERT_EQUAL_STRING("nav: Carousel Weather", serial);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_encoder_forward_quarter_turns);
  RUN_TEST(test_encoder_backward);
  RUN_TEST(test_encoder_clamp);
  RUN_TEST(test_center_tap_short_press);
  RUN_TEST(test_center_tap_long_hold_ignored);
  RUN_TEST(test_center_tap_refractory);
  RUN_TEST(test_nav_status_strings);
  return UNITY_END();
}
```

**Encoder detent note:** Prefer **one tick per 4-edge gray cycle** (one detent). If hardware emits one edge per detent with the Waveshare ISR style, Task 2 may feed a simpler edge count — but the pure decoder above must satisfy these tests.

- [ ] **Step 2: Run tests to verify they fail**

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio test -e native -f test_input
```

Expected: FAIL (missing headers / undefined symbols)

- [ ] **Step 3: Implement helpers**

`encoder_decode.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace desk_display {

constexpr int8_t kEncoderDeltaClamp = 8;

class EncoderDecoder {
 public:
  void reset();
  /** Feed current A/B levels; return ticks from this transition (−1/0/+1). */
  int8_t update(bool a_high, bool b_high);

 private:
  bool have_prev_ = false;
  uint8_t prev_ = 0;
  int8_t partial_ = 0;  // accumulate edge direction; emit ±1 every 4 valid steps
};

int8_t clampEncoderDelta(int32_t delta);

}  // namespace desk_display
```

Implement gray-code neighbor table in `.cpp`: for each valid transition, add ±1 to `partial_`; when `partial_` reaches ±4, emit ±1 and clear (or equivalent that yields +1/−1 per full cycle in the tests).

`center_tap.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace desk_display {

constexpr uint32_t kCenterTapMaxMs = 400;
constexpr uint32_t kCenterTapRefractoryMs = 80;

class CenterTapDetector {
 public:
  void reset();
  /** Returns true when a short press completes on this call. */
  bool onContact(bool down, uint32_t now_ms);

 private:
  bool down_ = false;
  uint32_t down_at_ms_ = 0;
  uint32_t refractory_until_ms_ = 0;
};

}  // namespace desk_display
```

`nav_status.hpp` / `.cpp`: implement `screenTitleUpper`, `navModeUpper`, `formatNavOverlay`, `formatNavSerial` with `snprintf` into caller buffers (null-terminate; no-op if `buf_len == 0`). Serial titles: `Clock`, `Timezones`, `Weather`, `Sports`, `Radar`. Modes: `Focused`, `Carousel`.

- [ ] **Step 4: Run tests to verify they pass**

```bash
pio test -e native -f test_input
```

Expected: PASS (all 7 tests)

Also:

```bash
pio test -e native -f test_nav
```

Expected: PASS (unchanged)

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/encoder_decode.hpp \
  lib/desk_display/src/encoder_decode.cpp \
  lib/desk_display/include/desk_display/center_tap.hpp \
  lib/desk_display/src/center_tap.cpp \
  lib/desk_display/include/desk_display/nav_status.hpp \
  lib/desk_display/src/nav_status.cpp \
  test/test_input/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add encoder, center-tap, and nav status helpers

Pure dial-input logic for native tests before HAL wiring.
EOF
)"
```

---

### Task 2: Pins + encoder HAL

**Files:**
- Modify: `src/hal/board_pins.hpp`
- Create: `src/hal/encoder.hpp`
- Create: `src/hal/encoder.cpp`

**Interfaces:**
- Consumes: `desk_display::EncoderDecoder`, `desk_display::clampEncoderDelta`, pins
- Produces:
  - `pins::kEncoderA = 8`, `pins::kEncoderB = 7`
  - `bool encoderInit();` — configure INPUT_PULLUP; Serial `encoder: ready`; return true
  - `int8_t encoderDrain();` — return clamped ticks since last drain
  - Optional: `constexpr bool kEncoderInvert = false;` — if true, negate drained delta (fix after hardware check without changing Nav)

- [ ] **Step 1: Extend `board_pins.hpp`**

Add after LCD pins:

```cpp
constexpr int kEncoderA = 8;
constexpr int kEncoderB = 7;
constexpr int kTouchSda = 11;
constexpr int kTouchScl = 12;
constexpr uint8_t kTouchAddr = 0x15;
```

(Need `#include <cstdint>` if not already present for `uint8_t`.)

- [ ] **Step 2: Implement encoder HAL**

`encoder.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace desk_hal {

bool encoderInit();
/** Signed ticks since last call, clamped to ±kEncoderDeltaClamp. */
int8_t encoderDrain();

}  // namespace desk_hal
```

`encoder.cpp` approach (pick one; prefer ISR for reliability while loop is busy with LVGL):

```cpp
#include "hal/encoder.hpp"
#include "hal/board_pins.hpp"

#include "desk_display/encoder_decode.hpp"

#include <Arduino.h>

namespace desk_hal {
namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
desk_display::EncoderDecoder s_decoder;
volatile int32_t s_accum = 0;

void IRAM_ATTR onEncoderEdge() {
  const bool a = digitalRead(pins::kEncoderA);
  const bool b = digitalRead(pins::kEncoderB);
  // ISR: only call decoder if it is ISR-safe (no heap). It must be.
  const int8_t d = s_decoder.update(a, b);
  if (d != 0) {
    portENTER_CRITICAL_ISR(&s_mux);
    s_accum += d;
    portEXIT_CRITICAL_ISR(&s_mux);
  }
}

}  // namespace

bool encoderInit() {
  pinMode(pins::kEncoderA, INPUT_PULLUP);
  pinMode(pins::kEncoderB, INPUT_PULLUP);
  s_decoder.reset();
  s_accum = 0;
  // Seed decoder with current levels
  s_decoder.update(digitalRead(pins::kEncoderA), digitalRead(pins::kEncoderB));
  attachInterrupt(digitalPinToInterrupt(pins::kEncoderA), onEncoderEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pins::kEncoderB), onEncoderEdge, CHANGE);
  Serial.println("encoder: ready");
  return true;
}

int8_t encoderDrain() {
  int32_t raw = 0;
  portENTER_CRITICAL(&s_mux);
  raw = s_accum;
  s_accum = 0;
  portEXIT_CRITICAL(&s_mux);
  // if (kEncoderInvert) raw = -raw;
  return desk_display::clampEncoderDelta(raw);
}

}  // namespace desk_hal
```

If ISR + `EncoderDecoder` detent logic under-counts on hardware, fall back to hardware-explorer style (ISR on A `RISING`, `±1` from B) **inside `encoder.cpp` only**, still clamp via `clampEncoderDelta`. Keep pure decoder tests green either way.

- [ ] **Step 3: Compile dial**

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio run -e dial
```

Expected: SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/hal/board_pins.hpp src/hal/encoder.hpp src/hal/encoder.cpp
git commit -m "$(cat <<'EOF'
feat: add Dial rotary encoder HAL

GPIO 8/7 interrupts drain clamped rotate ticks for Nav.
EOF
)"
```

---

### Task 3: CST816 touch HAL

**Files:**
- Create: `src/hal/touch.hpp`
- Create: `src/hal/touch.cpp`

**Interfaces:**
- Consumes: `desk_display::CenterTapDetector`, `pins::kTouchSda/Scl/Addr`
- Produces:
  - `bool touchInit();` — `Wire.begin(SDA,SCL)`; probe `0x15`; on success Serial `touch: ready` and return true; on fail Serial `touch: not found` and return false (non-fatal)
  - `bool touchPollCenterTap();` — read finger presence; feed `CenterTapDetector`; return true if a center tap fired this poll

**CST816 read (Knob 1.8 — from hardware explorer):**

```cpp
// Write reg 0x00, read 7 bytes:
// data[2] = finger count (>0 => down)
// data[3..4] / [5..6] = x/y 12-bit (unused this pass)
```

Poll every loop; do not require INT GPIO (not in locked pin map).

- [ ] **Step 1: Add `touch.hpp`**

```cpp
#pragma once

namespace desk_hal {

bool touchInit();
/** True if a short press completed since last successful poll path. */
bool touchPollCenterTap();

}  // namespace desk_hal
```

- [ ] **Step 2: Implement `touch.cpp`**

```cpp
#include "hal/touch.hpp"
#include "hal/board_pins.hpp"

#include "desk_display/center_tap.hpp"

#include <Arduino.h>
#include <Wire.h>

namespace desk_hal {
namespace {

bool s_ok = false;
desk_display::CenterTapDetector s_tap;

bool readFingerDown(bool& down) {
  Wire.beginTransmission(pins::kTouchAddr);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t n = Wire.requestFrom(pins::kTouchAddr, static_cast<uint8_t>(7));
  if (n < 3) {
    return false;
  }
  uint8_t data[7] = {};
  for (size_t i = 0; i < n && i < 7; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  down = data[2] > 0;
  return true;
}

}  // namespace

bool touchInit() {
  Wire.begin(pins::kTouchSda, pins::kTouchScl);
  Wire.beginTransmission(pins::kTouchAddr);
  if (Wire.endTransmission() != 0) {
    Serial.println("touch: not found");
    s_ok = false;
    return false;
  }
  s_tap.reset();
  s_ok = true;
  Serial.println("touch: ready");
  return true;
}

bool touchPollCenterTap() {
  if (!s_ok) {
    return false;
  }
  bool down = false;
  if (!readFingerDown(down)) {
    return false;
  }
  return s_tap.onContact(down, millis());
}

}  // namespace desk_hal
```

- [ ] **Step 3: Compile dial**

```bash
pio run -e dial
```

Expected: SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/hal/touch.hpp src/hal/touch.cpp
git commit -m "$(cat <<'EOF'
feat: add Dial CST816 touch HAL for center tap

Poll finger presence and emit short-press center taps.
EOF
)"
```

---

### Task 4: Nav overlay + wire main loop

**Files:**
- Create: `src/hal/nav_overlay.hpp`
- Create: `src/hal/nav_overlay.cpp`
- Modify: `src/main.cpp`
- Modify: `src/hal/lvgl_port.hpp` / `lvgl_port.cpp` only if overlay needs a “LVGL ready” query — prefer `navOverlayInit()` that no-ops when `lv_disp_get_default() == nullptr`

**Interfaces:**
- Consumes: `desk_display::Nav`, `formatNavOverlay` / `formatNavSerial`, `encoderDrain`, `touchPollCenterTap`, existing wifi/lvgl
- Produces:
  - `bool navOverlayInit();` — create centered label (light text on green); return false if no display
  - `void navOverlaySetText(const char* text);`
  - `main`: after display init, `encoderInit`, `touchInit` (ignore false), construct `Nav`, `navOverlayInit`, print initial Serial status; loop: wifi + lvgl + drain inputs → Nav + idle tick → update overlay/Serial on change

- [ ] **Step 1: Implement overlay**

`nav_overlay.hpp`:

```cpp
#pragma once

namespace desk_hal {

bool navOverlayInit();
void navOverlaySetText(const char* text);

}  // namespace desk_hal
```

`nav_overlay.cpp`: create `lv_label` on `lv_scr_act()`, white/light gray text, `lv_obj_align(..., LV_ALIGN_CENTER, 0, 0)` or top-center `LV_ALIGN_TOP_MID, 0, 24`; `lv_label_set_text`.

- [ ] **Step 2: Rewrite `src/main.cpp` wiring**

```cpp
/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"

#include "hal/encoder.hpp"
#include "hal/lvgl_port.hpp"
#include "hal/nav_overlay.hpp"
#include "hal/touch.hpp"
#include "net/wifi.hpp"

namespace {

desk_display::Nav g_nav;
uint32_t g_last_ms = 0;
desk_display::NavMode g_last_mode = desk_display::NavMode::Focused;
desk_display::Screen g_last_screen = desk_display::Screen::Clock;

void publishNavStatus() {
  char overlay[32];
  char serial[40];
  const auto mode = g_nav.mode();
  const auto screen = g_nav.active_screen();
  desk_display::formatNavOverlay(mode, screen, overlay, sizeof(overlay));
  desk_display::formatNavSerial(mode, screen, serial, sizeof(serial));
  desk_hal::navOverlaySetText(overlay);
  Serial.println(serial);
  g_last_mode = mode;
  g_last_screen = screen;
}

bool navChanged() {
  return g_nav.mode() != g_last_mode || g_nav.active_screen() != g_last_screen;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
  if (!desk_hal::lvglPortInit()) {
    Serial.println("display: lvgl port failed");
  } else {
    Serial.println("display: ready");
  }
  desk_hal::encoderInit();
  desk_hal::touchInit();  // may fail; rotate-only still OK
  desk_hal::navOverlayInit();
  g_nav.reset();
  g_last_ms = millis();
  publishNavStatus();
}

void loop() {
  desk_net::wifiLoop();
  desk_hal::lvglPortHandler();

  const uint32_t now = millis();
  uint32_t elapsed = now - g_last_ms;
  g_last_ms = now;

  const int8_t rot = desk_hal::encoderDrain();
  if (rot != 0) {
    g_nav.on_rotate(rot);
  }
  if (desk_hal::touchPollCenterTap()) {
    g_nav.on_center_tap();
  }

  g_nav.on_tick(elapsed);

  if (navChanged()) {
    publishNavStatus();
  }

  delay(5);
}
```

**Note:** `lvglPortInit` already leaves green background; do **not** remove that. If `display: ready` is already printed inside display init, avoid duplicate — match current code (today only `lvgl port failed` is printed from main; display HAL may already print `display: ready`). Keep a single clear ready line.

- [ ] **Step 3: Compile dial**

```bash
pio run -e dial
```

Expected: SUCCESS

- [ ] **Step 4: Flash and manual verify** (when hardware available)

```bash
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
pio device monitor -e dial
```

Expected Serial: wifi lines, `display: ready`, `encoder: ready`, `touch: ready` (or not found), `nav: Focused Clock`. Tap → `nav: Carousel Clock`; rotate → other screens; tap → Focused; idle ~60s in Carousel → Focused Clock. Overlay text matches.

- [ ] **Step 5: Commit**

```bash
git add src/hal/nav_overlay.hpp src/hal/nav_overlay.cpp src/main.cpp
git commit -m "$(cat <<'EOF'
feat: wire Dial encoder/touch into Nav with status overlay

Drive shell navigation on device and show mode/screen on Serial + LVGL.
EOF
)"
```

---

### Task 5: Docs

**Files:**
- Modify: `README.md` (Current status + Flashing notes)
- Modify: `docs/HANDOFF-dial-bringup.md` if present in tree (update “What’s next” / working now)

- [ ] **Step 1: Update README Current status**

Replace the sentence that says touch/encoder/nav are still to come with: Dial has Wi‑Fi, solid-color display, **encoder + CST816 center-tap driving on-device `Nav`**, with Serial + overlay status — full screens still to come.

- [ ] **Step 2: Add verify bullets under Flashing notes**

7. **Input/nav verify:** Serial `encoder: ready`, `touch: ready` (or not found), boot `nav: Focused Clock`; short tap → Carousel; rotate cycles highlight names; tap enters Focused; idle ~60s from Carousel returns to Focused Clock. Overlay text matches Serial.

- [ ] **Step 3: Commit**

```bash
git add README.md docs/HANDOFF-dial-bringup.md
git commit -m "$(cat <<'EOF'
docs: note Dial input and on-device Nav bring-up

EOF
)"
```

---

## Self-review (plan vs spec)

| Spec requirement | Task |
|------------------|------|
| Encoder GPIO 8/7 → Rotate | Task 2 + 4 |
| CST816 → CenterTap anywhere short press | Task 1 + 3 + 4 |
| Wire existing `Nav` | Task 4 |
| Serial + LVGL overlay | Task 1 (`nav_status`) + 4 |
| Idle `on_tick` 60s | Task 4 |
| Touch fail → rotate-only | Task 3 + 4 |
| No Tap/DoubleTap/LongPress / chrome / screens | Global constraints |
| Native tests for pure logic | Task 1 |
| Keep wifi + green bg | Task 4 |

No TBD placeholders. Types: `encoderDrain` → `int8_t`; `touchPollCenterTap` → `bool`; overlay/serial formatters take `NavMode` + `Screen`.
