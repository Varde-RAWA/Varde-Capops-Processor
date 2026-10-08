#include "domain/SectorSummary.hpp"
#include <algorithm>

SectorSummary::SectorSummary(int sectorId, int row, int column, std::string timestamp)
    : sectorId_(sectorId), row_(row), column_(column), timestamp_(std::move(timestamp)){}

int SectorSummary::getSectorId() const
{
    return sectorId_;
}

int SectorSummary::getRow() const
{
    return row_;
}

int SectorSummary::getColumn() const
{
    return column_;
}



void SectorSummary::updateTime(std::string timestamp)
{
    timestamp_ = std::move(timestamp);
}

void SectorSummary::addIcao(const std::string &icao)
{
    icao24List_.push_back(icao);
}

void SectorSummary::removeIcao(const std::string &icao)
{
    auto it = std::find(icao24List_.begin(), icao24List_.end(), icao);
    if (it != icao24List_.end())
    {
        icao24List_.erase(it);
    }
}

std::vector<std::string> SectorSummary::getIcao24List() const
{
    return icao24List_;
}