#pragma once

#include <cstdint>
#include <string>
#include <vector>

class SectorSummary
{
  public:
    SectorSummary(int sectorId, int row, int column, std::string timestamp);

    // getters
    int getSectorId() const;
    int getRow() const;
    int getColumn() const;
    std::vector<std::string> getIcao24List() const;

    // helpers
    void updateTime(std::string timestamp);
    void addIcao(const std::string &icao);
    void removeIcao(const std::string &icao);

  private:
    int sectorId_;
    int row_;
    int column_;
    std::string timestamp_;
    std::vector<std::string> icao24List_;
};