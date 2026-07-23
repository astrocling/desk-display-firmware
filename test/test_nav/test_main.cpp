#include <unity.h>

#include <desk_display/nav.hpp>
#include <desk_display/screens_stub.hpp>
#include <desk_display/theme.hpp>

using desk_display::kIdleTimeoutMs;
using desk_display::Nav;
using desk_display::NavMode;
using desk_display::Screen;
using desk_display::screen_ops;

static Nav nav;

void setUp(void) { nav.reset(); }

void tearDown(void) {}

void test_initial_state_is_focused_clock(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.highlighted()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.active_screen()));
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_carousel_rotate_cycles_forward(void) {
  nav.on_center_tap();  // Focused Clock → Carousel (Clock highlighted)
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.highlighted()));

  nav.on_rotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Timezones),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Weather),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Sports),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Radar),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.highlighted()));
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));
}

void test_carousel_rotate_cycles_backward(void) {
  nav.on_center_tap();
  nav.on_rotate(-1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Radar),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(-1);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Sports),
                    static_cast<int>(nav.highlighted()));
  nav.on_rotate(-2);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Timezones),
                    static_cast<int>(nav.highlighted()));
}

void test_carousel_center_tap_enters_focused(void) {
  nav.on_center_tap();  // → Carousel
  nav.on_rotate(2);     // Weather
  nav.on_center_tap();  // → Focused Weather
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Weather),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Weather),
                    static_cast<int>(nav.active_screen()));
}

void test_focused_center_tap_returns_to_carousel(void) {
  nav.on_center_tap();
  nav.on_rotate(3);  // Sports
  nav.on_center_tap();
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  nav.on_center_tap();
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Sports),
                    static_cast<int>(nav.highlighted()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Sports),
                    static_cast<int>(nav.active_screen()));
}

void test_focused_rotate_does_not_change_screen(void) {
  nav.on_center_tap();
  nav.on_rotate(1);  // Timezones highlighted
  nav.on_center_tap();
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Timezones),
                    static_cast<int>(nav.focused()));

  nav.on_rotate(3);
  nav.on_rotate(-2);
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Timezones),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Timezones),
                    static_cast<int>(nav.active_screen()));
}

void test_idle_timeout_returns_focused_clock(void) {
  nav.on_center_tap();
  nav.on_rotate(4);  // Radar
  nav.on_center_tap();
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Radar),
                    static_cast<int>(nav.focused()));

  nav.on_tick(kIdleTimeoutMs);
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.highlighted()));
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_idle_timeout_from_carousel(void) {
  nav.on_center_tap();
  nav.on_rotate(2);
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));

  nav.on_tick(kIdleTimeoutMs - 1);
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Weather),
                    static_cast<int>(nav.highlighted()));

  nav.on_tick(1);
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.focused()));
}

void test_activity_resets_idle_timer(void) {
  nav.on_center_tap();
  nav.on_tick(kIdleTimeoutMs / 2);
  TEST_ASSERT_TRUE(nav.idle_elapsed_ms() > 0);

  nav.on_rotate(1);
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());

  nav.on_tick(kIdleTimeoutMs / 2);
  nav.on_center_tap();
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());

  nav.on_tick(kIdleTimeoutMs / 2);
  nav.on_tap(10, 20);
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());

  nav.on_tick(kIdleTimeoutMs / 2);
  nav.on_double_tap();
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());

  nav.on_tick(kIdleTimeoutMs / 2);
  nav.on_long_press();
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());

  nav.idle_reset();
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_idle_home_does_not_rearm_while_already_home(void) {
  nav.on_tick(kIdleTimeoutMs * 2);
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_screen_ops_registry_non_null(void) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(Screen::Count); ++i) {
    const auto &ops = screen_ops(static_cast<Screen>(i));
    TEST_ASSERT_NOT_NULL(ops.create);
    TEST_ASSERT_NOT_NULL(ops.destroy);
    TEST_ASSERT_NOT_NULL(ops.show);
    TEST_ASSERT_NOT_NULL(ops.hide);
    ops.create();
    ops.show();
    ops.hide();
    ops.destroy();
  }
}

void test_theme_tokens_defined(void) {
  TEST_ASSERT_NOT_EQUAL(desk_display::theme::kBg, desk_display::theme::kAccent);
  TEST_ASSERT_NOT_EQUAL(desk_display::theme::kDim, desk_display::theme::kAlert);
  TEST_ASSERT_EQUAL_HEX32(0x0B0F14u, desk_display::theme::kBg);
  TEST_ASSERT_EQUAL_HEX32(0x3D9CF0u, desk_display::theme::kAccent);
  TEST_ASSERT_EQUAL_HEX32(0x6B7280u, desk_display::theme::kDim);
  TEST_ASSERT_EQUAL_HEX32(0xE85D4Cu, desk_display::theme::kAlert);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_is_focused_clock);
  RUN_TEST(test_carousel_rotate_cycles_forward);
  RUN_TEST(test_carousel_rotate_cycles_backward);
  RUN_TEST(test_carousel_center_tap_enters_focused);
  RUN_TEST(test_focused_center_tap_returns_to_carousel);
  RUN_TEST(test_focused_rotate_does_not_change_screen);
  RUN_TEST(test_idle_timeout_returns_focused_clock);
  RUN_TEST(test_idle_timeout_from_carousel);
  RUN_TEST(test_activity_resets_idle_timer);
  RUN_TEST(test_idle_home_does_not_rearm_while_already_home);
  RUN_TEST(test_screen_ops_registry_non_null);
  RUN_TEST(test_theme_tokens_defined);
  return UNITY_END();
}
