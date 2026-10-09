#include "compute/ComputeData.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>


ComputeData::ComputeData(const Configuration &configuration)
    : grid_(configuration.grid())
{}

void ComputeData::removeTrack(std::string icao)
{
    activeTracksByIcao_.erase(icao);
}

void ComputeData::handleTrackUpdate(const Track &newTrack)
{
    int newSectorId = grid_.determineSector(newTrack.getPosition());

    if (newSectorId == -1)
    {
        removeTrack(newTrack.getIcao());
        return;
    }

    auto currentTrack = activeTracksByIcao_.find(newTrack.getIcao());

    // existing track update
    if (currentTrack != activeTracksByIcao_.end())
    {
        const Track &oldTrack = currentTrack->second;
        if (newTrack.getTimestamp() <= oldTrack.getTimestamp())
        {
            return;
        }
        currentTrack->second = newTrack;
    }

    // new track
    else
    {
        activeTracksByIcao_.insert({newTrack.getIcao(), newTrack});
    }
}


ProcessingResult ComputeData::collectProcessingResult()
{
    ProcessingResult result;

    for (const auto &[icao, track] : activeTracksByIcao_)
    {
        result.tracks.push_back(track);
    }

    return result;
}
