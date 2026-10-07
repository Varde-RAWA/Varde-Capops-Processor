#include "../test_helpers.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/types/Position.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// GRID TESTS - Grid coordinate system and sector management
// ============================================================================

TEST_CASE("Grid initialization and sector counting")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());



    REQUIRE(grid.sectorCount() == 200);
    REQUIRE(grid.rows() == 20);
    REQUIRE(grid.cols() == 10);
}

TEST_CASE("Grid position conversion")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    // Test a position within the grid
    Position pos{59.5, 5.0};
    REQUIRE(grid.isInside(pos) == true);

    int sectorId = grid.determineSector(pos);
    REQUIRE(sectorId >= 0);
    REQUIRE(sectorId < grid.sectorCount());

    // Get sector center and verify it's within reasonable bounds
    Position center = grid.sectorCenter(sectorId);
    REQUIRE(center.latDeg >= config.grid().minLat);
    REQUIRE(center.latDeg <= config.grid().maxLat);
    REQUIRE(center.lonDeg >= config.grid().minLon);
    REQUIRE(center.lonDeg <= config.grid().maxLon);
}

TEST_CASE("Grid position outside bounds")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    Position outsidePos1{50.0, -80.0};
    Position outsidePos2{70.0, 10.0};
    Position outsidePos3{58.0, 2.0};

    REQUIRE(grid.isInside(outsidePos1) == false);
    REQUIRE(grid.isInside(outsidePos2) == false);
    REQUIRE(grid.isInside(outsidePos3) == false);
}

TEST_CASE("Grid row and column calculation")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    for (int sectorId = 0; sectorId < grid.sectorCount(); ++sectorId)
    {
        int row = grid.row(sectorId);
        int col = grid.column(sectorId);

        REQUIRE(row >= 0);
        REQUIRE(row < grid.rows());
        REQUIRE(col >= 0);
        REQUIRE(col < grid.cols());
    }
}

TEST_CASE("Grid sector center is inside the sector")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    // For each sector, verify that its center is actually inside that sector
    for (int sectorId = 0; sectorId < grid.sectorCount(); ++sectorId)
    {
        Position center = grid.sectorCenter(sectorId);

        // The center must be inside the grid
        REQUIRE(grid.isInside(center) == true);

        // The center must map back to the same sector
        int determinedSector = grid.determineSector(center);
        REQUIRE(determinedSector == sectorId);

        // Verify the center is within reasonable bounds (within the grid)
        REQUIRE(center.latDeg >= config.grid().minLat);
        REQUIRE(center.latDeg <= config.grid().maxLat);
        REQUIRE(center.lonDeg >= config.grid().minLon);
        REQUIRE(center.lonDeg <= config.grid().maxLon);
    }
}


TEST_CASE("Position on an internal boundary belongs to a sector")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    double cellWidth =
        (config.grid().maxLon - config.grid().minLon) / grid.cols();

    Position boundary = grid.sectorCenter(0);
    boundary.lonDeg = config.grid().minLon + cellWidth;

    REQUIRE(grid.isInside(boundary));

    int sectorId = grid.determineSector(boundary);
    REQUIRE((sectorId == 0 || sectorId == 1));
}