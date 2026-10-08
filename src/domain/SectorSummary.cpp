#include "domain/SectorSummary.hpp"
#include <algorithm>

SectorSummary::SectorSummary(int sectorId, int row, int column, std::string timestamp,
                             int localAircraftCount,double localAircraftBaseCapacity,
                             SectorState riskSeverity)
    : sectorId_(sectorId), row_(row), column_(column), timestamp_(std::move(timestamp)),
      localAircraftCount_(localAircraftCount), localAircraftBaseCapacity_(localAircraftBaseCapacity),
      riskSeverity_(riskSeverity)
{
}

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

int SectorSummary::getLocalAircraftCount() const
{
    return localAircraftCount_;
}

double SectorSummary::getBaseCapacity() const
{
    return localAircraftBaseCapacity_;
}

void SectorSummary::increaseLocalAircraftCount()
{
    localAircraftCount_++;
}

SectorState SectorSummary::getState() const
{
    return riskSeverity_;
}

double SectorSummary::getEffectiveCapacity() const
{
    return localAircraftBaseCapacity_;
}

void SectorSummary::decreaseLocalAircraftCount()
{
    if (localAircraftCount_ > 0)
    {
        localAircraftCount_--;
    }
}

bool SectorSummary::isAtRisk()
{
    if (localAircraftCount_ > getEffectiveCapacity())
    {
        return true;
    }
    return false;
}

bool SectorSummary::isCongested()
{
    if (localAircraftCount_ > localAircraftBaseCapacity_)
    {
        return true;
    }
    return false;
}

void SectorSummary::updateState()
{
    if (isCongested())
    {
        riskSeverity_ = SectorState::CONGESTED;
    }
    else if (isAtRisk())
    {
        riskSeverity_ = SectorState::AT_RISK;
    }
    else
    {
        riskSeverity_ = SectorState::NORMAL;
    }
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