#include "sim_app.hpp"

#include "sdl_hal.hpp"

#include "desk_display/adsb.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/format_time.hpp"
#include "desk_display/scores.hpp"
#include "desk_display/theme.hpp"
#include "desk_display/timezones.hpp"
#include "desk_display/weather.hpp"
#include "desk_display/weather_icons.hpp"
#include "weather_img.hpp"

#include <cstdio>
#include <ctime>
#include <cstring>

// Reuse test fixture loader
#include "../../test/fixture_loader.hpp"

namespace sim {
namespace {

lv_color_t rgb(uint32_t c) {
  return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

const char* screen_name(desk_display::Screen s) {
  using desk_display::Screen;
  switch (s) {
    case Screen::Clock:
      return "Clock";
    case Screen::Timezones:
      return "Timezones";
    case Screen::Weather:
      return "Weather";
    case Screen::Sports:
      return "Sports";
    case Screen::Radar:
      return "Radar";
    default:
      return "?";
  }
}

lv_obj_t* make_status_icon(lv_obj_t* parent, desk_display::TzRowStatus st) {
  constexpr lv_coord_t kSize = 12;

  if (st == desk_display::TzRowStatus::Night) {
    // Crescent: moon disc with a background-colored cutout.
    lv_obj_t* wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, kSize, kSize);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* moon = lv_obj_create(wrap);
    lv_obj_set_size(moon, kSize, kSize);
    lv_obj_set_style_radius(moon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(moon, rgb(desk_display::theme::kStatusNight), 0);
    lv_obj_set_style_border_width(moon, 0, 0);
    lv_obj_set_style_pad_all(moon, 0, 0);
    lv_obj_align(moon, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* cut = lv_obj_create(wrap);
    lv_obj_set_size(cut, kSize - 2, kSize - 2);
    lv_obj_set_style_radius(cut, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cut, rgb(desk_display::theme::kBg), 0);
    lv_obj_set_style_border_width(cut, 0, 0);
    lv_obj_set_style_pad_all(cut, 0, 0);
    lv_obj_align(cut, LV_ALIGN_CENTER, 3, -1);
    return wrap;
  }

  lv_obj_t* dot = lv_obj_create(parent);
  lv_obj_set_size(dot, kSize, kSize);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  const uint32_t color = (st == desk_display::TzRowStatus::Working)
                             ? desk_display::theme::kStatusWorking
                             : desk_display::theme::kStatusAwake;
  lv_obj_set_style_bg_color(dot, rgb(color), 0);
  return dot;
}

}  // namespace

bool SimApp::init() {
  load_fixtures();
  sync_clock_from_wall();

  root_ = lv_obj_create(lv_scr_act());
  lv_obj_set_size(root_, kDispW, kDispH);
  lv_obj_center(root_);
  lv_obj_set_style_radius(root_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(root_, true, 0);
  lv_obj_set_style_bg_color(root_, rgb(desk_display::theme::kBg), 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  chrome_ = lv_label_create(root_);
  lv_obj_set_style_text_color(chrome_, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_text_font(chrome_, &lv_font_montserrat_12, 0);
  lv_obj_align(chrome_, LV_ALIGN_TOP_MID, 0, 10);

  content_ = lv_obj_create(root_);
  lv_obj_set_size(content_, kDispW - 24, kDispH - 48);
  lv_obj_align(content_, LV_ALIGN_CENTER, 0, 8);
  lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content_, 0, 0);
  lv_obj_set_style_pad_all(content_, 4, 0);
  lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);

  last_screen_ = desk_display::Screen::Count;
  rebuild_ui_for_active();
  return true;
}

void SimApp::load_fixtures() {
  char buf[256 * 1024];

  desk_display::Weather w{};
  if (loadFixture("weather.json", buf, sizeof(buf)) && desk_display::parseWeather(buf, w)) {
    weather_.bind(w);
  }

  desk_display::Timezones tz{};
  if (loadFixture("timezones.json", buf, sizeof(buf)) &&
      desk_display::parseTimezones(buf, tz)) {
    timezones_.setSunTimes(tz);
  }

  desk_display::Scores scores{};
  if (loadFixture("scores.json", buf, sizeof(buf)) && desk_display::parseScores(buf, scores)) {
    sports_.bind(scores);
  }

  desk_display::AircraftList ac{};
  if (loadFixture("adsb_sample.json", buf, sizeof(buf)) && desk_display::parseAdsb(buf, ac)) {
    radar_.bind(ac);
  }

  clock_.setTimezoneBoardHint(true);
  std::fprintf(stdout, "Fixtures loaded (weather/tz/scores/adsb as available).\n");
}

void SimApp::sync_clock_from_wall() {
  const std::time_t now = std::time(nullptr);
  clock_.setUnixUtc(static_cast<std::int64_t>(now));
  timezones_.setLiveUnix(static_cast<std::int64_t>(now));
}

void SimApp::rebuild_ui_for_active() {
  if (body_) {
    lv_obj_del(body_);
    body_ = nullptr;
  }
  body_ = lv_obj_create(content_);
  lv_obj_set_size(body_, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body_, 0, 0);
  lv_obj_set_style_pad_all(body_, 0, 0);
  lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);

  last_screen_ = nav_.active_screen();
  last_mode_ = nav_.mode();
  refresh_content();
}

void SimApp::refresh_content() {
  using desk_display::NavMode;
  using desk_display::Screen;

  char chrome[64];
  std::snprintf(chrome, sizeof(chrome), "%s · %s",
                nav_.mode() == NavMode::Carousel ? "Carousel" : "Focused",
                screen_name(nav_.active_screen()));
  lv_label_set_text(chrome_, chrome);

  if (!body_) {
    return;
  }
  lv_obj_clean(body_);

  // Carousel and Focused share the same default screen views. Mode only
  // changes input: Carousel rotate cycles screens; Focused rotate is in-app.
  const Screen scr = nav_.active_screen();

  switch (scr) {
    case Screen::Clock: {
      const auto v = clock_.view();
      lv_obj_t* date = lv_label_create(body_);
      lv_label_set_text(date, v.dateText);
      lv_obj_set_style_text_color(date, rgb(desk_display::theme::kDim), 0);
      lv_obj_align(date, LV_ALIGN_CENTER, 0, 40);

      char timebuf[20];
      desk_display::format12HourWithSeconds(timebuf, sizeof(timebuf), v.hour, v.minute,
                                            v.second);
      lv_obj_t* time = lv_label_create(body_);
      lv_label_set_text(time, timebuf);
      lv_obj_set_style_text_font(time, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(time, rgb(0xFFFFFF), 0);
      lv_obj_align(time, LV_ALIGN_CENTER, 0, -10);

      // Analog ticks via arcs as stand-in for hands
      lv_obj_t* face = lv_arc_create(body_);
      lv_obj_set_size(face, 200, 200);
      lv_obj_center(face);
      lv_arc_set_bg_angles(face, 0, 360);
      lv_arc_set_value(face, 0);
      lv_obj_remove_style(face, nullptr, LV_PART_KNOB);
      lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_color(face, rgb(desk_display::theme::kDim), LV_PART_MAIN);
      lv_obj_set_style_arc_width(face, 2, LV_PART_MAIN);
      lv_obj_set_style_arc_opa(face, LV_OPA_40, LV_PART_INDICATOR);

      if (v.timezoneBoardHint) {
        lv_obj_t* hint = lv_label_create(body_);
        lv_label_set_text(hint, "→ Timezones");
        lv_obj_set_style_text_color(hint, rgb(desk_display::theme::kAccent), 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
      }
      break;
    }
    case Screen::Timezones: {
      const auto v = timezones_.view();
      constexpr lv_coord_t kRowPitch = 28;
      constexpr lv_coord_t kRowCount =
          static_cast<lv_coord_t>(desk_display::kTimezoneBoardRows);
      // content_ is kDispH - 48; center the block in that area
      constexpr lv_coord_t kBodyH = static_cast<lv_coord_t>(kDispH - 48);
      const lv_coord_t startY = (kBodyH - kRowCount * kRowPitch) / 2;
      for (std::size_t i = 0; i < desk_display::kTimezoneBoardRows; ++i) {
        const auto& row = v.rows[i];

        lv_obj_t* row_obj = lv_obj_create(body_);
        lv_obj_set_size(row_obj, LV_SIZE_CONTENT, kRowPitch);
        lv_obj_set_style_bg_opa(row_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_obj, 0, 0);
        lv_obj_set_style_pad_all(row_obj, 0, 0);
        lv_obj_set_style_pad_column(row_obj, 8, 0);
        lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_align(row_obj, LV_ALIGN_TOP_MID, 0,
                     static_cast<lv_coord_t>(startY + static_cast<lv_coord_t>(i) * kRowPitch));

        make_status_icon(row_obj, row.status);

        char line[80];
        std::snprintf(line, sizeof(line), "%s  %s%s", row.timeText, row.label,
                      row.isAnchor ? " *" : "");
        lv_obj_t* lab = lv_label_create(row_obj);
        lv_label_set_text(lab, line);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
        uint32_t textColor = desk_display::theme::kDim;
        if (row.status == desk_display::TzRowStatus::Night) {
          textColor = 0x4B5563;  // dimmer for night rows
        } else if (row.isAnchor) {
          textColor = desk_display::theme::kAccent;
        }
        lv_obj_set_style_text_color(lab, rgb(textColor), 0);
      }
      break;
    }
    case Screen::Weather: {
      const auto v = weather_.view();
      if (!v.ready) {
        lv_obj_t* lab = lv_label_create(body_);
        lv_label_set_text(lab, "Weather not ready");
        lv_obj_center(lab);
        break;
      }

      char hl[48];
      std::snprintf(hl, sizeof(hl), "H %.0f  L %.0f", static_cast<double>(v.todayHigh),
                    static_cast<double>(v.todayLow));
      lv_obj_t* hl_lab = lv_label_create(body_);
      lv_label_set_text(hl_lab, hl);
      lv_obj_set_style_text_color(hl_lab, rgb(desk_display::theme::kAccent), 0);
      lv_obj_set_style_text_font(hl_lab, &lv_font_montserrat_12, 0);
      lv_obj_align(hl_lab, LV_ALIGN_TOP_MID, 0, 4);

      lv_obj_t* when = lv_label_create(body_);
      lv_label_set_text(when, v.whenLabel);
      lv_obj_set_style_text_color(when, rgb(desk_display::theme::kDim), 0);
      lv_obj_set_style_text_font(when, &lv_font_montserrat_12, 0);
      lv_obj_align(when, LV_ALIGN_CENTER, 0, -70);

      lv_obj_t* row = lv_obj_create(body_);
      lv_obj_set_size(row, 200, 48);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(row, 0, 0);
      lv_obj_set_style_pad_all(row, 0, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(row, 8, 0);
      lv_obj_align(row, LV_ALIGN_CENTER, 0, -28);

      lv_obj_t* icon = lv_img_create(row);
      lv_img_set_src(icon, weatherIconImg(v.icon));

      char tbuf[32];
      std::snprintf(tbuf, sizeof(tbuf), "%.0f°", static_cast<double>(v.displayTemp));
      lv_obj_t* temp = lv_label_create(row);
      lv_label_set_text(temp, tbuf);
      lv_obj_set_style_text_font(temp, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(temp, rgb(0xFFFFFF), 0);

      char feels_buf[32];
      std::snprintf(feels_buf, sizeof(feels_buf), "feels %.0f°",
                    static_cast<double>(v.feelsLike));
      lv_obj_t* feels = lv_label_create(body_);
      lv_label_set_text(feels, feels_buf);
      lv_obj_set_style_text_color(feels, rgb(desk_display::theme::kDim), 0);
      lv_obj_set_style_text_font(feels, &lv_font_montserrat_12, 0);
      lv_obj_align(feels, LV_ALIGN_CENTER, 0, 10);

      if (v.alertBadge) {
        lv_obj_t* alert = lv_label_create(body_);
        lv_label_set_text(alert, v.alertDetailOpen && v.alertHeadline ? v.alertHeadline : "ALERT");
        lv_obj_set_style_text_color(alert, rgb(desk_display::theme::kAlert), 0);
        lv_obj_set_style_text_font(alert, &lv_font_montserrat_12, 0);
        lv_obj_align(alert, LV_ALIGN_CENTER, 0, 36);
      }

      // Hourly strip along the bottom
      lv_obj_t* strip = lv_obj_create(body_);
      lv_obj_set_size(strip, 280, 70);
      lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(strip, 0, 0);
      lv_obj_set_style_pad_all(strip, 0, 0);
      lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -4);

      for (std::size_t i = 0; i < v.stripCount; ++i) {
        const auto& slot = v.strip[i];
        if (!slot.valid) {
          continue;
        }
        lv_obj_t* cell = lv_obj_create(strip);
        lv_obj_set_size(cell, 48, 66);
        lv_obj_set_style_radius(cell, 6, 0);
        lv_obj_set_style_border_width(cell, 0, 0);
        lv_obj_set_style_pad_all(cell, 2, 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        if (slot.selected) {
          lv_obj_set_style_bg_color(cell, rgb(desk_display::theme::kAccent), 0);
          lv_obj_set_style_bg_opa(cell, LV_OPA_30, 0);
        } else {
          lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
        }
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(cell, 2, 0);

        lv_obj_t* simg = lv_img_create(cell);
        lv_img_set_src(simg, weatherIconImg(slot.icon));
        lv_img_set_zoom(simg, 102);  // ~16px from 40px (256 = 100%)

        lv_obj_t* hour_lab = lv_label_create(cell);
        lv_label_set_text(hour_lab, slot.hourDigit);
        lv_obj_set_style_text_font(hour_lab, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(hour_lab, rgb(desk_display::theme::kDim), 0);

        char st[16];
        std::snprintf(st, sizeof(st), "%.0f°", static_cast<double>(slot.temp));
        lv_obj_t* stemp = lv_label_create(cell);
        lv_label_set_text(stemp, st);
        lv_obj_set_style_text_font(stemp, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(stemp, rgb(0xFFFFFF), 0);
      }
      break;
    }
    case Screen::Sports: {
      const auto v = sports_.view();
      if (!v.ready) {
        lv_obj_t* lab = lv_label_create(body_);
        lv_label_set_text(lab, "Scores not ready");
        lv_obj_center(lab);
        break;
      }
      const char* card =
          v.card == desk_display::SportsCard::Mlb ? "MLB" : "Flagstand";
      lv_obj_t* title = lv_label_create(body_);
      lv_label_set_text(title, card);
      lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(title, rgb(desk_display::theme::kAccent), 0);
      lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

      lv_obj_t* primary = lv_label_create(body_);
      lv_label_set_long_mode(primary, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(primary, 280);
      if (v.card == desk_display::SportsCard::Mlb) {
        lv_label_set_text(primary, v.mlb.primary);
      } else {
        lv_label_set_text(primary, v.flagstand.lastResultSummary);
      }
      lv_obj_set_style_text_color(primary, rgb(0xFFFFFF), 0);
      lv_obj_align(primary, LV_ALIGN_CENTER, 0, -10);

      lv_obj_t* secondary = lv_label_create(body_);
      lv_label_set_long_mode(secondary, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(secondary, 280);
      if (v.card == desk_display::SportsCard::Mlb) {
        lv_label_set_text(secondary, v.mlb.secondary);
      } else {
        lv_label_set_text(secondary, v.flagstand.nextRaceSummary);
      }
      lv_obj_set_style_text_color(secondary, rgb(desk_display::theme::kDim), 0);
      lv_obj_align(secondary, LV_ALIGN_CENTER, 0, 40);

      if (v.detail) {
        lv_obj_t* d = lv_label_create(body_);
        lv_label_set_text(d, "(detail)");
        lv_obj_set_style_text_color(d, rgb(desk_display::theme::kAlert), 0);
        lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -4);
      }
      break;
    }
    case Screen::Radar: {
      const auto v = radar_.view();
      lv_obj_t* ring = lv_arc_create(body_);
      lv_obj_set_size(ring, 240, 240);
      lv_obj_center(ring);
      lv_arc_set_bg_angles(ring, 0, 360);
      lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
      lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_width(ring, 1, LV_PART_MAIN);
      lv_obj_set_style_arc_color(ring, rgb(desk_display::theme::kDim), LV_PART_MAIN);

      char hdr[48];
      std::snprintf(hdr, sizeof(hdr), "%s · %.0f mi · %zu",
                    v.mode == desk_display::RadarMode::ClassicSweep ? "Sweep" : "Detail",
                    static_cast<double>(v.rangeMiles), v.blipCount);
      lv_obj_t* hdr_lab = lv_label_create(body_);
      lv_label_set_text(hdr_lab, hdr);
      lv_obj_set_style_text_font(hdr_lab, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(hdr_lab, rgb(desk_display::theme::kDim), 0);
      lv_obj_align(hdr_lab, LV_ALIGN_TOP_MID, 0, 4);

      const float scale = 110.0f / (v.rangeMiles > 0 ? v.rangeMiles : 1.0f);
      for (std::size_t i = 0; i < v.blipCount && i < 40; ++i) {
        const auto& b = v.blips[i];
        lv_obj_t* dot = lv_obj_create(body_);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, rgb(0x3DFF7A), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        const lv_coord_t x = static_cast<lv_coord_t>(b.offsetXMi * scale);
        const lv_coord_t y = static_cast<lv_coord_t>(-b.offsetYMi * scale);
        lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
      }

      if (v.hasSelection && v.detail.present) {
        char card[64];
        std::snprintf(card, sizeof(card), "%s", v.detail.callsign);
        lv_obj_t* c = lv_label_create(body_);
        lv_label_set_text(c, card);
        lv_obj_set_style_text_color(c, rgb(desk_display::theme::kAccent), 0);
        lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, -8);
      }
      break;
    }
    default:
      break;
  }
}

void SimApp::on_rotate_focused(int delta) {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onRotate(delta);
      break;
    case Screen::Weather:
      weather_.onRotate(delta);
      break;
    case Screen::Sports:
      sports_.onRotate(delta);
      break;
    case Screen::Radar:
      radar_.onRotate(delta);
      break;
    default:
      break;
  }
}

void SimApp::on_tap_focused() {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Sports:
      sports_.onTap();
      break;
    case Screen::Weather:
      if (weather_.view().alertBadge) {
        if (weather_.alertDetailOpen()) {
          weather_.closeAlertDetail();
        } else {
          weather_.openAlertDetail();
        }
      }
      break;
    case Screen::Radar:
      if (radar_.blipCount() > 0) {
        if (radar_.hasSelection()) {
          radar_.clearSelection();
        } else {
          radar_.selectBlip(0);
        }
      } else {
        radar_.toggleMode();
      }
      break;
    case Screen::Timezones:
      // Tap cycles anchor for sim convenience (device uses row tap)
      timezones_.onTapRow((timezones_.anchorIndex() + 1) % desk_display::kTimezoneBoardRows);
      break;
    default:
      break;
  }
}

void SimApp::on_double_tap_focused() {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onDoubleTap();
      break;
    case Screen::Weather:
      weather_.snapToNow();
      break;
    case Screen::Radar:
      radar_.toggleMode();
      break;
    default:
      break;
  }
}

void SimApp::on_long_press_focused() {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onLongPress();
      break;
    case Screen::Radar:
      radar_.pinCenter();
      break;
    default:
      break;
  }
}

void SimApp::handle_input() {
  const KeyEvents keys = hal_take_keys();
  if (keys.quit) {
    return;
  }

  const bool any = keys.rotate_delta != 0 || keys.center_tap || keys.tap ||
                   keys.double_tap || keys.long_press;
  if (!any) {
    return;
  }

  const auto prev_mode = nav_.mode();
  const auto prev_screen = nav_.active_screen();

  if (keys.rotate_delta != 0) {
    if (nav_.mode() == desk_display::NavMode::Carousel) {
      nav_.on_rotate(static_cast<int8_t>(keys.rotate_delta));
    } else {
      on_rotate_focused(keys.rotate_delta);
      nav_.idle_reset();
    }
  }
  if (keys.center_tap) {
    if (nav_.mode() == desk_display::NavMode::Focused &&
        nav_.focused() == desk_display::Screen::Radar) {
      radar_.revertTempCenter();
    }
    nav_.on_center_tap();
  }
  if (keys.tap) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_tap_focused();
    }
    nav_.on_tap(0, 0);
  }
  if (keys.double_tap) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_double_tap_focused();
    }
    nav_.on_double_tap();
  }
  if (keys.long_press) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_long_press_focused();
    }
    nav_.on_long_press();
  }

  if (nav_.mode() != prev_mode || nav_.active_screen() != prev_screen) {
    rebuild_ui_for_active();
  } else {
    refresh_content();
  }
}

void SimApp::update(uint32_t elapsed_ms) {
  static uint32_t clock_accum = 0;
  clock_accum += elapsed_ms;
  if (clock_accum >= 1000) {
    clock_accum = 0;
    sync_clock_from_wall();
    if (nav_.active_screen() == desk_display::Screen::Clock ||
        nav_.active_screen() == desk_display::Screen::Timezones) {
      refresh_content();
    }
  }

  nav_.on_tick(elapsed_ms);
  if (nav_.active_screen() != last_screen_ || nav_.mode() != last_mode_) {
    rebuild_ui_for_active();
  }
}

void SimApp::shutdown() {}

}  // namespace sim
