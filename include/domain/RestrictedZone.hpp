#pragma once

#include "domain/types/Position.hpp"

#include <string>
#include <vector>

enum class ZoneType
{
    Disabled,
    Danger,
    Restricted,
    Temporary
};

class RestrictedZone
{
    private:
        std::string id_;
        ZoneType type_;
        bool enabled_;
        std::vector<Position> vertices_;

    public:
        RestrictedZone(std::string id, ZoneType zoneType, std::vector<Position> vertices);

        const std::string &getId() const;
        bool isEnabled() const;
        ZoneType getZoneType() const;
        bool containsPosition(const Position &position) const;
};





