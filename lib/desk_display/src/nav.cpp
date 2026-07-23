#include "desk_display/nav.hpp"

namespace desk_display {

namespace {

constexpr uint8_t kScreenCount = static_cast<uint8_t>(Screen::Count);

Screen wrap_screen(int16_t index) {
  while (index < 0) {
    index += kScreenCount;
  }
  return static_cast<Screen>(static_cast<uint8_t>(index % kScreenCount));
}

}  // namespace

Nav::Nav() { reset(); }

void Nav::reset() {
  mode_ = NavMode::Focused;
  highlighted_ = Screen::Clock;
  focused_ = Screen::Clock;
  idle_elapsed_ms_ = 0;
}

Screen Nav::active_screen() const {
  return (mode_ == NavMode::Focused) ? focused_ : highlighted_;
}

void Nav::note_activity() { idle_elapsed_ms_ = 0; }

void Nav::idle_reset() { idle_elapsed_ms_ = 0; }

void Nav::apply_idle_home() {
  mode_ = NavMode::Focused;
  highlighted_ = Screen::Clock;
  focused_ = Screen::Clock;
  idle_elapsed_ms_ = 0;
}

void Nav::cycle_highlight(int8_t delta) {
  if (delta == 0) {
    return;
  }
  const int16_t next =
      static_cast<int16_t>(highlighted_) + static_cast<int16_t>(delta);
  highlighted_ = wrap_screen(next);
}

void Nav::on_rotate(int8_t delta) {
  note_activity();
  if (mode_ == NavMode::Carousel) {
    cycle_highlight(delta);
  }
  // Focused: rotate is screen-specific; shell leaves mode/screen unchanged.
}

void Nav::on_center_tap() {
  note_activity();
  if (mode_ == NavMode::Carousel) {
    focused_ = highlighted_;
    mode_ = NavMode::Focused;
  } else {
    highlighted_ = focused_;
    mode_ = NavMode::Carousel;
  }
}

void Nav::on_tap(int16_t /*x*/, int16_t /*y*/) { note_activity(); }

void Nav::on_double_tap() { note_activity(); }

void Nav::on_long_press() { note_activity(); }

void Nav::on_tick(uint32_t elapsed_ms) {
  if (elapsed_ms == 0) {
    return;
  }

  // Already at idle home: keep timer cleared so we do not re-fire.
  if (mode_ == NavMode::Focused && focused_ == Screen::Clock &&
      highlighted_ == Screen::Clock) {
    idle_elapsed_ms_ = 0;
    return;
  }

  const uint32_t room = kIdleTimeoutMs - idle_elapsed_ms_;
  if (elapsed_ms >= room) {
    apply_idle_home();
    return;
  }
  idle_elapsed_ms_ += elapsed_ms;
}

}  // namespace desk_display
