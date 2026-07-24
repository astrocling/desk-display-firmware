#include "desk_display/aircraft_notable.hpp"

#include <cstring>

namespace desk_display {
namespace {

bool isEmergencySquawk(const char* squawk) {
  if (!squawk || squawk[0] == '\0') {
    return false;
  }
  return std::strcmp(squawk, "7500") == 0 || std::strcmp(squawk, "7600") == 0 ||
         std::strcmp(squawk, "7700") == 0;
}

bool isEmergencyField(const char* emergency) {
  if (!emergency || emergency[0] == '\0') {
    return false;
  }
  return std::strcmp(emergency, "none") != 0;
}

bool matchesInterestingReg(const Aircraft& ac, const char* const* regs,
                           std::size_t count) {
  if (!regs || count == 0) {
    return false;
  }
  for (std::size_t i = 0; i < count; ++i) {
    const char* reg = regs[i];
    if (!reg || reg[0] == '\0') {
      continue;
    }
    if (ac.registration[0] != '\0' &&
        std::strcmp(ac.registration, reg) == 0) {
      return true;
    }
    if (ac.callsign[0] != '\0' && std::strcmp(ac.callsign, reg) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

AircraftNotable classifyAircraftNotable(const Aircraft& ac,
                                        const char* const* interestingRegs,
                                        std::size_t interestingRegCount) {
  if (isEmergencySquawk(ac.squawk) || isEmergencyField(ac.emergency)) {
    return AircraftNotable::Emergency;
  }
  if ((ac.dbFlags & kDbFlagMilitary) != 0) {
    return AircraftNotable::Military;
  }
  if ((ac.dbFlags & kDbFlagInteresting) != 0 ||
      matchesInterestingReg(ac, interestingRegs, interestingRegCount)) {
    return AircraftNotable::Interesting;
  }
  return AircraftNotable::None;
}

const char* aircraftNotableReason(AircraftNotable notable) {
  switch (notable) {
    case AircraftNotable::Emergency:
      return "EMRG";
    case AircraftNotable::Military:
      return "MIL";
    case AircraftNotable::Interesting:
      return "INTR";
    case AircraftNotable::None:
    default:
      return nullptr;
  }
}

}  // namespace desk_display
