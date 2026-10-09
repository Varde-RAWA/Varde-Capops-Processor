#pragma once
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <string>
#include <unordered_map>

class ComputeData
{
  public:
    ComputeData(const Configuration &config);
    void handleTrackUpdate(const Track &track);
    ProcessingResult collectProcessingResult();

  private:
    void removeTrack(std::string icao);

    std::unordered_map<std::string, Track> activeTracksByIcao_;
    Grid grid_;
};