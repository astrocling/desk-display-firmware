#pragma once

#include <cstddef>

namespace desk_display {

constexpr std::size_t kMaxAircraft = 64;
constexpr std::size_t kMaxCallsign = 16;

struct Aircraft {
  char callsign[kMaxCallsign];
  char type[8];          // ICAO type designator from `t` (e.g. B738)
  char registration[kMaxCallsign];  // from `r`
  char squawk[8];        // Mode A code from `squawk`
  float altFt;     // barometric altitude when available; NaN if missing
  float speedKt;   // ground speed when available; NaN if missing
  float trackDeg;  // track heading when available; NaN if missing
  float baroRateFpm;  // vertical rate when available; NaN if missing
  double lat;
  double lon;
  bool hasPosition;
  bool hasAlt;
  bool hasSpeed;
  bool hasTrack;
  bool hasBaroRate;
};

struct AircraftList {
  Aircraft items[kMaxAircraft];
  std::size_t count;
};

/** Parse adsb.lol v2 response (`ac` array). */
bool parseAdsb(const char* json, AircraftList& out);

/**
 * Copy aircraft within `rangeMiles` of (centerLat, centerLon) into `out`.
 * Aircraft without position are dropped. Returns count written.
 */
std::size_t filterAircraftByRange(const AircraftList& in, double centerLat,
                                  double centerLon, float rangeMiles,
                                  AircraftList& out);

}  // namespace desk_display
