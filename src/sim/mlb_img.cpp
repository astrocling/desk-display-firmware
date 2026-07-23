#include "mlb_img.hpp"

#include "assets/mlb/mlb_logos_assets.h"

#include <cctype>
#include <cstring>

namespace {

bool equalsIgnoreCase(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    if (std::toupper(static_cast<unsigned char>(*a)) !=
        std::toupper(static_cast<unsigned char>(*b))) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

struct LogoEntry {
  const char* abbr;
  const lv_img_dsc_t* img;
};

const LogoEntry kLogos[] = {
    {"ARI", &mlb_logo_ari}, {"ATL", &mlb_logo_atl}, {"BAL", &mlb_logo_bal},
    {"BOS", &mlb_logo_bos}, {"CHC", &mlb_logo_chc}, {"CHW", &mlb_logo_chw},
    {"CIN", &mlb_logo_cin}, {"CLE", &mlb_logo_cle}, {"COL", &mlb_logo_col},
    {"DET", &mlb_logo_det}, {"HOU", &mlb_logo_hou}, {"KC", &mlb_logo_kc},
    {"LAA", &mlb_logo_laa}, {"LAD", &mlb_logo_lad}, {"MIA", &mlb_logo_mia},
    {"MIL", &mlb_logo_mil}, {"MIN", &mlb_logo_min}, {"NYM", &mlb_logo_nym},
    {"NYY", &mlb_logo_nyy}, {"OAK", &mlb_logo_oak}, {"PHI", &mlb_logo_phi},
    {"PIT", &mlb_logo_pit}, {"SD", &mlb_logo_sd},   {"SF", &mlb_logo_sf},
    {"SEA", &mlb_logo_sea}, {"STL", &mlb_logo_stl}, {"TB", &mlb_logo_tb},
    {"TEX", &mlb_logo_tex}, {"TOR", &mlb_logo_tor}, {"WSH", &mlb_logo_wsh},
};

constexpr std::size_t kLogoCount = sizeof(kLogos) / sizeof(kLogos[0]);

}  // namespace

const lv_img_dsc_t* mlbTeamLogoImg(const char* abbr) {
  if (abbr == nullptr || abbr[0] == '\0') {
    return nullptr;
  }
  for (std::size_t i = 0; i < kLogoCount; ++i) {
    if (equalsIgnoreCase(abbr, kLogos[i].abbr)) {
      return kLogos[i].img;
    }
  }
  return nullptr;
}
