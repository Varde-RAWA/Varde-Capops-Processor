#pragma once

#include "domain/types/SourceType.hpp"
#include <string>

struct GridConfig
{
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;
    double cellSizeDeg = 1.0;
    int rows;
    int cols;
    void computeDimensions();
};

class Configuration
{
  public:
    explicit Configuration(const std::string &path);

    const GridConfig &grid() const;
    void load(const std::string &path);
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
    GridConfig grid_;
    int protobufVersion_;
    std::string coordinateSystem_;
    std::string redisUrl_;
    std::string redisChannel_;
    SourceType sourceType_;
    int numFlights_;
    double timestepSize_;
    int loopIntervalMs_;

    std::string zoneSource_ = "file";
    std::string zoneApiUrl_ = "http://localhost:5040/collections/restricted-areas/items";
    std::string zoneFile_ = "data/zones.geojson";
    std::string avoidanceApiUrl_ = "http://localhost:5040/collections/avoidance-areas/items";
    std::string avoidanceFile_ = "data/avoidance-areas.geojson";
    int zoneRefreshSeconds_ = 30;
};