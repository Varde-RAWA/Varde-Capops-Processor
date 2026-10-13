#include "domain/ZoneCheckResult.hpp"

#include <utility>

ZoneCheckResult::ZoneCheckResult(
    std::string icao,
    std::string zoneId,
    WarningLevel level,
    std::optional<double> timeToEntrySeconds)
    : icao_(std::move(icao)),
      zoneId_(std::move(zoneId)),
      level_(level),
      timeToEntrySeconds_(timeToEntrySeconds)
{
}

const std::string &ZoneCheckResult::getIcao() const{
    return icao_;
}

const std::string &ZoneCheckResult::getZoneId() const{
    return zoneId_;
}

WarningLevel ZoneCheckResult::getLevel() const{
    return level_;
}

std::optional<double> ZoneCheckResult::getTimeToEntrySeconds() const{
    return timeToEntrySeconds_;
}