#pragma once

#include "domain/types/SectorState.hpp"
#include <cstdint>
#include <string>
#include <vector>

class SectorSummary
{
  public:
    SectorSummary(int sectorId, int row, int column, std::string timestamp, int localAircraftCount,
                  double localAircraftBaseCapacity, SectorState riskSeverity);

    // getters
    int getSectorId() const;
    int getRow() const;
    int getColumn() const;
    int getLocalAircraftCount() const;
    double getBaseCapacity() const;
    SectorState getState() const;
    double getEffectiveCapacity() const;
    std::vector<std::string> getIcao24List() const;

    // helpers
    void increaseLocalAircraftCount();
    void decreaseLocalAircraftCount();
    void updateState();
    void updateTime(std::string timestamp);
    bool isAtRisk();
    bool isCongested();
    void addIcao(const std::string &icao);
    void removeIcao(const std::string &icao);

  private:
    int sectorId_;
    int row_;
    int column_;
    std::string timestamp_;
    int localAircraftCount_ = 0;
    double localAircraftBaseCapacity_;
    SectorState riskSeverity_;
    std::vector<std::string> icao24List_;
};