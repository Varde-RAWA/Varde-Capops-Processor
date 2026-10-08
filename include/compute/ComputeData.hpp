#pragma once
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/RiskEvent.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "domain/types/SectorState.hpp"
#include <deque>
#include <string>
#include <unordered_map>

class ComputeData
{
  public:
    ComputeData(const Configuration &config);
    void handleTrackUpdate(const Track &track);
    ProcessingResult collectProcessingResult();

  private:
    void evaluateSectorState(int sectorId, const std::string &timestamp);
    void removeTrack(std::string icao);

    std::unordered_map<std::string, Track> activeTracksByIcao_;
    std::unordered_map<int, SectorSummary> sectorSummariesById_;
    std::deque<RiskEvent> pendingRiskEvents_;
    ProcessingResult result_;
    Grid grid_;
    Configuration config_;
    int totalRiskEvents_ = 0;

    void initializeSectors();
};