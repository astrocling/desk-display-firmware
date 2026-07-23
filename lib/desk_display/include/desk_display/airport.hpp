#pragma once

namespace desk_display {

struct Airport {
  double lat;
  double lon;
};

bool parseAirport(const char* json, Airport& out);

}  // namespace desk_display
