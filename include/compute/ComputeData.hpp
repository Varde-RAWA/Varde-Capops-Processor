#pragma once
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class ComputeData
{
  public:
    ComputeData(const Configuration &config);
    void handleTrackUpdate(const Track &track);
    void handleTrackSnapshot(const std::vector<Track> &tracks);
    ProcessingResult collectProcessingResult();

  private:
    void removeTrack(std::string icao);

    std::unordered_map<std::string, Track> activeTracksByIcao_;
    Grid grid_;
};