# Dial Display HAL Bring-up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dial boots Wi‑Fi as today, initializes ST77916 over QSPI, runs LVGL 8.4 with a flush path, and fills the 360×360 screen with one solid color.

**Architecture:** Pin constants in `src/hal/board_pins.hpp`. Panel bring-up + `drawBitmap`/flush in `src/hal/display.*` using ESP-IDF `esp_lcd` QSPI. LVGL registration/tick in `src/hal/lvgl_port.*`. `main.cpp` wires Wi‑Fi → display → LVGL solid screen → `lv_timer_handler` in `loop`.

**Tech Stack:** PlatformIO Arduino ESP32 (`env:dial`, pioarduino), LVGL 8.4 (existing), `esp_lcd` panel IO + ST77916 init (Espressif component or Waveshare-compatible vendor cmds), PSRAM where useful

**Spec:** `docs/superpowers/specs/2026-08-03-dial-display-hal-design.md`

## Global Constraints

- Success UI: full-screen solid color only (no widgets required)
- Keep `desk_net::wifiSetup` / `wifiLoop` behavior
- Pins for **Knob 1.8 only**: CS=14, PCLK=13, D0–D3=15–18, RST=21, BL=47 (cross-check Waveshare `08_LVGL_Test`; do **not** use 1.85B pinouts)
- No touch, encoder, nav, or sim screen port this pass
- Never log Wi‑Fi password
- Upload/monitor on native S3 USB (`usbmodem*`); use Python 3.10+ PlatformIO (`~/.platformio/penv311/bin/pio` on this machine)
- `lib/desk_display` must not gain LVGL or Arduino display deps

## File map

| File | Responsibility |
|------|----------------|
| `src/hal/board_pins.hpp` | GPIO constants |
| `src/hal/display.hpp` / `display.cpp` | Backlight, panel init, bitmap draw used by flush |
| `src/hal/lvgl_port.hpp` / `lvgl_port.cpp` | `lv_init`, disp driver, tick, solid screen helper |
| `src/main.cpp` | Wire boot + loop |
| `platformio.ini` | Dial build flags / lib_deps if ST77916 component needed |
| `README.md` | Note display bring-up + solid-color verify |

---

### Task 1: Board pins + backlight HAL

**Files:**
- Create: `src/hal/board_pins.hpp`
- Create: `src/hal/display.hpp`
- Create: `src/hal/display.cpp` (backlight + stub init returning false until Task 2 completes panel — **or** implement backlight fully and leave `displayInit` returning error until Task 2; prefer full backlight API now)
- Modify: `platformio.ini` only if a compile flag is required (usually none)

**Interfaces:**
- Produces:
  - `namespace desk_hal { namespace pins { constexpr int kLcdCs = 14; constexpr int kLcdPclk = 13; constexpr int kLcdData0 = 15; constexpr int kLcdData1 = 16; constexpr int kLcdData2 = 17; constexpr int kLcdData3 = 18; constexpr int kLcdRst = 21; constexpr int kLcdBl = 47; } }`
  - `bool displayInit();` — Task 1: set BL pin output, turn backlight **on**, return `true` only if later panel init succeeds; for Task 1 alone, implement `displaySetBacklight(bool on)` and call it from a temporary path **or** implement `displayInit()` that turns BL on and returns `true` with Serial `display: backlight on` while panel is still TODO — **prefer:** Task 1 ends with `displaySetBacklight` + pins header compiling in dial build; `displayInit` full panel is Task 2.
  - Simpler Task 1 deliverable: pins header + `displaySetBacklight(bool)` implemented; `displayInit` declared, implemented in Task 2.

- [ ] **Step 1: Add `src/hal/board_pins.hpp`**

```cpp
#pragma once

namespace desk_hal {
namespace pins {

constexpr int kLcdCs = 14;
constexpr int kLcdPclk = 13;
constexpr int kLcdData0 = 15;
constexpr int kLcdData1 = 16;
constexpr int kLcdData2 = 17;
constexpr int kLcdData3 = 18;
constexpr int kLcdRst = 21;
constexpr int kLcdBl = 47;

}  // namespace pins
}  // namespace desk_hal
```

- [ ] **Step 2: Add `src/hal/display.hpp`**

```cpp
#pragma once

#include <cstdint>

namespace desk_hal {

bool displayInit();
void displaySetBacklight(bool on);
/** Draw RGB565 buffer for inclusive rectangle [x0,y0]–[x1,y1]. */
bool displayFlush(int x0, int y0, int x1, int y1, const uint16_t* pixels);

constexpr int kLcdWidth = 360;
constexpr int kLcdHeight = 360;

}  // namespace desk_hal
```

