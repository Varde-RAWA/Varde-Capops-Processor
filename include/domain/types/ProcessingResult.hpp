#pragma once
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include <vector>

struct ProcessingResult
{
    std::vector<Track> tracks;
    std::vector<SectorSummary> sectorSummaries;
};