#include "config/Config.hpp"
#include "config/ConfigLoader.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

Configuration::Configuration(const std::string& path)
{
    load(path);
}

void Configuration::load(const std::string& path)
{
    auto candidate = loadConfigurationValues(path);
    values_ = std::move(candidate);
}

const GridConfig& Configuration::grid() const { return values_.grid; }
int Configuration::getProtobufVersion() const { return values_.protobufVersion; }
std::string Configuration::getCoordinateSystem() const { return values_.coordinateSystem; }
std::string Configuration::getRedisUrl() const { return values_.redisUrl; }
std::string Configuration::getRedisChannel() const { return values_.redisChannel; }
SourceType Configuration::getSourceType() const { return values_.sourceType; }
int Configuration::getNumFlights() const { return values_.numFlights; }
double Configuration::getTimestepSize() const { return values_.timestepSize; }
int Configuration::getLoopInterval() const { return values_.loopIntervalMs; }
std::string Configuration::getZoneSource() const { return values_.zoneSource; }
std::string Configuration::getZoneApiUrl() const { return values_.zoneApiUrl; }
std::string Configuration::getZoneFile() const { return values_.zoneFile; }
std::string Configuration::getAvoidanceApiUrl() const { return values_.avoidanceApiUrl; }
std::string Configuration::getAvoidanceFile() const { return values_.avoidanceFile; }
int Configuration::getZoneRefreshSeconds() const { return values_.zoneRefreshSeconds; }

void GridConfig::computeDimensions()
{
    if (!std::isfinite(minLat) || !std::isfinite(maxLat) ||
        !std::isfinite(minLon) || !std::isfinite(maxLon))
        throw std::invalid_argument("grid: all four finite bounds are required");
    if (minLat <= -90.0 || maxLat >= 90.0 || minLon < -180.0 || maxLon > 180.0 ||
        maxLat <= minLat || maxLon <= minLon)
        throw std::invalid_argument("grid: bounds must be ordered and within supported geographic ranges");
    if (!std::isfinite(cellSizeDeg) || cellSizeDeg <= 0.0)
        throw std::invalid_argument("grid.cellSizeDeg must be finite and greater than zero");

    const double latRange = maxLat - minLat;
    const double lonRange = maxLon - minLon;
    const double rowCount = std::max(1.0, std::round(latRange / cellSizeDeg));
    const double lonScale = std::cos((minLat + maxLat) * 0.5 * std::acos(-1.0) / 180.0);
    const double colCount = std::max(1.0, std::round(lonRange * lonScale / (latRange / rowCount)));
    const double maxInt = std::numeric_limits<int>::max();
    if (!std::isfinite(rowCount) || !std::isfinite(colCount) ||
        rowCount > maxInt || colCount > maxInt || rowCount * colCount > maxInt)
        throw std::invalid_argument("grid: dimensions exceed the supported sector count");

    rows = static_cast<int>(rowCount);
    cols = static_cast<int>(colCount);
}
