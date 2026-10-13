#pragma once

#include <optional>
#include <string>

enum class WarningLevel
{
    NONE,
    PREDICTED,
    IMMINENT,
    INFRINGEMENT
};


class ZoneCheckResult{
    private:
        std::string icao_;
        std::string zoneId_;
        WarningLevel level_;
        std::optional<double> timeToEntrySeconds_;

    public:
        ZoneCheckResult(std::string icao, std::string zoneId, WarningLevel level, std::optional<double> timeToEntrySeconds);

        const std::string& getIcao() const;
        const std::string& getZoneId() const;
        WarningLevel getLevel() const;
        std::optional<double> getTimeToEntrySeconds() const;

};









