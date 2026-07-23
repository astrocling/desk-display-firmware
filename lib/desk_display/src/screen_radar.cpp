#include "desk_display/screen_radar.hpp"

#include "desk_display/radar_format.hpp"

#include <cmath>
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

void ScreenRadar::bind(const AircraftList& list) {
  char selectedCallsign[kMaxCallsign]{};
  bool hadSelection = hasSelection_;
  if (hadSelection && selectedIndex_ < blips_.count) {
    copyCallsign(selectedCallsign, sizeof(selectedCallsign),
                 blips_.items[selectedIndex_].aircraft.callsign);
  } else {
    hadSelection = false;
  }

  source_ = list;
  ready_ = true;
  rebuildBlips();

  if (hadSelection) {
    bool reselected = false;
    for (std::size_t i = 0; i < blips_.count; ++i) {
      if (std::strcmp(blips_.items[i].aircraft.callsign, selectedCallsign) ==
          0) {
        hasSelection_ = true;
        selectedIndex_ = i;
        reselected = true;
        break;
      }
    }
    if (!reselected) {
      clearSelection();
    }
  } else {
    hasSelection_ = false;
    selectedIndex_ = 0;
  }
}

void ScreenRadar::onTick(uint32_t elapsedMs) {
  sweepAngleDeg_ += static_cast<float>(elapsedMs) * kRadarSweepDegPerSec / 1000.0f;
  sweepAngleDeg_ = std::fmod(sweepAngleDeg_, 360.0f);
  if (sweepAngleDeg_ < 0.0f) {
    sweepAngleDeg_ += 360.0f;
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
  AircraftList filtered{};
  filterAircraftByRange(source_, centerLat_, centerLon_, rangeMiles_, filtered);

  std::memset(&blips_, 0, sizeof(blips_));
  for (std::size_t i = 0; i < filtered.count; ++i) {
    RadarBlip& b = blips_.items[blips_.count];
    b.aircraft = filtered.items[i];
    aircraftOffsetMiles(centerLat_, centerLon_, b.aircraft.lat, b.aircraft.lon,
                        b.offsetXMi, b.offsetYMi);
    ++blips_.count;
  }

  if (hasSelection_ && selectedIndex_ >= blips_.count) {
    clearSelection();
  }
}

void ScreenRadar::applyRange(float rangeMiles) {
  const float clamped = clampRadarRangeMiles(rangeMiles);
  if (clamped == rangeMiles_) {
    return;
  }
  rangeMiles_ = clamped;
  if (ready_) {
    rebuildBlips();
  }
}

void ScreenRadar::setActiveCenter(double lat, double lon, bool temp) {
  centerLat_ = lat;
  centerLon_ = lon;
  isTempCenter_ = temp;
  clearSelection();
  if (ready_) {
    rebuildBlips();
  }
}

}  // namespace desk_display
