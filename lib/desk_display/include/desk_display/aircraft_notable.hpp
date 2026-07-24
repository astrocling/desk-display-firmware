#pragma once

#include "desk_display/adsb.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Notable traffic class for radar symbology (priority order for classify). */
enum class AircraftNotable : uint8_t {
  None = 0,
  Interesting = 1,
  Military = 2,
  Emergency = 3,
};

constexpr uint8_t kDbFlagMilitary = 0x01;
constexpr uint8_t kDbFlagInteresting = 0x02;

/**
 * Dayton-area CareFlight + Medflight registrations (tail numbers).
 * Matched against Aircraft.registration and Aircraft.callsign.
 */
inline constexpr const char* kRadarInterestingRegsDefault[] = {
    // CareFlight
    "N730CF",  // CareFlight 1
    "N841CF",  // CareFlight 1 (Leonardo)
    "N520CF",  // CareFlight 2
    "N3842",   // CareFlight 2 backup (EC135)
    "N164CF",  // CareFlight 3
    "N942CF",  // CareFlight 4
    "N625CF",  // CareFlight 4 alt
    // Medflight
    "N130HB",  // Medflight 1 — KRZT
    "N130JV",  // Medflight 2 — 3OA9
    "N130KH",  // Medflight 3 — 9OH6
    "N130MU",  // Medflight 6 — I71
    "N130NB",  // Medflight 9 — 6OH3 Eaton
};

inline constexpr std::size_t kRadarInterestingRegCount =
    sizeof(kRadarInterestingRegsDefault) / sizeof(kRadarInterestingRegsDefault[0]);

/**
 * Classify an aircraft. Priority: Emergency > Military > Interesting.
 * `interestingRegs` may be null (treated as empty).
 */
AircraftNotable classifyAircraftNotable(const Aircraft& ac,
                                        const char* const* interestingRegs,
                                        std::size_t interestingRegCount);

/** Short tag reason: "EMRG" / "MIL" / "INTR", or nullptr if None. */
const char* aircraftNotableReason(AircraftNotable notable);

}  // namespace desk_display
