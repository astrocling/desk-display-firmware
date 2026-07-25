#pragma once

#include <cstddef>
#include <cstdint>

#include "desk_display/adsb.hpp"
#include "desk_display/aircraft_notable.hpp"

namespace desk_display {

constexpr float kRadarFlightLevelMinFt = 18000.0f;
constexpr float kRadarBaroRateDeadbandFpm = 100.0f;

enum class RadarTrend : uint8_t { None = 0, Climb = 1, Descend = 2 };

/** Full = F/A + G prefixes; Dense = numeric only (decluttered). */
enum class RadarTagStyle : uint8_t { Full = 0, Dense = 1 };

RadarTrend radarTrendFromRate(float baroRateFpm, bool hasBaroRate);

bool formatRadarAltitude(char* buf, std::size_t bufLen, float altFt);
bool formatRadarAltitudeDense(char* buf, std::size_t bufLen, float altFt);
bool formatRadarSpeed(char* buf, std::size_t bufLen, float speedKt);
bool formatRadarSpeedDense(char* buf, std::size_t bufLen, float speedKt);

/** Line 2: altitude + optional trend + speed. Omits missing segments. */
bool formatRadarTagLine2(char* buf, std::size_t bufLen, const Aircraft& ac,
                         RadarTagStyle style = RadarTagStyle::Full);

/** Line 3 (selected): type + squawk + optional notable reason. Omits missing. */
bool formatRadarTagLine3(char* buf, std::size_t bufLen, const char* type,
                         const char* squawk,
                         AircraftNotable notable = AircraftNotable::None);

/** Line 4 (full tag): arrival ICAO. Empty until routeset data exists. */
bool formatRadarTagLine4(char* buf, std::size_t bufLen, const char* arrivalIcao);

}  // namespace desk_display
