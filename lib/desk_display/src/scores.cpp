#include "desk_display/scores.hpp"

#include <ArduinoJson.h>
#include <cstring>

namespace desk_display {
namespace {

void copyStr(char* dest, std::size_t destLen, const char* src) {
  if (destLen == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  std::strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

bool parseFlagstandRace(JsonObjectConst obj, FlagstandRace& out, bool expectStatus) {
  std::memset(&out, 0, sizeof(out));
  if (obj.isNull()) {
    return false;
  }
  if (obj["id"].isNull() || obj["name"].isNull() || obj["scheduledAt"].isNull() ||
      obj["leagueName"].isNull() || obj["seasonName"].isNull()) {
    return false;
  }

  out.present = true;
  copyStr(out.id, sizeof(out.id), obj["id"].as<const char*>());
  copyStr(out.name, sizeof(out.name), obj["name"].as<const char*>());
  copyStr(out.scheduledAt, sizeof(out.scheduledAt),
          obj["scheduledAt"].as<const char*>());
  copyStr(out.leagueName, sizeof(out.leagueName),
          obj["leagueName"].as<const char*>());
  copyStr(out.seasonName, sizeof(out.seasonName),
          obj["seasonName"].as<const char*>());

  out.hasSeriesName = false;
  if (!obj["seriesName"].isNull() && obj["seriesName"].is<const char*>()) {
    out.hasSeriesName = true;
    copyStr(out.seriesName, sizeof(out.seriesName),
            obj["seriesName"].as<const char*>());
  }

  out.hasTrackName = false;
  if (!obj["trackName"].isNull() && obj["trackName"].is<const char*>()) {
    out.hasTrackName = true;
    copyStr(out.trackName, sizeof(out.trackName),
            obj["trackName"].as<const char*>());
  }

  out.hasStatus = false;
  if (expectStatus && !obj["status"].isNull() && obj["status"].is<const char*>()) {
    out.hasStatus = true;
    copyStr(out.status, sizeof(out.status), obj["status"].as<const char*>());
  }

  return true;
}

}  // namespace

bool parseScores(const char* json, Scores& out) {
  if (!json) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (!doc["mlb"].is<JsonObjectConst>()) {
    return false;
  }

  JsonObjectConst mlb = doc["mlb"].as<JsonObjectConst>();
  if (mlb["live"].isNull()) {
    return false;
  }
  out.mlb.live = mlb["live"].as<bool>();

  out.mlb.hasScore = false;
  if (!mlb["score"].isNull() && mlb["score"].is<const char*>()) {
    out.mlb.hasScore = true;
    copyStr(out.mlb.score, sizeof(out.mlb.score), mlb["score"].as<const char*>());
  }

  out.mlb.hasInning = false;
  if (!mlb["inning"].isNull() && mlb["inning"].is<const char*>()) {
    out.mlb.hasInning = true;
    copyStr(out.mlb.inning, sizeof(out.mlb.inning),
            mlb["inning"].as<const char*>());
  }

  out.mlb.hasNextGame = false;
  if (!mlb["nextGame"].isNull() && mlb["nextGame"].is<const char*>()) {
    out.mlb.hasNextGame = true;
    copyStr(out.mlb.nextGame, sizeof(out.mlb.nextGame),
            mlb["nextGame"].as<const char*>());
  }

  out.mlb.hasMatchup = false;
  if (!mlb["matchup"].isNull() && mlb["matchup"].is<const char*>()) {
    out.mlb.hasMatchup = true;
    copyStr(out.mlb.matchup, sizeof(out.mlb.matchup),
            mlb["matchup"].as<const char*>());
  }

  out.mlb.hasWhenEt = false;
  if (!mlb["whenEt"].isNull() && mlb["whenEt"].is<const char*>()) {
    out.mlb.hasWhenEt = true;
    copyStr(out.mlb.whenEt, sizeof(out.mlb.whenEt),
            mlb["whenEt"].as<const char*>());
  }

  out.mlb.hasRecord = false;
  if (!mlb["record"].isNull() && mlb["record"].is<const char*>()) {
    out.mlb.hasRecord = true;
    copyStr(out.mlb.record, sizeof(out.mlb.record),
            mlb["record"].as<const char*>());
  }

  out.mlb.hasStandingLine = false;
  if (!mlb["standingLine"].isNull() && mlb["standingLine"].is<const char*>()) {
    out.mlb.hasStandingLine = true;
    copyStr(out.mlb.standingLine, sizeof(out.mlb.standingLine),
            mlb["standingLine"].as<const char*>());
  }

  out.mlb.hasTeamAbbr = false;
  if (!mlb["teamAbbr"].isNull() && mlb["teamAbbr"].is<const char*>()) {
    out.mlb.hasTeamAbbr = true;
    copyStr(out.mlb.teamAbbr, sizeof(out.mlb.teamAbbr),
            mlb["teamAbbr"].as<const char*>());
  }

  out.mlb.hasOpponentAbbr = false;
  if (!mlb["opponentAbbr"].isNull() && mlb["opponentAbbr"].is<const char*>()) {
    out.mlb.hasOpponentAbbr = true;
    copyStr(out.mlb.opponentAbbr, sizeof(out.mlb.opponentAbbr),
            mlb["opponentAbbr"].as<const char*>());
  }

  out.mlb.homeAway = MlbHomeAway::Unknown;
  if (!mlb["homeAway"].isNull() && mlb["homeAway"].is<const char*>()) {
    const char* ha = mlb["homeAway"].as<const char*>();
    if (std::strcmp(ha, "home") == 0) {
      out.mlb.homeAway = MlbHomeAway::Home;
    } else if (std::strcmp(ha, "away") == 0) {
      out.mlb.homeAway = MlbHomeAway::Away;
    }
  }

  auto parseOptInt = [&](const char* key, bool& has, int& value) {
    has = false;
    value = 0;
    if (!mlb[key].isNull() && mlb[key].is<int>()) {
      has = true;
      value = mlb[key].as<int>();
    }
  };
  auto parseOptBool = [&](const char* key, bool& has, bool& value) {
    has = false;
    value = false;
    if (!mlb[key].isNull() && mlb[key].is<bool>()) {
      has = true;
      value = mlb[key].as<bool>();
    }
  };

  parseOptInt("teamRuns", out.mlb.hasTeamRuns, out.mlb.teamRuns);
  parseOptInt("opponentRuns", out.mlb.hasOpponentRuns, out.mlb.opponentRuns);
  parseOptInt("balls", out.mlb.hasBalls, out.mlb.balls);
  parseOptInt("strikes", out.mlb.hasStrikes, out.mlb.strikes);
  parseOptInt("outs", out.mlb.hasOuts, out.mlb.outs);
  parseOptBool("onFirst", out.mlb.hasOnFirst, out.mlb.onFirst);
  parseOptBool("onSecond", out.mlb.hasOnSecond, out.mlb.onSecond);
  parseOptBool("onThird", out.mlb.hasOnThird, out.mlb.onThird);

  out.mlb.hasBatterName = false;
  if (!mlb["batterName"].isNull() && mlb["batterName"].is<const char*>()) {
    out.mlb.hasBatterName = true;
    copyStr(out.mlb.batterName, sizeof(out.mlb.batterName),
            mlb["batterName"].as<const char*>());
  }
  out.mlb.hasBatterAvg = false;
  if (!mlb["batterAvg"].isNull() && mlb["batterAvg"].is<const char*>()) {
    out.mlb.hasBatterAvg = true;
    copyStr(out.mlb.batterAvg, sizeof(out.mlb.batterAvg),
            mlb["batterAvg"].as<const char*>());
  }
  out.mlb.hasBatterSummary = false;
  if (!mlb["batterSummary"].isNull() && mlb["batterSummary"].is<const char*>()) {
    out.mlb.hasBatterSummary = true;
    copyStr(out.mlb.batterSummary, sizeof(out.mlb.batterSummary),
            mlb["batterSummary"].as<const char*>());
  }
  out.mlb.hasPitcherName = false;
  if (!mlb["pitcherName"].isNull() && mlb["pitcherName"].is<const char*>()) {
    out.mlb.hasPitcherName = true;
    copyStr(out.mlb.pitcherName, sizeof(out.mlb.pitcherName),
            mlb["pitcherName"].as<const char*>());
  }
  out.mlb.hasPitcherEra = false;
  if (!mlb["pitcherEra"].isNull() && mlb["pitcherEra"].is<const char*>()) {
    out.mlb.hasPitcherEra = true;
    copyStr(out.mlb.pitcherEra, sizeof(out.mlb.pitcherEra),
            mlb["pitcherEra"].as<const char*>());
  }
  out.mlb.hasPitcherSummary = false;
  if (!mlb["pitcherSummary"].isNull() && mlb["pitcherSummary"].is<const char*>()) {
    out.mlb.hasPitcherSummary = true;
    copyStr(out.mlb.pitcherSummary, sizeof(out.mlb.pitcherSummary),
            mlb["pitcherSummary"].as<const char*>());
  }

  if (doc["flagstand"].is<JsonObjectConst>()) {
    JsonObjectConst fs = doc["flagstand"].as<JsonObjectConst>();
    if (!fs["lastResult"].isNull() && fs["lastResult"].is<JsonObjectConst>()) {
      parseFlagstandRace(fs["lastResult"].as<JsonObjectConst>(),
                         out.flagstand.lastResult, false);
    }
    if (!fs["nextRace"].isNull() && fs["nextRace"].is<JsonObjectConst>()) {
      parseFlagstandRace(fs["nextRace"].as<JsonObjectConst>(),
                         out.flagstand.nextRace, true);
    }
  }

  out.hasUpdatedAt = false;
  if (!doc["updatedAt"].isNull() && doc["updatedAt"].is<const char*>()) {
    out.hasUpdatedAt = true;
    copyStr(out.updatedAt, sizeof(out.updatedAt),
            doc["updatedAt"].as<const char*>());
  }

  return true;
}

}  // namespace desk_display
