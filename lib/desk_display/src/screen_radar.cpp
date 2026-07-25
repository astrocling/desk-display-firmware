#include "desk_display/screen_radar.hpp"

#include "desk_display/aircraft_notable.hpp"
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

void sortAirportCandidatesByDistance(std::size_t* indices, float* distances,
                                     std::size_t count) {
  for (std::size_t i = 1; i < count; ++i) {
    const std::size_t idx = indices[i];
    const float dist = distances[i];
    std::size_t j = i;
    while (j > 0 && distances[j - 1] > dist) {
      indices[j] = indices[j - 1];
      distances[j] = distances[j - 1];
      --j;
    }
    indices[j] = idx;
    distances[j] = dist;
  }
}

struct RingCandidate {
  std::size_t sourceIndex;
  float centroidDistMi;
  AirspaceClass cls;
};

bool ringIntersectsRange(const MapAirspaceRing& ring, double centerLat,
                         double centerLon, float rangeMi) {
  float cx = 0.0f;
  float cy = 0.0f;
  for (uint8_t i = 0; i < ring.pointCount; ++i) {
    aircraftOffsetMiles(centerLat, centerLon, ring.pointsLat[i],
                        ring.pointsLon[i], cx, cy);
    const float dist = std::sqrt(cx * cx + cy * cy);
    if (dist <= rangeMi + 0.01f) {
      return true;
    }
  }

  double sumLat = 0.0;
  double sumLon = 0.0;
  for (uint8_t i = 0; i < ring.pointCount; ++i) {
    sumLat += ring.pointsLat[i];
    sumLon += ring.pointsLon[i];
  }
  const double centroidLat = sumLat / static_cast<double>(ring.pointCount);
  const double centroidLon = sumLon / static_cast<double>(ring.pointCount);
  return distanceMiles(centerLat, centerLon, centroidLat, centroidLon) <=
         rangeMi + 0.01f;
}

void trimRingCandidates(RingCandidate* cands, std::size_t& count,
                        std::size_t maxCount) {
  while (count > maxCount) {
    std::size_t dropIdx = count;
    for (std::size_t i = 0; i < count; ++i) {
      if (cands[i].cls != AirspaceClass::D) {
        continue;
      }
      if (dropIdx == count ||
          cands[i].centroidDistMi > cands[dropIdx].centroidDistMi) {
        dropIdx = i;
      }
    }
    if (dropIdx == count) {
      for (std::size_t i = 0; i < count; ++i) {
        if (dropIdx == count ||
            cands[i].centroidDistMi > cands[dropIdx].centroidDistMi) {
          dropIdx = i;
        }
      }
    }
    for (std::size_t j = dropIdx + 1; j < count; ++j) {
      cands[j - 1] = cands[j];
    }
    --count;
  }
}

void normalizeRadarSettings(RadarSettings& s) {
  const auto declutter = static_cast<uint8_t>(s.declutter);
  if (declutter > static_cast<uint8_t>(RadarDeclutterMode::TargetTag)) {
    s.declutter = RadarDeclutterMode::TargetTag;
  }
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

  interestingRegs_ = kRadarInterestingRegsDefault;
  interestingRegCount_ = kRadarInterestingRegCount;

  std::memset(&source_, 0, sizeof(source_));
  std::memset(&blips_, 0, sizeof(blips_));
  hasSelection_ = false;
  selectedIndex_ = 0;

  std::memset(&mapContext_, 0, sizeof(mapContext_));
  hasMapContext_ = false;
  poiCount_ = 0;
  staticMarkCount_ = 0;
  airspaceRingCount_ = 0;
  highwayCount_ = 0;
  hasStaticSelection_ = false;
  selectedStaticIndex_ = 0;
  sweepAngleDeg_ = 0.0f;
  settings_ = radarSettingsFactoryDefaults();
  settingsOpen_ = false;
}

void ScreenRadar::setInterestingRegs(const char* const* regs, std::size_t count) {
  if (!regs) {
    interestingRegs_ = kRadarInterestingRegsDefault;
    interestingRegCount_ = kRadarInterestingRegCount;
    return;
  }
  interestingRegs_ = regs;
  interestingRegCount_ = count;
}

