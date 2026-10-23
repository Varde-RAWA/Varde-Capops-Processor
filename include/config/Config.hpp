#pragma once

#include "domain/types/SourceType.hpp"
#include <limits>
#include <string>

struct GridConfig
{
    double minLat = std::numeric_limits<double>::quiet_NaN();
    double maxLat = std::numeric_limits<double>::quiet_NaN();
    double minLon = std::numeric_limits<double>::quiet_NaN();
    double maxLon = std::numeric_limits<double>::quiet_NaN();
    double cellSizeDeg = 1.0;
    int rows = 0;
    int cols = 0;
    void computeDimensions();
};

struct ConfigurationValues
{
    GridConfig grid;
    int protobufVersion = 3;
    std::string coordinateSystem = "WGS84";
    std::string redisUrl = "tcp://localhost:6379?connect_timeout=200ms&socket_timeout=200ms";
    std::string redisChannel = "flightdata";
    SourceType sourceType = SourceType::Simulation;
    int numFlights = 1;
    double timestepSize = 0.5;
    int loopIntervalMs = 500;
    std::string zoneSource = "file";
    std::string zoneApiUrl = "http://localhost:5040/collections/restricted-areas/items";
    std::string zoneFile = "data/zones.geojson";
    std::string avoidanceApiUrl = "http://localhost:5040/collections/avoidance-areas/items";
    std::string avoidanceFile = "data/avoidance-areas.geojson";
    int zoneRefreshSeconds = 30;
};

class Configuration
{
public:
    explicit Configuration(const std::string& path);
    void load(const std::string& path);
    const GridConfig& grid() const;
    int getProtobufVersion() const;
    std::string getCoordinateSystem() const;
    std::string getRedisUrl() const;
    std::string getRedisChannel() const;
    SourceType getSourceType() const;
    int getNumFlights() const;
    double getTimestepSize() const;
    int getLoopInterval() const;

    std::string getZoneSource() const;
    std::string getZoneApiUrl() const;
    std::string getZoneFile() const;
    std::string getAvoidanceApiUrl() const;
    std::string getAvoidanceFile() const;
    int getZoneRefreshSeconds() const;

private:
    ConfigurationValues values_;
};