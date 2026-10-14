#pragma once

#include "domain/AlertEvent.hpp"
#include "domain/ZoneCheckResult.hpp"

#include <string>
#include <map>
#include <optional>
#include <utility>
#include <vector>
#include <functional>


class AlertManager
{
public:
    explicit AlertManager(std::function<std::string()> makeAlertId);

    std::optional<AlertEvent> update(const ZoneCheckResult &result);
    std::vector<AlertEvent> handleTrackDisappearance(const std::string &icao);
    std::vector<AlertEvent> handleZoneDeactivation(const std::string &zoneId);

private:
    struct State
    {
        std::string alertId;
        WarningLevel publishedLevel;
        std::vector<ZoneCheckResult> lowerResults;
    };

    using Key = std::pair<std::string, std::string>; // ICAO, zone ID

    std::map<Key, State> states_;
    std::function<std::string()> makeAlertId_;
};

