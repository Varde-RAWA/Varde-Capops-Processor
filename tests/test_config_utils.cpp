#include "config/Config.hpp"
#include "compute/Grid.hpp"
#include "domain/types/WeatherSeverity.hpp"
#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// UTILITY/EDGE CASE TESTS
// ============================================================================

TEST_CASE("Weather levels are properly sorted")
{
    Configuration config = createTestConfig();
    auto weatherLevels = config.getSortedWeatherLevels();

    // Weather levels should be sorted by threshold
    for (size_t i = 1; i < weatherLevels.size(); ++i)
    {
        REQUIRE(weatherLevels[i].second >= weatherLevels[i - 1].second);
    }
}

TEST_CASE("Configuration loads default values")
{
    Configuration config = createTestConfig();

    REQUIRE(config.getProtobufVersion() == 1);
    REQUIRE(config.getCoordinateSystem() == "WGS84");
    REQUIRE(config.getNumFlights() == 3);
    REQUIRE(config.defaultBaseCapacity() == 1.0);
}

TEST_CASE("Weather factors are applied correctly")
{
    Configuration config = createTestConfig();

    double factorOk = config.weatherFactor(WeatherSeverity::OK);
    double factorDegraded = config.weatherFactor(WeatherSeverity::DEGRADED);
    double factorSevere = config.weatherFactor(WeatherSeverity::SEVERE);
    double factorExtreme = config.weatherFactor(WeatherSeverity::EXTREME);

    REQUIRE(factorOk == 1.0);
    REQUIRE(factorDegraded == 0.8);
    REQUIRE(factorSevere == 0.6);
    REQUIRE(factorExtreme == 0.4);

    // Generally, worse weather should reduce capacity
    REQUIRE(factorOk >= factorDegraded);
    REQUIRE(factorDegraded >= factorSevere);
    REQUIRE(factorSevere >= factorExtreme);
}

TEST_CASE("Loads a complete configuration")
{
    Configuration config = createTestConfig();

    REQUIRE(config.grid().minLat == 59.0);
    REQUIRE(config.grid().maxLat == 61.0);
    REQUIRE(config.grid().minLon == 4.0);
    REQUIRE(config.grid().maxLon == 6.0);
    REQUIRE(config.grid().cellSizeDeg == 0.1);

    REQUIRE(config.getCoordinateSystem() == "WGS84");
    REQUIRE(config.getProtobufVersion() == 1);

    REQUIRE(config.getRedisUrl() == "tcp://127.0.0.1:6379");
    REQUIRE(config.getRedisChannel() == "test_channel");

    REQUIRE(config.getSourceType() == SourceType::Simulation);
    REQUIRE(config.getNumFlights() == 3);
    REQUIRE(config.getTimestepSize() == 1.0);
    REQUIRE(config.getLoopInterval() == 100);
}


TEST_CASE("Loaded configuration produces a grid that locates aircraft")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    REQUIRE(grid.rows() == 20);
    REQUIRE(grid.cols() == 10);
    REQUIRE(grid.sectorCount() == 200);

    Position aircraft{59.25, 4.35};
    REQUIRE(grid.isInside(aircraft));
    REQUIRE(grid.determineSector(aircraft) == 21);

    Position centre = grid.sectorCenter(21);
    REQUIRE(grid.isInside(centre));
    REQUIRE(grid.determineSector(centre) == 21);

    Position outside{58.0, 5.0};
    REQUIRE_FALSE(grid.isInside(outside));
    REQUIRE(grid.determineSector(outside) == -1);
}



