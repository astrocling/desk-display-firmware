#pragma once

namespace desk_display {

/** Curated point-of-interest for radar overlay (device config). */
struct RadarPoi {
  const char* name;
  double lat;
  double lon;
};

}  // namespace desk_display
