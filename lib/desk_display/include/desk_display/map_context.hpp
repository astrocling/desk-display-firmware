#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_display {

enum class AirspaceClass : uint8_t { B, C, D };

struct MapAirport {
  char icao[8];
  char name[48];
  double lat;
  double lon;
  /** False = non-towered. Missing JSON field parses as true (legacy fixtures). */
  bool towered;
};

struct MapAirspaceRing {
  AirspaceClass cls;
  char id[24];
  float pointsLat[64];
  float pointsLon[64];
  uint8_t pointCount;
};

struct MapHighway {
  char id[16];
  char route[12];
  float pointsLat[80];
  float pointsLon[80];
  uint8_t pointCount;
};

struct MapContext {
  MapAirport airports[40];
  std::size_t airportCount;
  MapAirspaceRing rings[24];
  std::size_t ringCount;
  MapHighway highways[12];
  std::size_t highwayCount;
};

bool parseMapContext(const char* json, MapContext& out);

}  // namespace desk_display