- [ ] **Step 3: Implement backlight in `src/hal/display.cpp`; stub `displayInit`/`displayFlush`**

```cpp
#include "hal/display.hpp"
#include "hal/board_pins.hpp"

#include <Arduino.h>

namespace desk_hal {

void displaySetBacklight(bool on) {
  pinMode(pins::kLcdBl, OUTPUT);
  digitalWrite(pins::kLcdBl, on ? HIGH : LOW);
}

bool displayInit() {
  displaySetBacklight(true);
  Serial.println("display: backlight on (panel init pending)");
  return false;  // Task 2 replaces with real panel init
}

bool displayFlush(int, int, int, int, const uint16_t*) { return false; }

}  // namespace desk_hal
```

- [ ] **Step 4: Compile dial**

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio run -e dial
```

Expected: SUCCESS (stubs link; main not wired yet — if unused, ensure files are under `src/` so they compile, or leave unreferenced until Task 3; PlatformIO compiles all `src/**` for dial except `sim/`).

If unused translation units are fine, stubs still compile.

- [ ] **Step 5: Commit**

```bash
git add src/hal/board_pins.hpp src/hal/display.hpp src/hal/display.cpp
git commit -m "$(cat <<'EOF'
feat: add Dial LCD pin map and backlight stub

Board GPIO constants and backlight control; panel init follows.
EOF
)"
```

---

### Task 2: ST77916 QSPI panel init + flush

**Files:**
- Modify: `src/hal/display.cpp` / `display.hpp` as needed
- Modify: `platformio.ini` — add Espressif ST77916 component **or** vendor init command table from Waveshare Knob 1.8 `08_LVGL_Test` / compatible JC3636 source (document URL in a short comment at top of init table)

**Interfaces:**
- Consumes: `desk_hal::pins::*`, `kLcdWidth` / `kLcdHeight`
- Produces: working `displayInit()` → `true` + Serial `display: ready`; `displayFlush(...)` via `esp_lcd_panel_draw_bitmap` (or equivalent)

**Implementation requirements:**

1. Reset sequence on `kLcdRst`.
2. Create QSPI (quad) panel IO: CLK=`kLcdPclk`, DATA0–3 as pinned, CS=`kLcdCs`. Prefer ESP-IDF `esp_lcd_new_panel_io_spi` with quad mode flags available in Arduino-ESP32 3.x / pioarduino used by this project.
3. Install ST77916 panel driver. Prefer component `espressif/esp_lcd_st77916` if it integrates cleanly with PlatformIO; otherwise copy the **vendor init command list** from Waveshare’s Knob 1.8 LVGL demo (not 1.85B) into `display.cpp` with a source URL comment.
4. `displaySetBacklight(true)` after panel is on.
5. `displayFlush`: map LVGL area to `esp_lcd_panel_draw_bitmap(panel, x0, y0, x1+1, y1+1, pixels)` (note esp_lcd often wants exclusive end coords — match the API you use).
6. On failure: Serial `display: init failed` (+ esp_err name if available); return `false`; leave backlight off or on per safest blank panel (prefer BL off if init failed).

**Color / endianness:** Panel may need RGB565 byte swap. If first bring-up shows wrong colors later under LVGL, set `LV_COLOR_16_SWAP` or swap in flush — document which you chose in the commit message. For Task 2 alone, prove flush by optionally filling the full screen once inside `displayInit` with a solid RGB565 color **0x07E0** (pure green) using `displayFlush` before returning — helps hardware debug before LVGL. Spec success is LVGL solid color (Task 3); this optional full-screen green is allowed as a temporary init self-test and may remain (overdrawn by LVGL) or be removed once Task 3 works.

- [ ] **Step 1: Implement real `displayInit` / `displayFlush`**

(Implementer writes the full `esp_lcd` setup; do not leave stubs.)

- [ ] **Step 2: Build dial**

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio run -e dial
```

Expected: SUCCESS

- [ ] **Step 3: Manual flash smoke (if device attached)**

```bash
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
```

If Task 3 not wired yet, main still won’t call `displayInit` — either temporarily call it from `main` for this smoke test and revert, **or** defer hardware proof to Task 3. Prefer deferring UI wiring to Task 3 so Task 2 is compile-proven only unless you add a one-line temporary call (must not leave broken Wi‑Fi path).

- [ ] **Step 4: Commit**

```bash
git add src/hal/display.cpp src/hal/display.hpp platformio.ini
git commit -m "$(cat <<'EOF'
feat: initialize Dial ST77916 panel over QSPI

Bring up esp_lcd flush path for 360x360 RGB565 on Knob 1.8 pins.
EOF
)"
```

---

### Task 3: LVGL port + solid color + main wiring

**Files:**
- Create: `src/hal/lvgl_port.hpp`
- Create: `src/hal/lvgl_port.cpp`
- Modify: `src/main.cpp`
- Modify: `include/lv_conf.h` or dial `build_flags` only if dial needs larger `LV_MEM_SIZE` (e.g. `-DLV_MEM_SIZE=\"(128U*1024U)\"`) — only if link/runtime requires it

**Interfaces:**
- Consumes: `desk_hal::displayInit`, `displayFlush`, `kLcdWidth`/`kLcdHeight`
- Produces:
  - `namespace desk_hal { bool lvglPortInit(); void lvglPortHandler(); }`
  - Solid screen color: **`lv_color_hex(0x1B5E20)`** (dark green — distinct from black/off)

- [ ] **Step 1: Implement `lvgl_port`**

Requirements:

1. `lv_init()`.
2. Register LVGL 8 display driver: `hor_res`/`ver_res` = 360; `flush_cb` calls `displayFlush` then `lv_disp_flush_ready`.
3. Allocate partial draw buffer: at least `360 * 40` `lv_color_t` (prefer `heap_caps_malloc(..., MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)` or PSRAM if DMA allows; if QSPI DMA requires internal, use internal).
4. Tick: call `lv_tick_inc(1)` from a 1 ms `esp_timer` **or** from `lvglPortHandler` using `millis()` delta (pick one; esp_timer preferred).
5. `lvglPortInit`: if `!displayInit()` return false; else create LVGL disp, set `lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1B5E20), 0)` and `lv_obj_set_style_bg_opa(..., LV_OPA_COVER, 0)`; Serial `display: ready` if not already printed by `displayInit`.
6. `lvglPortHandler`: `lv_timer_handler()`.

Example flush (LVGL 8):

```cpp
static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  desk_hal::displayFlush(area->x1, area->y1, area->x2, area->y2,
                         reinterpret_cast<const uint16_t*>(color_p));
  lv_disp_flush_ready(drv);
}
```

- [ ] **Step 2: Wire `src/main.cpp`**

```cpp
#include <Arduino.h>

#include "hal/lvgl_port.hpp"
#include "net/wifi.hpp"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
  if (!desk_hal::lvglPortInit()) {
    Serial.println("display: lvgl port failed");
  }
}

void loop() {
  desk_net::wifiLoop();
  desk_hal::lvglPortHandler();
  delay(5);
}
```

- [ ] **Step 3: Build + flash + verify**

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
# Capture Serial ~20s for wifi + display: ready
```

Expected hardware: round LCD shows dark green solid fill; Serial still shows Wi‑Fi connect when AP available.

- [ ] **Step 4: Commit**

```bash
git add src/hal/lvgl_port.hpp src/hal/lvgl_port.cpp src/main.cpp include/lv_conf.h platformio.ini
git commit -m "$(cat <<'EOF'
feat: show solid LVGL color on Dial after Wi-Fi setup

Register LVGL flush to ST77916 HAL and fill the active screen.
EOF
)"
```

---

### Task 4: README status update

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update Current status** — Dial has Wi‑Fi **and** display HAL solid-color bring-up; touch/encoder/nav on device still later.

- [ ] **Step 2: Add a short “Display verify” bullet under flashing** — after upload, expect solid dark green (`0x1B5E20`) and Serial `display: ready`.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "$(cat <<'EOF'
docs: note Dial solid-color display bring-up

Document expected green screen and display: ready Serial line.
EOF
)"
```

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| Solid color success UI | 3 |
| LVGL + HAL flush | 2, 3 |
| Wi‑Fi preserved | 3 |
| Knob 1.8 pins | 1, 2 |
| No touch/encoder/nav | all |
| Serial ready / fail | 2, 3 |
| README verify | 4 |

## Plan self-review

- No captive portal / screens ported.
- Pin constants match explorer + ESPHome/roon-knob Knob maps (CS14/PCLK13/D15–18/RST21/BL47).
- Host unit tests N/A for panel; dial compile + manual flash is the gate.
- Task 2 intentionally allows implementer judgment on esp_lcd vs vendored init — must cite source URL in code comments.
