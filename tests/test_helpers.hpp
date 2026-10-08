#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include "config/Config.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <sw/redis++/redis++.h>
#include <string>

inline void requireRedisAvailable(const Configuration &config)
{
    std::string url = config.getRedisUrl();
    url += (url.find('?') == std::string::npos) ? "?" : "&";
    url += "connect_timeout=1000ms&socket_timeout=2000ms";

    std::string response;

    try
    {
        sw::redis::Redis redis(url);
        response = redis.ping();
    }
    catch (const sw::redis::IoError &)
    {
        SKIP("Redis unavailable; test skipped.");
    }
    REQUIRE(response == "PONG");
}

// Helper function to create a test configuration
inline Configuration createTestConfig()
{
    // Create a temporary config file for testing
    std::string configPath = "/tmp/test_config.cfg";
    std::ofstream configFile(configPath);

    configFile << "[grid]\n";
    configFile << "minLat=59.0\n";
    configFile << "maxLat=61.0\n";
    configFile << "minLon=4.0\n";
    configFile << "maxLon=6.0\n";
    configFile << "rows=3\n";
    configFile << "cols=3\n";
    configFile << "cellSizeDeg=0.1\n";
    configFile << "coordinateSystem=WGS84\n";
    configFile << "\n";
    configFile << "[protobufVersion]\n";
    configFile << "version=1\n";
    configFile << "\n";
    configFile << "[redis]\n";
    configFile << "redisUrl=tcp://127.0.0.1:6379\n";
    configFile << "redisChannel=test_channel\n";
    configFile << "\n";
    configFile << "[dataSource]\n";
    configFile << "type=sim\n";
    configFile << "\n";
    configFile << "[simulation]\n";
    configFile << "numFlights=3\n";
    configFile << "timestepSize=1.0\n";
    configFile << "\n";
    configFile << "[execution]\n";
    configFile << "loopIntervalMs=100\n";

    configFile.close();

    Configuration config(configPath);
    std::remove(configPath.c_str());

    return config;
}

// Helper function to create a test configuration with API sources
inline Configuration createTestConfigApi()
{
    // Create a temporary config file for testing
    std::string configPath = "/tmp/test_config_api.cfg";
    std::ofstream configFile(configPath);

    configFile << "[grid]\n";
    configFile << "minLat=59.0\n";
    configFile << "maxLat=61.0\n";
    configFile << "minLon=4.0\n";
    configFile << "maxLon=6.0\n";
    configFile << "rows=3\n";
    configFile << "cols=3\n";
    configFile << "coordinateSystem=WGS84\n";
    configFile << "\n";
    configFile << "[protobufVersion]\n";
    configFile << "version=1\n";
    configFile << "\n";
    configFile << "[redis]\n";
    configFile << "redisUrl=tcp://127.0.0.1:6379\n";
    configFile << "redisChannel=test_channel\n";
    configFile << "\n";
    configFile << "[dataSource]\n";
    configFile << "type=api\n";
    configFile << "\n";
    configFile << "[simulation]\n";
    configFile << "numFlights=3\n";
    configFile << "timestepSize=1.0\n";
    configFile << "\n";
    configFile << "[execution]\n";
    configFile << "loopIntervalMs=100\n";

    configFile.close();

    Configuration config(configPath);
    std::remove(configPath.c_str());

    return config;
}

// Helper function to find a sector summary by ID
static auto findSectorSummary(const ProcessingResult &result, int sectorId)
{
    return std::find_if(result.sectorSummaries.begin(), result.sectorSummaries.end(),
                        [sectorId](const SectorSummary &s) { return s.getSectorId() == sectorId; });
}

#endif // TEST_HELPERS_HPP
