#pragma once

namespace desk_display {

constexpr float kRadarRangeMinMi = 5.0f;
constexpr float kRadarRangeMaxMi = 50.0f;

/** Clamp radar range to 5–50 miles. */
float clampRadarRangeMiles(float rangeMiles);

/** Convert statute miles to nautical miles. */
float statuteMilesToNauticalMiles(float statuteMi);

/** Clamp ADS-B query radius to [1, 250] nautical miles. */
float clampAdsbQueryRadiusNm(float nm);

/**
 * Equirectangular relative offset of aircraft from center, in miles.
 * +x = east, +y = north.
 */
void aircraftOffsetMiles(double centerLat, double centerLon, double acLat,
                         double acLon, float& outXMi, float& outYMi);

/** Great-circle distance in miles (haversine). */
float distanceMiles(double lat1, double lon1, double lat2, double lon2);

}  // namespace desk_display
