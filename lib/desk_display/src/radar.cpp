#include "desk_display/radar.hpp"

#include <cmath>

namespace desk_display {
namespace {

constexpr double kEarthRadiusMi = 3958.7613;
constexpr double kDegToRad = 0.017453292519943295;

}  // namespace

float clampRadarRangeMiles(float rangeMiles) {
  if (rangeMiles < kRadarRangeMinMi) {
    return kRadarRangeMinMi;
  }
  if (rangeMiles > kRadarRangeMaxMi) {
    return kRadarRangeMaxMi;
  }
  return rangeMiles;
}

void aircraftOffsetMiles(double centerLat, double centerLon, double acLat,
                         double acLon, float& outXMi, float& outYMi) {
  const double latRad = centerLat * kDegToRad;
  const double dLat = (acLat - centerLat) * kDegToRad;
  const double dLon = (acLon - centerLon) * kDegToRad;

  outYMi = static_cast<float>(dLat * kEarthRadiusMi);
  outXMi = static_cast<float>(dLon * kEarthRadiusMi * std::cos(latRad));
}

float distanceMiles(double lat1, double lon1, double lat2, double lon2) {
  const double phi1 = lat1 * kDegToRad;
  const double phi2 = lat2 * kDegToRad;
  const double dPhi = (lat2 - lat1) * kDegToRad;
  const double dLambda = (lon2 - lon1) * kDegToRad;

  const double a = std::sin(dPhi / 2.0) * std::sin(dPhi / 2.0) +
                   std::cos(phi1) * std::cos(phi2) * std::sin(dLambda / 2.0) *
                       std::sin(dLambda / 2.0);
  const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return static_cast<float>(kEarthRadiusMi * c);
}

}  // namespace desk_display
