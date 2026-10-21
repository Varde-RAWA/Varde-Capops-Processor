#pragma once

#include "compute/VektorEngine.hpp"
#include <string>

struct VektorProposal
{
    std::string proposalId;
    std::string alertId;
    std::string icao;
    std::string zoneId;
    Detour detour;
};
