#include "desk_display/radar_format.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace desk_display {
namespace {

bool appendSegment(char* buf, std::size_t bufLen, std::size_t& len,
                   const char* segment) {
  if (!segment || segment[0] == '\0') {
    return true;
  }
  const int n = (len == 0)
                    ? std::snprintf(buf + len, bufLen - len, "%s", segment)
                    : std::snprintf(buf + len, bufLen - len, " %s", segment);
  if (n < 0 || static_cast<std::size_t>(len + n) >= bufLen) {
    return false;
  }
  len += static_cast<std::size_t>(n);
  return true;
}

}  // namespace

RadarTrend radarTrendFromRate(float baroRateFpm, bool hasBaroRate) {
  if (!hasBaroRate) {
    return RadarTrend::None;
  }
  if (baroRateFpm > kRadarBaroRateDeadbandFpm) {
    return RadarTrend::Climb;
  }
  if (baroRateFpm < -kRadarBaroRateDeadbandFpm) {
    return RadarTrend::Descend;
  }
  return RadarTrend::None;
}

bool formatRadarAltitude(char* buf, std::size_t bufLen, float altFt) {
  if (!buf || bufLen == 0) {
    return false;
  }
  const int hundreds = static_cast<int>(std::lroundf(altFt / 100.0f));
  const char prefix = (altFt >= kRadarFlightLevelMinFt) ? 'F' : 'A';
  const int n = std::snprintf(buf, bufLen, "%c%03d", prefix, hundreds);
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return false;
  }
  return true;
}

bool formatRadarAltitudeDense(char* buf, std::size_t bufLen, float altFt) {
  if (!buf || bufLen == 0) {
    return false;
  }
  const int hundreds = static_cast<int>(std::lroundf(altFt / 100.0f));
  const int n = std::snprintf(buf, bufLen, "%03d", hundreds);
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return false;
  }
  return true;
}

bool formatRadarSpeed(char* buf, std::size_t bufLen, float speedKt) {
  if (!buf || bufLen == 0) {
    return false;
  }
  const int knots = static_cast<int>(std::lroundf(speedKt));
  const int n = std::snprintf(buf, bufLen, "G%03d", knots);
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return false;
  }
  return true;
}

bool formatRadarSpeedDense(char* buf, std::size_t bufLen, float speedKt) {
  if (!buf || bufLen == 0) {
    return false;
  }
  const int knots = static_cast<int>(std::lroundf(speedKt));
  const int n = std::snprintf(buf, bufLen, "%03d", knots);
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return false;
  }
  return true;
}

bool formatRadarTagLine2(char* buf, std::size_t bufLen, const Aircraft& ac,
                         RadarTagStyle style) {
  if (!buf || bufLen == 0) {
    return false;
  }

  char altBuf[8];
  char speedBuf[8];
  bool wroteAny = false;
  std::size_t len = 0;
  buf[0] = '\0';

  const bool dense = style == RadarTagStyle::Dense;
  if (ac.hasAlt) {
    const bool ok = dense ? formatRadarAltitudeDense(altBuf, sizeof(altBuf), ac.altFt)
                          : formatRadarAltitude(altBuf, sizeof(altBuf), ac.altFt);
    if (ok) {
      if (!appendSegment(buf, bufLen, len, altBuf)) {
        return false;
      }
      wroteAny = true;
    }
  }

  switch (radarTrendFromRate(ac.baroRateFpm, ac.hasBaroRate)) {
    case RadarTrend::Climb:
      if (!appendSegment(buf, bufLen, len, "^")) {
        return false;
      }
      wroteAny = true;
      break;
    case RadarTrend::Descend:
      if (!appendSegment(buf, bufLen, len, "v")) {
        return false;
      }
      wroteAny = true;
      break;
    case RadarTrend::None:
      break;
  }

  if (ac.hasSpeed) {
    const bool ok = dense ? formatRadarSpeedDense(speedBuf, sizeof(speedBuf), ac.speedKt)
                          : formatRadarSpeed(speedBuf, sizeof(speedBuf), ac.speedKt);
    if (ok) {
      if (!appendSegment(buf, bufLen, len, speedBuf)) {
        return false;
      }
      wroteAny = true;
    }
  }

  return wroteAny;
}

bool formatRadarTagLine3(char* buf, std::size_t bufLen, const char* type,
                         const char* squawk) {
  if (!buf || bufLen == 0) {
    return false;
  }
  std::size_t len = 0;
  buf[0] = '\0';
  bool wroteAny = false;
  if (type && type[0] != '\0') {
    if (!appendSegment(buf, bufLen, len, type)) {
      return false;
    }
    wroteAny = true;
  }
  if (squawk && squawk[0] != '\0') {
    if (!appendSegment(buf, bufLen, len, squawk)) {
      return false;
    }
    wroteAny = true;
  }
  return wroteAny;
}

}  // namespace desk_display
