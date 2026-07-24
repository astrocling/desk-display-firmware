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
};

struct MapAirspaceRing {
  AirspaceClass cls;
  char id[24];
  float pointsLat[64];
  float pointsLon[64];
  uint8_t pointCount;
};

struct MapContext {
  MapAirport airports[40];
  std::size_t airportCount;
  MapAirspaceRing rings[16];
  std::size_t ringCount;
};

bool parseMapContext(const char* json, MapContext& out);

}  // namespace desk_display
