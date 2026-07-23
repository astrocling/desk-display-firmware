#pragma once

#include <cstddef>
#include <cstdint>

#include "desk_display/adsb.hpp"

namespace desk_display {

constexpr float kRadarFlightLevelMinFt = 18000.0f;
constexpr float kRadarBaroRateDeadbandFpm = 100.0f;

enum class RadarTrend : uint8_t { None = 0, Climb = 1, Descend = 2 };

RadarTrend radarTrendFromRate(float baroRateFpm, bool hasBaroRate);

bool formatRadarAltitude(char* buf, std::size_t bufLen, float altFt);
bool formatRadarSpeed(char* buf, std::size_t bufLen, float speedKt);
bool formatRadarTagLine2(char* buf, std::size_t bufLen, const Aircraft& ac);

}  // namespace desk_display
