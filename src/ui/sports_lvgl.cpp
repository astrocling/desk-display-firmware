#include "sports_lvgl.hpp"

#include "desk_display/theme.hpp"
#include "mlb_img.hpp"

#include <cstdio>

namespace desk_ui {
namespace {

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

void build_mlb_live(lv_obj_t* parent, const desk_display::SportsMlbView& mlb) {
  lv_obj_t* col = lv_obj_create(parent);
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

  if (mlb.showLiveScorebug) {
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

    add_logo(mlb.teamAbbr);
    add_runs(mlb.teamRuns);

    lv_obj_t* sep = lv_label_create(matchup);
    lv_label_set_text(sep, "-");
    lv_obj_set_style_text_font(sep, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sep, rgb(desk_display::theme::kDim), 0);

    add_runs(mlb.opponentRuns);
    add_logo(mlb.opponentAbbr);

    add_line(mlb.hasInning ? mlb.inning : "", true, nullptr);
    add_line(mlb.countLine, true, &lv_font_montserrat_14);

    // Classic 4-diamond: 2nd top, 3rd left, 1st right, home bottom.
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

    add_base(18, 4, mlb.onSecond);   // 2nd
    add_base(4, 18, mlb.onThird);    // 3rd
    add_base(32, 18, mlb.onFirst);   // 1st
    add_base(18, 32, false);         // home (always empty)

    add_line(mlb.batterPitcherLine, true, &lv_font_montserrat_12);
  } else {
    add_line(mlb.primary, false, &lv_font_montserrat_28);
    add_line(mlb.secondary, true, nullptr);
  }
}

void build_mlb_next(lv_obj_t* parent, const desk_display::SportsMlbView& mlb) {
  const bool nextGameCard = !mlb.live && (mlb.hasMatchup || mlb.hasNextGame);

  lv_obj_t* col = lv_obj_create(parent);
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

  if (mlb.showLogoHero) {
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

    add_team_slot(mlb.teamAbbr);

    lv_obj_t* conn = lv_label_create(hero);
    lv_label_set_text(conn, mlb.hasConnector ? mlb.connector : "@");
    lv_obj_set_style_text_font(conn, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(conn, rgb(desk_display::theme::kDim), 0);

    add_team_slot(mlb.opponentAbbr);
  } else {
    add_line(mlb.hasMatchup ? mlb.matchup : mlb.primary, false,
             &lv_font_montserrat_16);
  }

  add_line(mlb.hasWhenEt ? mlb.whenEt : mlb.secondary, true, nullptr);
  add_line(mlb.hasRecord ? mlb.record : "", false, nullptr);
  add_line(mlb.hasStandingLine ? mlb.standingLine : "", true, nullptr);
}

void build_flagstand(lv_obj_t* parent,
                     const desk_display::SportsFlagstandView& flagstand) {
  lv_obj_t* title = lv_label_create(parent);
  lv_label_set_text(title, "Flagstand");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, rgb(desk_display::theme::kAccent), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t* primary = lv_label_create(parent);
  lv_label_set_long_mode(primary, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(primary, 280);
  lv_label_set_text(primary, flagstand.lastResultSummary);
  lv_obj_set_style_text_color(primary, rgb(0xFFFFFF), 0);
  lv_obj_align(primary, LV_ALIGN_CENTER, 0, -10);

  lv_obj_t* secondary = lv_label_create(parent);
  lv_label_set_long_mode(secondary, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(secondary, 280);
  lv_label_set_text(secondary, flagstand.nextRaceSummary);
  lv_obj_set_style_text_color(secondary, rgb(desk_display::theme::kDim), 0);
  lv_obj_align(secondary, LV_ALIGN_CENTER, 0, 40);
}

}  // namespace

void sports_lvgl_build(lv_obj_t* parent, const desk_display::SportsView& v) {
  if (!v.ready) {
    lv_obj_t* lab = lv_label_create(parent);
    lv_label_set_text(lab, "Scores not ready");
    lv_obj_center(lab);
    return;
  }

  if (v.card == desk_display::SportsCard::Mlb) {
    if (v.mlb.live) {
      build_mlb_live(parent, v.mlb);
    } else {
      build_mlb_next(parent, v.mlb);
    }
  } else {
    build_flagstand(parent, v.flagstand);
  }

  if (v.detail) {
    lv_obj_t* d = lv_label_create(parent);
    lv_label_set_text(d, "(detail)");
    lv_obj_set_style_text_color(d, rgb(desk_display::theme::kAlert), 0);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -4);
  }
}

}  // namespace desk_ui
