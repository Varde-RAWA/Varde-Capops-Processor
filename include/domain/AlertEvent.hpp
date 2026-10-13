#pragma once

#include "domain/ZoneCheckResult.hpp"

#include <optional>
#include <string>


enum class AlertEventType
{
    OPENED,
    LEVEL_CHANGED,
    CLOSED
};


struct AlertEvent
{
    std::string alertId;
    std::string icao;
    std::string zoneId;
    WarningLevel level;
    std::optional<double> timeToEntrySeconds;
    AlertEventType type;
};