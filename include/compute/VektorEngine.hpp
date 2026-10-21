#pragma once

#include "domain/Track.hpp"
#include "domain/Zone.hpp"
#include <optional>
#include <vector>

struct Detour
{
    std::vector<Position> waypoints;
    double extraDistanceKm;
    double extraTimeSeconds;
};

class VektorEngine
{
public:
    std::optional<Detour> calculateDetour(const Track &track, Position destination, const Zone &avoidance) const;
};