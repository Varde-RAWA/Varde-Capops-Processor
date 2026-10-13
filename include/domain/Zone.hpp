#pragma once

#include "domain/types/Position.hpp"

#include <string>
#include <vector>
#include <optional>

enum class ZoneType
{
    Disabled,
    Danger,
    Restricted,
    Temporary
};

class Zone
{
    private:
        std::string id_;
        ZoneType type_;
        bool enabled_;
        std::vector<Position> vertices_;

    public:
        Zone(std::string id, ZoneType zoneType, std::vector<Position> vertices);

        const std::string &getId() const;
        bool isEnabled() const;
        ZoneType getZoneType() const;
        bool containsPosition(const Position &position) const;
        std::optional<double> firstEntryFraction(const Position &start, const Position &end) const;
};





