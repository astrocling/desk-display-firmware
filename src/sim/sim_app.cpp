#include "sim_app.hpp"

#include "sdl_hal.hpp"

#include "desk_display/adsb.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/radar_prefs.hpp"
#include "desk_display/radar_settings.hpp"
#include "desk_display/format_time.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/scores.hpp"
#include "desk_display/theme.hpp"
#include "desk_display/timezones.hpp"
#include "desk_display/weather.hpp"
#include "desk_display/weather_icons.hpp"
#include "mlb_img.hpp"
#include "sim_http.hpp"
#include "weather_img.hpp"

#include "../ui/radar_lvgl.hpp"

#if __has_include("config.h")
#include "config.h"
#endif

#include <cstdio>
#include <ctime>
#include <cstring>

// Reuse test fixture loader
#include "../../test/fixture_loader.hpp"

namespace sim {
namespace {

constexpr const char* kRadarPrefsPath = "radar_prefs.bin";

lv_color_t rgb(uint32_t c) {
  return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
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
  adsb_poll_.setHttpGet(&sim::simAdsbHttpGet, nullptr);
  map_ctx_poll_.setHttpGet(&sim::simMapContextHttpGet, nullptr);
  scores_poll_.setHttpGet(&sim::simScoresHttpGet, nullptr);

  root_ = lv_obj_create(lv_scr_act());
  lv_obj_set_size(root_, kDispW, kDispH);
  lv_obj_center(root_);
  lv_obj_set_style_radius(root_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(root_, true, 0);
  lv_obj_set_style_bg_color(root_, rgb(desk_display::theme::kBg), 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  carousel_root_ = lv_obj_create(root_);
  lv_obj_set_size(carousel_root_, kDispW, kDispH);
  lv_obj_center(carousel_root_);
  lv_obj_set_style_bg_opa(carousel_root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(carousel_root_, 0, 0);
  lv_obj_set_style_pad_all(carousel_root_, 0, 0);
  lv_obj_clear_flag(carousel_root_, LV_OBJ_FLAG_SCROLLABLE);
  carousel_ = desk_ui::carousel_lvgl_build(carousel_root_);
  desk_ui::carousel_lvgl_set_highlight(carousel_, nav_.highlighted());

  focused_host_ = lv_obj_create(root_);
  lv_obj_set_size(focused_host_, kDispW, kDispH);
  lv_obj_center(focused_host_);
  lv_obj_set_style_bg_opa(focused_host_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(focused_host_, 0, 0);
  lv_obj_set_style_pad_all(focused_host_, 0, 0);
  lv_obj_clear_flag(focused_host_, LV_OBJ_FLAG_SCROLLABLE);

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

  desk_display::MapContext mapCtx{};
  if (loadFixture("map_context_dayton.json", buf, sizeof(buf)) &&
      desk_display::parseMapContext(buf, mapCtx)) {
    radar_.bindMapContext(mapCtx);
  }

#if defined(RADAR_POI_COUNT)
  radar_.setPois(RADAR_POIS, static_cast<std::size_t>(RADAR_POI_COUNT));
#endif

  desk_display::RadarSettings prefs = desk_display::radarSettingsFactoryDefaults();
  desk_display::loadRadarSettingsFromFile(prefs, kRadarPrefsPath);
  radar_.setSettings(prefs);
  if (prefs.demoMode) {
    bind_demo_adsb_fixture();
  }

  // Carousel is the browse path to Timezones — no "→ Timezones" chrome on Clock.
  clock_.setTimezoneBoardHint(false);
  std::fprintf(stdout, "Fixtures loaded (weather/tz/scores/map-context as available).\n");
}

void SimApp::persist_radar_prefs() {
  desk_display::saveRadarSettingsToFile(radar_.settings(), kRadarPrefsPath);
}

void SimApp::bind_demo_adsb_fixture() {
  char buf[256 * 1024];
  desk_display::AircraftList list{};
  if (loadFixture("adsb_sample.json", buf, sizeof(buf)) &&
      desk_display::parseAdsb(buf, list)) {
    radar_.bind(list);
  }
}

void SimApp::sync_clock_from_wall() {
  const std::time_t now = std::time(nullptr);
  clock_.setUnixUtc(static_cast<std::int64_t>(now));
  timezones_.setLiveUnix(static_cast<std::int64_t>(now));
}

void SimApp::rebuild_ui_for_active() {
  using desk_display::NavMode;
  const bool carousel_mode = nav_.mode() == NavMode::Carousel;

  if (carousel_mode) {
    lv_obj_clear_flag(carousel_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(focused_host_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(carousel_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(focused_host_, LV_OBJ_FLAG_HIDDEN);
  }

  if (body_) {
    lv_obj_del(body_);
    body_ = nullptr;
  }
  desk_ui::radar_lvgl_invalidate();

  lv_obj_t* const body_parent = carousel_mode ? carousel_.preview_host : focused_host_;
  body_ = lv_obj_create(body_parent);
  lv_obj_set_size(body_, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body_, 0, 0);
  lv_obj_set_style_pad_all(body_, 0, 0);
  lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);

  if (carousel_mode) {
    desk_ui::carousel_lvgl_set_highlight(carousel_, nav_.highlighted());
  }

  last_screen_ = nav_.active_screen();
  last_mode_ = nav_.mode();
  refresh_content();
}

void SimApp::settle_focused_screens() {
  weather_.onIdleSettle();
  radar_.onIdleSettle();
  sports_.onIdleSettle();
  timezones_.onIdleSettle();
  // Clock: no ephemeral state.
}

void SimApp::refresh_content() {
  using desk_display::Screen;

  if (!body_) {
    return;
  }

  const Screen scr = nav_.active_screen();

  // Radar: update sweep/traffic in place to avoid a full teardown flash
  // (especially noticeable when the beam wraps through north).
  if (scr == Screen::Radar &&
      desk_ui::radar_lvgl_animate_classic(body_, radar_.view())) {
    return;
  }

  lv_obj_clean(body_);
  desk_ui::radar_lvgl_invalidate();

  // Carousel and Focused share the same default screen views. Mode only
  // changes input: Carousel rotate cycles screens; Focused rotate is in-app.
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
      // Approximate body_ height; center the block in that area
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

      if (v.card == desk_display::SportsCard::Mlb) {
        const bool nextGameCard = !v.mlb.live && (v.mlb.hasMatchup || v.mlb.hasNextGame);

        if (v.mlb.live) {
          lv_obj_t* col = lv_obj_create(body_);
          lv_obj_set_size(col, 280, 260);
          lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
          lv_obj_set_style_border_width(col, 0, 0);
          lv_obj_set_style_pad_all(col, 0, 0);
          lv_obj_set_style_pad_row(col, 6, 0);
          lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
          lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                LV_FLEX_ALIGN_CENTER);
          lv_obj_center(col);

          lv_obj_t* title = lv_label_create(col);
          lv_label_set_text(title, "LIVE");
          lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
          lv_obj_set_style_text_color(title, rgb(desk_display::theme::kAccent), 0);

          auto add_line = [&](const char* text, bool dim, const lv_font_t* font) {
            if (!text || text[0] == '\0') {
              return;
            }
            lv_obj_t* lab = lv_label_create(col);
            lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lab, 280);
            lv_label_set_text(lab, text);
            if (font) {
              lv_obj_set_style_text_font(lab, font, 0);
            }
            lv_obj_set_style_text_color(
                lab, rgb(dim ? desk_display::theme::kDim : 0xFFFFFF), 0);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
          };

          if (v.mlb.showLiveScorebug) {
            // Side-by-side: team logo + runs | opponent runs + logo
            lv_obj_t* matchup = lv_obj_create(col);
            lv_obj_set_size(matchup, 280, 56);
            lv_obj_set_style_bg_opa(matchup, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(matchup, 0, 0);
            lv_obj_set_style_pad_all(matchup, 0, 0);
            lv_obj_set_style_pad_column(matchup, 10, 0);
            lv_obj_clear_flag(matchup, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(matchup, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(matchup, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);

            auto add_logo = [&](const char* abbr) {
              const lv_img_dsc_t* logo = mlbTeamLogoImg(abbr);
              if (logo) {
                lv_obj_t* img = lv_img_create(matchup);
                lv_img_set_src(img, logo);
                lv_img_set_zoom(img, 200);  // ~44px from 56px
              } else {
                lv_obj_t* lab = lv_label_create(matchup);
                lv_label_set_text(lab, (abbr && abbr[0]) ? abbr : "?");
                lv_obj_set_style_text_font(lab, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(lab, rgb(0xFFFFFF), 0);
              }
            };

            auto add_runs = [&](int runs) {
              char runsBuf[8];
              std::snprintf(runsBuf, sizeof(runsBuf), "%d", runs);
              lv_obj_t* runsLab = lv_label_create(matchup);
              lv_label_set_text(runsLab, runsBuf);
              lv_obj_set_style_text_font(runsLab, &lv_font_montserrat_28, 0);
              lv_obj_set_style_text_color(runsLab, rgb(0xFFFFFF), 0);
            };

            add_logo(v.mlb.teamAbbr);
            add_runs(v.mlb.teamRuns);

            lv_obj_t* sep = lv_label_create(matchup);
            lv_label_set_text(sep, "-");
            lv_obj_set_style_text_font(sep, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(sep, rgb(desk_display::theme::kDim), 0);

            add_runs(v.mlb.opponentRuns);
            add_logo(v.mlb.opponentAbbr);

            add_line(v.mlb.hasInning ? v.mlb.inning : "", true, nullptr);
            add_line(v.mlb.countLine, true, &lv_font_montserrat_14);

            // Classic 4-diamond: 2nd top, 3rd left, 1st right, home bottom.
            // Empty bases use a dim fill so all four diamonds stay visible.
            lv_obj_t* diamond = lv_obj_create(col);
            lv_obj_set_size(diamond, 48, 48);
            lv_obj_set_style_bg_opa(diamond, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(diamond, 0, 0);
            lv_obj_set_style_pad_all(diamond, 0, 0);
            lv_obj_clear_flag(diamond, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(diamond, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

            auto add_base = [&](lv_coord_t x, lv_coord_t y, bool occupied) {
              constexpr lv_coord_t kSize = 12;
              lv_obj_t* base = lv_obj_create(diamond);
              lv_obj_remove_style_all(base);
              lv_obj_set_size(base, kSize, kSize);
              lv_obj_set_pos(base, x, y);
              lv_obj_set_style_radius(base, 0, 0);
              lv_obj_set_style_border_width(base, 0, 0);
              lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);
              lv_obj_set_style_bg_color(
                  base, rgb(occupied ? 0xFFFFFF : desk_display::theme::kDim), 0);
              lv_obj_set_style_transform_pivot_x(base, kSize / 2, 0);
              lv_obj_set_style_transform_pivot_y(base, kSize / 2, 0);
              lv_obj_set_style_transform_angle(base, 450, 0);  // 45°
              lv_obj_clear_flag(base, LV_OBJ_FLAG_SCROLLABLE);
            };

            add_base(18, 4, v.mlb.onSecond);   // 2nd
            add_base(4, 18, v.mlb.onThird);    // 3rd
            add_base(32, 18, v.mlb.onFirst);   // 1st
            add_base(18, 32, false);           // home (always empty)

            add_line(v.mlb.batterPitcherLine, true, &lv_font_montserrat_12);
          } else {
            add_line(v.mlb.primary, false, &lv_font_montserrat_28);
            add_line(v.mlb.secondary, true, nullptr);
          }
        } else {
          // Vertically centered card: title + (logo hero or matchup text) +
          // whenEt/record/standing, all in one flex column (no top y-stack).
          lv_obj_t* col = lv_obj_create(body_);
          lv_obj_set_size(col, 280, 220);
          lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
          lv_obj_set_style_border_width(col, 0, 0);
          lv_obj_set_style_pad_all(col, 0, 0);
          lv_obj_set_style_pad_row(col, 6, 0);
          lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
          lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                LV_FLEX_ALIGN_CENTER);
          lv_obj_center(col);

          lv_obj_t* title = lv_label_create(col);
          lv_label_set_text(title, nextGameCard ? "Next Game" : "MLB");
          lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
          lv_obj_set_style_text_color(title, rgb(desk_display::theme::kAccent), 0);

          auto add_line = [&](const char* text, bool dim, const lv_font_t* font) {
            if (!text || text[0] == '\0') {
              return;
            }
            lv_obj_t* lab = lv_label_create(col);
            lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lab, 280);
            lv_label_set_text(lab, text);
            if (font) {
              lv_obj_set_style_text_font(lab, font, 0);
            }
            lv_obj_set_style_text_color(
                lab, rgb(dim ? desk_display::theme::kDim : 0xFFFFFF), 0);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
          };

          if (v.mlb.showLogoHero) {
            lv_obj_t* hero = lv_obj_create(col);
            lv_obj_set_size(hero, 280, 60);
            lv_obj_set_style_bg_opa(hero, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(hero, 0, 0);
            lv_obj_set_style_pad_all(hero, 0, 0);
            lv_obj_set_style_pad_column(hero, 10, 0);
            lv_obj_clear_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(hero, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(hero, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);

            auto add_team_slot = [&](const char* abbr) {
              const lv_img_dsc_t* logo = mlbTeamLogoImg(abbr);
              if (logo) {
                lv_obj_t* img = lv_img_create(hero);
                lv_img_set_src(img, logo);
              } else {
                lv_obj_t* lab = lv_label_create(hero);
                lv_label_set_text(lab, (abbr && abbr[0]) ? abbr : "?");
                lv_obj_set_style_text_font(lab, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(lab, rgb(0xFFFFFF), 0);
              }
            };

            add_team_slot(v.mlb.teamAbbr);

            lv_obj_t* conn = lv_label_create(hero);
            lv_label_set_text(conn, v.mlb.hasConnector ? v.mlb.connector : "@");
            lv_obj_set_style_text_font(conn, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(conn, rgb(desk_display::theme::kDim), 0);

            add_team_slot(v.mlb.opponentAbbr);
          } else {
            add_line(v.mlb.hasMatchup ? v.mlb.matchup : v.mlb.primary, false,
                     &lv_font_montserrat_16);
          }

          add_line(v.mlb.hasWhenEt ? v.mlb.whenEt : v.mlb.secondary, true, nullptr);
          add_line(v.mlb.hasRecord ? v.mlb.record : "", false, nullptr);
          add_line(v.mlb.hasStandingLine ? v.mlb.standingLine : "", true, nullptr);
        }
      } else {
        lv_obj_t* title = lv_label_create(body_);
        lv_label_set_text(title, "Flagstand");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(title, rgb(desk_display::theme::kAccent), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t* primary = lv_label_create(body_);
        lv_label_set_long_mode(primary, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(primary, 280);
        lv_label_set_text(primary, v.flagstand.lastResultSummary);
        lv_obj_set_style_text_color(primary, rgb(0xFFFFFF), 0);
        lv_obj_align(primary, LV_ALIGN_CENTER, 0, -10);

        lv_obj_t* secondary = lv_label_create(body_);
        lv_label_set_long_mode(secondary, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(secondary, 280);
        lv_label_set_text(secondary, v.flagstand.nextRaceSummary);
        lv_obj_set_style_text_color(secondary, rgb(desk_display::theme::kDim), 0);
        lv_obj_align(secondary, LV_ALIGN_CENTER, 0, 40);
      }

      if (v.detail) {
        lv_obj_t* d = lv_label_create(body_);
        lv_label_set_text(d, "(detail)");
        lv_obj_set_style_text_color(d, rgb(desk_display::theme::kAlert), 0);
        lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -4);
      }
      break;
    }
    case Screen::Radar: {
      desk_ui::radar_lvgl_build(body_, radar_.view());
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

void SimApp::on_tap_focused(int16_t x, int16_t y) {
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
    case Screen::Radar: {
      const auto rv = radar_.view();
      std::size_t nearest = 0;
      // Aircraft wins when both overlap; try blip first, then static marks.
      if (desk_ui::radar_lvgl_hit_blip(body_, rv, x, y, &nearest)) {
        if (radar_.hasSelection() && radar_.selectedIndex() == nearest) {
          radar_.clearSelection();
        } else {
          radar_.selectBlip(nearest);
        }
      } else if (desk_ui::radar_lvgl_hit_static(body_, rv, x, y, &nearest)) {
        if (rv.hasStaticSelection && rv.selectedStaticIndex == nearest) {
          radar_.clearStaticSelection();
        } else {
          radar_.selectStaticMark(nearest);
        }
      } else {
        radar_.clearSelection();
        radar_.clearStaticSelection();
      }
      break;
    }
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
      radar_.clearSelection();
      radar_.clearStaticSelection();
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
      radar_.openSettings();
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

  const bool radar_settings_ui =
      nav_.mode() == desk_display::NavMode::Focused &&
      nav_.focused() == desk_display::Screen::Radar && radar_.settingsOpen();

  if (keys.rotate_delta != 0 && !radar_settings_ui) {
    if (nav_.mode() == desk_display::NavMode::Carousel) {
      nav_.on_rotate(static_cast<int8_t>(keys.rotate_delta));
    } else {
      on_rotate_focused(keys.rotate_delta);
      nav_.idle_reset();
    }
  }
  if (keys.center_tap) {
    if (radar_settings_ui) {
      radar_.closeSettings();
    } else {
      if (nav_.mode() == desk_display::NavMode::Focused &&
          nav_.focused() == desk_display::Screen::Radar) {
        radar_.revertTempCenter();
      }
      nav_.on_center_tap();
    }
  }
  if (keys.tap) {
    if (radar_settings_ui) {
      const bool prev_demo = radar_.settings().demoMode;
      if (desk_ui::radar_lvgl_settings_hit(keys.tap_x, keys.tap_y, radar_)) {
        persist_radar_prefs();
        if (radar_.settings().demoMode && !prev_demo) {
          bind_demo_adsb_fixture();
        }
      }
    } else {
      if (nav_.mode() == desk_display::NavMode::Focused) {
        on_tap_focused(keys.tap_x, keys.tap_y);
      }
      nav_.on_tap(keys.tap_x, keys.tap_y);
    }
  }
  if (keys.double_tap && !radar_settings_ui) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_double_tap_focused();
    }
    nav_.on_double_tap();
  }
  if (keys.long_press && !radar_settings_ui) {
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
  using desk_display::AircraftList;
  using desk_display::Screen;

  static uint32_t clock_accum = 0;
  clock_accum += elapsed_ms;
  if (clock_accum >= 1000) {
    clock_accum = 0;
    sync_clock_from_wall();
    if (nav_.active_screen() == Screen::Clock ||
        nav_.active_screen() == Screen::Timezones) {
      refresh_content();
    }
  }

  const auto idle = nav_.on_tick(elapsed_ms);
  if (idle == desk_display::IdleEvent::SettleFocused) {
    settle_focused_screens();
  }
  // HomeToClock changes mode + active screen, which the checks below already
  // catch and route through rebuild_ui_for_active().

  // Poll adsb.lol only while Radar is the active screen; sweep animation
  // also only advances while Radar is visible.
  const bool radar_active = nav_.active_screen() == Screen::Radar;
  const bool sports_active = nav_.active_screen() == Screen::Sports;
  const bool demo_mode = radar_.settings().demoMode;
  adsb_poll_.setActive(radar_active && !demo_mode);
  map_ctx_poll_.setActive(radar_active);
  scores_poll_.setActive(sports_active);
  if (radar_active) {
    radar_.onTick(elapsed_ms);
    const double lat = radar_.centerLat();
    const double lon = radar_.centerLon();
    const float range = radar_.rangeMiles();
    adsb_poll_.setCenter(lat, lon, range);
    map_ctx_poll_.setCenter(lat, lon, range);
  }
  adsb_poll_.onTick(elapsed_ms);
  map_ctx_poll_.onTick(elapsed_ms);
  scores_poll_.onTick(elapsed_ms);

  AircraftList fresh{};
  const bool got_fresh = adsb_poll_.takeAircraft(fresh);
  if (got_fresh) {
    radar_.bind(fresh);
  }

  desk_display::MapContext mapCtx{};
  const bool got_map_ctx = map_ctx_poll_.takeContext(mapCtx);
  if (got_map_ctx) {
    radar_.bindMapContext(mapCtx);
  }

  desk_display::Scores freshScores{};
  const bool got_scores = scores_poll_.takeScores(freshScores);
  if (got_scores) {
    sports_.bind(freshScores);
  }

  if (nav_.active_screen() != last_screen_ || nav_.mode() != last_mode_) {
    rebuild_ui_for_active();
  } else if (idle == desk_display::IdleEvent::SettleFocused || got_fresh || got_map_ctx ||
             got_scores || radar_active) {
    // Refresh on settle (so cleared overlays render), when live scores land,
    // and every tick while Radar is up so the sweep keeps moving.
    refresh_content();
  }
}

void SimApp::shutdown() {}

}  // namespace sim
