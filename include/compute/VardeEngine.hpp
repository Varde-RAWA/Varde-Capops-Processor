#pragma once


#include "domain/types/Position.hpp"
#include "domain/Track.hpp"
#include "domain/Zone.hpp"
#include "domain/ZoneCheckResult.hpp"

#include <string>
#include <vector>


class VardeEngine{
    private:
    public:
        VardeEngine() = default;
        Position predictPosition(const Track &track, double secondsAhead) const;
        std::vector<std::string> getZonesContainingTrack(const Track &track, const std::vector<Zone> &zones) const;

        std::vector<ZoneCheckResult> evaluateTrack(const Track &track, const std::vector<Zone> &Zones) const;
};


