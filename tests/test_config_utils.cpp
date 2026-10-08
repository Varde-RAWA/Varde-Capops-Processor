#include "config/Config.hpp"
#include "compute/Grid.hpp"
#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// UTILITY/EDGE CASE TESTS
// ============================================================================


TEST_CASE("Configuration loads default values")
{
    Configuration config = createTestConfig();

    REQUIRE(config.getProtobufVersion() == 1);
    REQUIRE(config.getCoordinateSystem() == "WGS84");
    REQUIRE(config.getNumFlights() == 3);
    REQUIRE(config.defaultBaseCapacity() == 1.0);
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



