#include "desk_display/screen_radar.hpp"

#include "desk_display/radar_format.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace desk_display {
namespace {

void copyCallsign(char* dst, std::size_t dstLen, const char* src) {
  if (!dst || dstLen == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  std::snprintf(dst, dstLen, "%s", src);
}

bool callsignInSource(const AircraftList& source, const char* callsign) {
  if (!callsign || callsign[0] == '\0') {
    return false;
  }
  for (std::size_t i = 0; i < source.count; ++i) {
    if (std::strcmp(source.items[i].callsign, callsign) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

ScreenRadar::ScreenRadar() { reset(); }

void ScreenRadar::reset() {
  ready_ = false;
  mode_ = RadarMode::ClassicSweep;
  rangeMiles_ = kRadarDefaultRangeMi;

  homeLat_ = kRadarHomeLat;
  homeLon_ = kRadarHomeLon;
  permanentLat_ = homeLat_;
  permanentLon_ = homeLon_;
  centerLat_ = homeLat_;
  centerLon_ = homeLon_;
  isTempCenter_ = false;
  isPinned_ = false;

  std::memset(&source_, 0, sizeof(source_));
  std::memset(&blips_, 0, sizeof(blips_));
  hasSelection_ = false;
  selectedIndex_ = 0;
  sweepAngleDeg_ = 0.0f;
}

bool ScreenRadar::captureSelectionCallsign(char* dst,
                                           std::size_t dstLen) const {
  if (!dst || dstLen == 0) {
    return false;
  }
  if (hasSelection_ && selectedIndex_ < blips_.count) {
    copyCallsign(dst, dstLen, blips_.items[selectedIndex_].aircraft.callsign);
    return true;
  }
  dst[0] = '\0';
  return false;
}

void ScreenRadar::restoreSelectionByCallsign(const char* callsign) {
  if (!callsign || callsign[0] == '\0') {
    clearSelection();
    return;
  }
  for (std::size_t i = 0; i < blips_.count; ++i) {
    if (std::strcmp(blips_.items[i].aircraft.callsign, callsign) == 0) {
      hasSelection_ = true;
      selectedIndex_ = i;
      return;
    }
  }
  clearSelection();
}

float ScreenRadar::bearingDegFromOffset(float offsetXMi, float offsetYMi) {
  // atan2(east, north) → clockwise degrees from north.
  float deg = std::atan2(offsetXMi, offsetYMi) * 180.0f /
              3.14159265358979323846f;
  deg = std::fmod(deg, 360.0f);
  if (deg < 0.0f) {
    deg += 360.0f;
  }
  return deg;
}

void ScreenRadar::paintBlipFromAircraft(const Aircraft& ac) {
  if (!ac.hasPosition) {
    return;
  }
  float x = 0.0f;
  float y = 0.0f;
  aircraftOffsetMiles(centerLat_, centerLon_, ac.lat, ac.lon, x, y);
  const float dist = std::sqrt(x * x + y * y);
  if (dist > rangeMiles_ + 0.01f) {
    return;
  }

  for (std::size_t i = 0; i < blips_.count; ++i) {
    if (std::strcmp(blips_.items[i].aircraft.callsign, ac.callsign) == 0) {
      blips_.items[i].aircraft = ac;
      blips_.items[i].offsetXMi = x;
      blips_.items[i].offsetYMi = y;
      blips_.items[i].litAgeMs = 0;
      return;
    }
  }

  if (blips_.count >= kMaxAircraft) {
    return;
  }
  RadarBlip& b = blips_.items[blips_.count++];
  b.aircraft = ac;
  b.offsetXMi = x;
  b.offsetYMi = y;
  b.litAgeMs = 0;
}

void ScreenRadar::paintSweepAtAngle(float sweepDeg) {
  AircraftList filtered{};
  filterAircraftByRange(source_, centerLat_, centerLon_, rangeMiles_, filtered);

  for (std::size_t i = 0; i < filtered.count; ++i) {
    const Aircraft& ac = filtered.items[i];
    float x = 0.0f;
    float y = 0.0f;
    aircraftOffsetMiles(centerLat_, centerLon_, ac.lat, ac.lon, x, y);
    const float brg = bearingDegFromOffset(x, y);
    const float after = std::fmod(sweepDeg - brg + 360.0f, 360.0f);
    if (after < kRadarSweepGateDeg) {
      paintBlipFromAircraft(ac);
    }
  }

  // Drop displayed blips no longer in source when the sweep covers them.
  for (std::size_t i = 0; i < blips_.count;) {
    const RadarBlip& b = blips_.items[i];
    const float brg = bearingDegFromOffset(b.offsetXMi, b.offsetYMi);
    const float after = std::fmod(sweepDeg - brg + 360.0f, 360.0f);
    if (after < kRadarSweepGateDeg &&
        !callsignInSource(source_, b.aircraft.callsign)) {
      char selectedCallsign[kMaxCallsign]{};
      const bool hadSelection =
          captureSelectionCallsign(selectedCallsign, sizeof(selectedCallsign));
      for (std::size_t j = i + 1; j < blips_.count; ++j) {
        blips_.items[j - 1] = blips_.items[j];
      }
      --blips_.count;
      if (hadSelection) {
        restoreSelectionByCallsign(selectedCallsign);
      }
      continue;
    }
    ++i;
  }
}

void ScreenRadar::pruneDisplayedOutsideRange() {
  char selectedCallsign[kMaxCallsign]{};
  const bool hadSelection =
      captureSelectionCallsign(selectedCallsign, sizeof(selectedCallsign));

  std::size_t w = 0;
  for (std::size_t i = 0; i < blips_.count; ++i) {
    const float dist = std::sqrt(blips_.items[i].offsetXMi * blips_.items[i].offsetXMi +
                                 blips_.items[i].offsetYMi * blips_.items[i].offsetYMi);
    if (dist <= rangeMiles_ + 0.01f) {
      if (w != i) {
        blips_.items[w] = blips_.items[i];
      }
      ++w;
    }
  }
  blips_.count = w;

  if (hadSelection) {
    restoreSelectionByCallsign(selectedCallsign);
  }
}

void ScreenRadar::reprojectDisplayedOffsets() {
  for (std::size_t i = 0; i < blips_.count; ++i) {
    RadarBlip& b = blips_.items[i];
    aircraftOffsetMiles(centerLat_, centerLon_, b.aircraft.lat, b.aircraft.lon,
                        b.offsetXMi, b.offsetYMi);
  }
  pruneDisplayedOutsideRange();
}

void ScreenRadar::bind(const AircraftList& list) {
  char selectedCallsign[kMaxCallsign]{};
  const bool hadSelection =
      captureSelectionCallsign(selectedCallsign, sizeof(selectedCallsign));

  source_ = list;
  ready_ = true;

  if (mode_ == RadarMode::Detail) {
    rebuildBlips();
  }
  // ClassicSweep: leave displayed blips in place; onTick paints as the sweep
  // crosses each aircraft (matches DeskRad — no full-screen jump on poll).

  if (hadSelection) {
    if (!callsignInSource(source_, selectedCallsign)) {
      clearSelection();
    } else {
      restoreSelectionByCallsign(selectedCallsign);
    }
  }
}

void ScreenRadar::onTick(uint32_t elapsedMs) {
  if (elapsedMs == 0) {
    return;
  }

  for (std::size_t i = 0; i < blips_.count; ++i) {
    const uint32_t age = blips_.items[i].litAgeMs;
    const uint32_t next = age + elapsedMs;
    blips_.items[i].litAgeMs = next < age ? UINT32_MAX : next;
  }

  if (mode_ != RadarMode::ClassicSweep) {
    return;
  }

  // Step the sweep so a large elapsedMs still paints every gate (DeskRad
  // illuminates when (sweep - bearing) mod 360 < GATE).
  constexpr float kStepDeg = 2.0f;
  float remainingDeg =
      static_cast<float>(elapsedMs) * kRadarSweepDegPerSec / 1000.0f;
  while (remainingDeg > 0.0f) {
    const float step = remainingDeg < kStepDeg ? remainingDeg : kStepDeg;
    sweepAngleDeg_ += step;
    sweepAngleDeg_ = std::fmod(sweepAngleDeg_, 360.0f);
    if (sweepAngleDeg_ < 0.0f) {
      sweepAngleDeg_ += 360.0f;
    }
    paintSweepAtAngle(sweepAngleDeg_);
    remainingDeg -= step;
  }
}

void ScreenRadar::unbind() {
  ready_ = false;
  std::memset(&source_, 0, sizeof(source_));
  std::memset(&blips_, 0, sizeof(blips_));
  hasSelection_ = false;
  selectedIndex_ = 0;
}

void ScreenRadar::toggleMode() {
  mode_ = (mode_ == RadarMode::ClassicSweep) ? RadarMode::Detail
                                             : RadarMode::ClassicSweep;
  clearSelection();
  if (ready_ && mode_ == RadarMode::Detail) {
    rebuildBlips();
  }
}

void ScreenRadar::onRotate(int delta) {
  if (delta == 0) {
    return;
  }
  applyRange(rangeMiles_ + static_cast<float>(delta) * kRadarRangeStepMi);
}

const RadarBlip& ScreenRadar::blip(std::size_t index) const {
  static const RadarBlip kEmpty{};
  if (index >= blips_.count) {
    return kEmpty;
  }
  return blips_.items[index];
}

bool ScreenRadar::selectBlip(std::size_t index) {
  if (!ready_ || index >= blips_.count) {
    return false;
  }
  hasSelection_ = true;
  selectedIndex_ = index;
  return true;
}

void ScreenRadar::clearSelection() {
  hasSelection_ = false;
  selectedIndex_ = 0;
}

RadarDetailCard ScreenRadar::detailCard() const {
  RadarDetailCard card{};
  card.present = false;
  card.callsign[0] = '\0';
  card.altFt = 0.0f;
  card.speedKt = 0.0f;
  card.hasAlt = false;
  card.hasSpeed = false;
  card.tagLine2[0] = '\0';
  card.altLabel[0] = '\0';
  card.speedLabel[0] = '\0';

  if (!hasSelection_ || selectedIndex_ >= blips_.count) {
    return card;
  }

  const Aircraft& ac = blips_.items[selectedIndex_].aircraft;
  card.present = true;
  copyCallsign(card.callsign, sizeof(card.callsign), ac.callsign);
  card.hasAlt = ac.hasAlt;
  card.hasSpeed = ac.hasSpeed;
  card.altFt = ac.altFt;
  card.speedKt = ac.speedKt;

  if (ac.hasAlt) {
    formatRadarAltitude(card.altLabel, sizeof(card.altLabel), ac.altFt);
  }
  if (ac.hasSpeed) {
    formatRadarSpeed(card.speedLabel, sizeof(card.speedLabel), ac.speedKt);
  }
  formatRadarTagLine2(card.tagLine2, sizeof(card.tagLine2), ac);
  return card;
}

bool ScreenRadar::isHomeCenter() const {
  return !isTempCenter_ && centerLat_ == homeLat_ && centerLon_ == homeLon_;
}

void ScreenRadar::setTempCenter(double lat, double lon) {
  setActiveCenter(lat, lon, true);
}

void ScreenRadar::setTempCenter(const Airport& airport) {
  setTempCenter(airport.lat, airport.lon);
}

void ScreenRadar::pinCenter() {
  permanentLat_ = centerLat_;
  permanentLon_ = centerLon_;
  isPinned_ = true;
  isTempCenter_ = false;
}

void ScreenRadar::clearPin() {
  isPinned_ = false;
  permanentLat_ = homeLat_;
  permanentLon_ = homeLon_;
}

void ScreenRadar::revertTempCenter() {
  if (!isTempCenter_) {
    return;
  }
  setActiveCenter(permanentLat_, permanentLon_, false);
}

RadarView ScreenRadar::view() const {
  RadarView v{};
  v.ready = ready_;
  v.mode = mode_;
  v.rangeMiles = rangeMiles_;
  v.centerLat = centerLat_;
  v.centerLon = centerLon_;
  v.isTempCenter = isTempCenter_;
  v.isPinned = isPinned_;
  v.isHomeCenter = isHomeCenter();
  v.blips = blips_.items;
  v.blipCount = blips_.count;
  v.hasSelection = hasSelection_;
  v.selectedIndex = selectedIndex_;
  v.detail = detailCard();
  v.sweepAngleDeg = sweepAngleDeg_;
  return v;
}

void ScreenRadar::rebuildBlips() {
  char selectedCallsign[kMaxCallsign]{};
  const bool hadSelection =
      captureSelectionCallsign(selectedCallsign, sizeof(selectedCallsign));

  AircraftList filtered{};
  filterAircraftByRange(source_, centerLat_, centerLon_, rangeMiles_, filtered);

  std::memset(&blips_, 0, sizeof(blips_));
  for (std::size_t i = 0; i < filtered.count; ++i) {
    RadarBlip& b = blips_.items[blips_.count];
    b.aircraft = filtered.items[i];
    aircraftOffsetMiles(centerLat_, centerLon_, b.aircraft.lat, b.aircraft.lon,
                        b.offsetXMi, b.offsetYMi);
    b.litAgeMs = 0;
    ++blips_.count;
  }

  if (hadSelection) {
    restoreSelectionByCallsign(selectedCallsign);
  }
}

void ScreenRadar::applyRange(float rangeMiles) {
  const float clamped = clampRadarRangeMiles(rangeMiles);
  if (clamped == rangeMiles_) {
    return;
  }
  rangeMiles_ = clamped;
  if (!ready_) {
    return;
  }
  if (mode_ == RadarMode::Detail) {
    rebuildBlips();
  } else {
    reprojectDisplayedOffsets();
  }
}

void ScreenRadar::setActiveCenter(double lat, double lon, bool temp) {
  centerLat_ = lat;
  centerLon_ = lon;
  isTempCenter_ = temp;
  clearSelection();
  if (!ready_) {
    return;
  }
  if (mode_ == RadarMode::Detail) {
    rebuildBlips();
  } else {
    std::memset(&blips_, 0, sizeof(blips_));
  }
}

}  // namespace desk_display
