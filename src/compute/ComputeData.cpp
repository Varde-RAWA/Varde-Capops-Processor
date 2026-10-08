#include "compute/ComputeData.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>


ComputeData::ComputeData(const Configuration &configuration)
    : config_(configuration), grid_(configuration.grid())
{
    initializeSectors();
}

void ComputeData::initializeSectors()
{
    for (int sectorId = 0; sectorId < grid_.sectorCount(); ++sectorId)
    {
        sectorSummariesById_.emplace(
            sectorId, SectorSummary(sectorId, grid_.row(sectorId), grid_.column(sectorId),
                                    "", // timestamp
                                    0  // localAircraftCount
                                    ));
    }
}

void ComputeData::removeTrack(std::string icao)
{
    auto currentTrack = activeTracksByIcao_.find(icao);

    if (currentTrack == activeTracksByIcao_.end())
    {
        return;
    }

    const Track &track = currentTrack->second;
    int sectorId = grid_.determineSector(track.getPosition());

    if (sectorId != -1)
    {
        sectorSummariesById_.at(sectorId).decreaseLocalAircraftCount();
    }

    activeTracksByIcao_.erase(currentTrack);
    sectorSummariesById_.at(sectorId).removeIcao(icao);
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
        int oldSectorId = grid_.determineSector(oldTrack.getPosition());
        if (oldSectorId != newSectorId)
        {
            sectorSummariesById_.at(oldSectorId).decreaseLocalAircraftCount();
            sectorSummariesById_.at(oldSectorId).removeIcao(newTrack.getIcao());

            sectorSummariesById_.at(newSectorId).increaseLocalAircraftCount();
            sectorSummariesById_.at(newSectorId).addIcao(newTrack.getIcao());
        }
        currentTrack->second = newTrack;
    }

    // new track
    else
    {
        sectorSummariesById_.at(newSectorId).increaseLocalAircraftCount();
        sectorSummariesById_.at(newSectorId).addIcao(newTrack.getIcao());
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

    for (const auto &[sectorId, summary] : sectorSummariesById_)
    {
        result.sectorSummaries.push_back(summary);
    }

    return result;
}