AircraftNotable ScreenRadar::classify(const Aircraft& ac) const {
  return classifyAircraftNotable(ac, interestingRegs_, interestingRegCount_);
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
      blips_.items[i].notable = classify(ac);
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
  b.notable = classify(ac);
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
  // Leave displayed blips in place; onTick paints as the sweep crosses each
  // aircraft (matches DeskRad — no full-screen jump on poll).

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

  // Step the sweep so a large elapsedMs still paints every gate (DeskRad
  // illuminates when (sweep - bearing) mod 360 < GATE). Selection does not
  // pause the beam.
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

void ScreenRadar::setMode(RadarMode mode) {
  mode_ = mode;
}

void ScreenRadar::toggleMode() {
  clearSelection();
  setMode(mode_ == RadarMode::ClassicSweep ? RadarMode::Detail
                                           : RadarMode::ClassicSweep);
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
  clearStaticSelection();
  hasSelection_ = true;
  selectedIndex_ = index;
  return true;
}

void ScreenRadar::clearSelection() {
  hasSelection_ = false;
  selectedIndex_ = 0;
}

void ScreenRadar::bindMapContext(const MapContext& ctx) {
  mapContext_ = ctx;
  hasMapContext_ = true;
  reprojectOverlays();
}

void ScreenRadar::setPois(const RadarPoi* pois, std::size_t count) {
  poiCount_ = 0;
  if (!pois || count == 0) {
    reprojectOverlays();
    return;
  }
  const std::size_t n = count > kMaxProjectedPois ? kMaxProjectedPois : count;
  for (std::size_t i = 0; i < n; ++i) {
    pois_[i].name = pois[i].name;
    pois_[i].lat = pois[i].lat;
    pois_[i].lon = pois[i].lon;
  }
  poiCount_ = n;
  reprojectOverlays();
}

bool ScreenRadar::selectStaticMark(std::size_t index) {
  if (index >= staticMarkCount_) {
    return false;
  }
  clearSelection();
  hasStaticSelection_ = true;
  selectedStaticIndex_ = index;
  return true;
}

void ScreenRadar::clearStaticSelection() {
  hasStaticSelection_ = false;
  selectedStaticIndex_ = 0;
}

void ScreenRadar::setSettings(const RadarSettings& s) {
  settings_ = s;
  normalizeRadarSettings(settings_);
  reprojectOverlays();
}

void ScreenRadar::setDeclutterMode(RadarDeclutterMode m) {
  settings_.declutter = m;
  normalizeRadarSettings(settings_);
}

void ScreenRadar::setShowAirports(bool show) {
  settings_.showAirports = show;
  reprojectOverlays();
}

void ScreenRadar::setShowAirspace(bool show) {
  settings_.showAirspace = show;
  reprojectOverlays();
}

void ScreenRadar::setShowRoads(bool show) {
  settings_.showRoads = show;
  reprojectOverlays();
}

void ScreenRadar::setDemoMode(bool demo) { settings_.demoMode = demo; }

void ScreenRadar::openSettings() { settingsOpen_ = true; }

void ScreenRadar::closeSettings() { settingsOpen_ = false; }

void ScreenRadar::onIdleSettle() {
  if (settingsOpen_) {
    closeSettings();
  }
  clearSelection();
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
  card.tagLine3[0] = '\0';
  card.altLabel[0] = '\0';
  card.speedLabel[0] = '\0';
  card.type[0] = '\0';
  card.registration[0] = '\0';
  card.squawk[0] = '\0';
  card.notable = AircraftNotable::None;

  if (!hasSelection_ || selectedIndex_ >= blips_.count) {
    return card;
  }

  const RadarBlip& blip = blips_.items[selectedIndex_];
  const Aircraft& ac = blip.aircraft;
  card.present = true;
  card.notable = blip.notable;
  copyCallsign(card.callsign, sizeof(card.callsign), ac.callsign);
  copyCallsign(card.type, sizeof(card.type), ac.type);
  copyCallsign(card.registration, sizeof(card.registration), ac.registration);
  copyCallsign(card.squawk, sizeof(card.squawk), ac.squawk);
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
  formatRadarTagLine2(card.tagLine2, sizeof(card.tagLine2), ac,
                      RadarTagStyle::Full);
  formatRadarTagLine3(card.tagLine3, sizeof(card.tagLine3), ac.type, ac.squawk,
                      blip.notable);
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
  v.staticMarks = staticMarks_;
  v.staticMarkCount = staticMarkCount_;
  v.airspaceRings = airspaceRings_;
  v.airspaceRingCount = airspaceRingCount_;
  v.highways = highways_;
  v.highwayCount = highwayCount_;
  v.hasStaticSelection = hasStaticSelection_;
  v.selectedStaticIndex = selectedStaticIndex_;
  v.hasSelection = hasSelection_;
  v.selectedIndex = selectedIndex_;
  v.detail = detailCard();
  v.sweepAngleDeg = sweepAngleDeg_;
  v.settings = settings_;
  v.settingsOpen = settingsOpen_;
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
    b.notable = classify(b.aircraft);
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
  reprojectOverlays();
  if (!ready_) {
    return;
  }
  reprojectDisplayedOffsets();
}

void ScreenRadar::setActiveCenter(double lat, double lon, bool temp) {
  centerLat_ = lat;
  centerLon_ = lon;
  isTempCenter_ = temp;
  clearSelection();
  clearStaticSelection();
  reprojectOverlays();
  if (!ready_) {
    return;
  }
  // New center: clear painted blips; sweep will repaint from source_.
  std::memset(&blips_, 0, sizeof(blips_));
}

void ScreenRadar::reprojectOverlays() {
  staticMarkCount_ = 0;
  airspaceRingCount_ = 0;
  highwayCount_ = 0;

  if (hasMapContext_) {
    if (settings_.showAirports) {
      std::size_t airportIndices[40];
      float airportDistances[40];
      std::size_t airportCandidateCount = 0;
      for (std::size_t i = 0; i < mapContext_.airportCount; ++i) {
        const MapAirport& ap = mapContext_.airports[i];
        const float dist =
            distanceMiles(centerLat_, centerLon_, ap.lat, ap.lon);
        if (dist > rangeMiles_ + 0.01f) {
          continue;
        }
        airportIndices[airportCandidateCount] = i;
        airportDistances[airportCandidateCount] = dist;
        ++airportCandidateCount;
      }
      sortAirportCandidatesByDistance(airportIndices, airportDistances,
                                      airportCandidateCount);
      const std::size_t airportLimit =
          airportCandidateCount > kMaxProjectedAirports ? kMaxProjectedAirports
                                                        : airportCandidateCount;
      for (std::size_t i = 0; i < airportLimit; ++i) {
        const MapAirport& ap = mapContext_.airports[airportIndices[i]];
        RadarStaticMark& mark = staticMarks_[staticMarkCount_++];
        mark.kind = RadarStaticMark::Kind::Airport;
        if (ap.icao[0] != '\0') {
          copyCallsign(mark.label, sizeof(mark.label), ap.icao);
        } else {
          copyCallsign(mark.label, sizeof(mark.label), ap.name);
        }
        aircraftOffsetMiles(centerLat_, centerLon_, ap.lat, ap.lon,
                            mark.offsetXMi, mark.offsetYMi);
      }
    }

    if (settings_.showAirspace) {
      RingCandidate ringCands[24];
      std::size_t ringCandCount = 0;
      for (std::size_t i = 0; i < mapContext_.ringCount && ringCandCount < 24;
           ++i) {
        const MapAirspaceRing& ring = mapContext_.rings[i];
        if (!ringIntersectsRange(ring, centerLat_, centerLon_, rangeMiles_)) {
          continue;
        }
        double sumLat = 0.0;
        double sumLon = 0.0;
        for (uint8_t p = 0; p < ring.pointCount; ++p) {
          sumLat += ring.pointsLat[p];
          sumLon += ring.pointsLon[p];
        }
        const double centroidLat =
            sumLat / static_cast<double>(ring.pointCount);
        const double centroidLon =
            sumLon / static_cast<double>(ring.pointCount);
        ringCands[ringCandCount].sourceIndex = i;
        ringCands[ringCandCount].centroidDistMi =
            distanceMiles(centerLat_, centerLon_, centroidLat, centroidLon);
        ringCands[ringCandCount].cls = ring.cls;
        ++ringCandCount;
      }
      trimRingCandidates(ringCands, ringCandCount, kMaxAirspaceRingsView);
      for (std::size_t i = 0; i < ringCandCount; ++i) {
        const MapAirspaceRing& ring =
            mapContext_.rings[ringCands[i].sourceIndex];
        RadarAirspaceRingView& view = airspaceRings_[airspaceRingCount_++];
        view.cls = ring.cls;
        view.pointCount = ring.pointCount;
        for (uint8_t p = 0; p < ring.pointCount; ++p) {
          aircraftOffsetMiles(centerLat_, centerLon_, ring.pointsLat[p],
                              ring.pointsLon[p], view.offsetXMi[p],
                              view.offsetYMi[p]);
        }
      }
    }

    if (settings_.showRoads) {
      for (std::size_t i = 0;
           i < mapContext_.highwayCount && highwayCount_ < kMaxHighwaysView;
           ++i) {
        const MapHighway& hw = mapContext_.highways[i];
        bool inRange = false;
        for (uint8_t p = 0; p < hw.pointCount; ++p) {
          const float dist = distanceMiles(centerLat_, centerLon_,
                                           hw.pointsLat[p], hw.pointsLon[p]);
          if (dist <= rangeMiles_ + 0.01f) {
            inRange = true;
            break;
          }
        }
        if (!inRange) {
          continue;
        }
        RadarHighwayView& view = highways_[highwayCount_++];
        view.pointCount = hw.pointCount;
        for (uint8_t p = 0; p < hw.pointCount; ++p) {
          aircraftOffsetMiles(centerLat_, centerLon_, hw.pointsLat[p],
                              hw.pointsLon[p], view.offsetXMi[p],
                              view.offsetYMi[p]);
        }
      }
    }
  }

  if (settings_.showAirports) {
    for (std::size_t i = 0; i < poiCount_ && staticMarkCount_ < kMaxStaticMarks;
         ++i) {
      const StoredPoi& poi = pois_[i];
      const float dist =
          distanceMiles(centerLat_, centerLon_, poi.lat, poi.lon);
      if (dist > rangeMiles_ + 0.01f) {
        continue;
      }
      RadarStaticMark& mark = staticMarks_[staticMarkCount_++];
      mark.kind = RadarStaticMark::Kind::Poi;
      copyCallsign(mark.label, sizeof(mark.label), poi.name);
      aircraftOffsetMiles(centerLat_, centerLon_, poi.lat, poi.lon,
                          mark.offsetXMi, mark.offsetYMi);
    }
  }

  if (hasStaticSelection_ && selectedStaticIndex_ >= staticMarkCount_) {
    clearStaticSelection();
  }
}

}  // namespace desk_display
