#pragma once

#include <cstdint>

namespace desk_display {

/** Screens in carousel order (Clock is home). */
enum class Screen : uint8_t {
  Clock = 0,
  Timezones,
  Weather,
  Sports,
  Radar,
  Count
};

/** Top-level shell mode. */
enum class NavMode : uint8_t {
  Carousel = 0,
  Focused
};

/** Idle home: Focused on Clock (viewing the clock face, not browsing carousel). */
constexpr uint32_t kIdleTimeoutMs = 60000;

/** Idle timeout outcome for shell handling. */
enum class IdleEvent : uint8_t {
  None = 0,
  HomeToClock,     // Carousel → Focused Clock
  SettleFocused,   // Focused non-home → stay; shell must settle screens
};

class Nav {
 public:
  Nav();

  void reset();

  NavMode mode() const { return mode_; }
  Screen highlighted() const { return highlighted_; }
  Screen focused() const { return focused_; }

  /** Active screen for rendering: Focused → focused_; Carousel → highlighted_. */
  Screen active_screen() const;

  /** Encoder ticks. Positive = next screen (carousel) or deferred to screen (focused). */
  void on_rotate(int8_t delta);

  /** Short center touch — plan's "knob click". Encoder has no push button. */
  void on_center_tap();

  /** Touch events (idle reset; screen-specific handling is outside nav). */
  void on_tap(int16_t x, int16_t y);
  void on_double_tap();
  void on_long_press();

  /** Advance idle timer by elapsed milliseconds; may trigger home fallback or SettleFocused (stay on current screen). */
  IdleEvent on_tick(uint32_t elapsed_ms);

  /** Clear idle accumulator without changing mode/screen. */
  void idle_reset();

  uint32_t idle_elapsed_ms() const { return idle_elapsed_ms_; }

 private:
  void apply_idle_home();
  void cycle_highlight(int8_t delta);
  void note_activity();

  NavMode mode_;
  Screen highlighted_;
  Screen focused_;
  uint32_t idle_elapsed_ms_;
};

}  // namespace desk_display
