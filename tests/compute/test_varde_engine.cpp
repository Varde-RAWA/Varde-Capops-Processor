#include "compute/VardeEngine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

namespace
{
Track makePredictionTrack(
    Position position,
    double speedKnots,
    double groundTrackDegrees)
{
    return Track(
        "SIM-0",
        "2026-10-05T12:00:00Z",
        position,
        15000.0,
        speedKnots,
        0.0,
        groundTrackDegrees,
        groundTrackDegrees
    );
}
}

TEST_CASE("Varde projects an eastward aircraft 120 seconds ahead")
{
    const Track track(
        "SIM-0",
        "2026-10-05T12:00:00Z",
        Position{60.0, 4.6},
        15000.0, // Altitude in feet.
        360.0,   // Ground speed in knots.
        0.0,     // Vertical speed.
        90.0,    // Heading.
        90.0     // Ground-track direction.
    );

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track);

    // At 360 knots, the aircraft travels 22.224 km in 120 seconds.
    REQUIRE(predicted.latDeg ==
            Catch::Approx(60.0).margin(1e-7));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(4.99928135).margin(1e-7));
}

TEST_CASE("Varde predicts movement in all four directions", "[projection]")
{
    double direction = 0.0;
    Position expected;

    SECTION("North")
    {
        direction = 0.0;
        expected = {60.19964068, 4.6};
    }

    SECTION("East")
    {
        direction = 90.0;
        expected = {60.0, 4.99928135};
    }

    SECTION("South")
    {
        direction = 180.0;
        expected = {59.80035932, 4.6};
    }

    SECTION("West")
    {
        direction = 270.0;
        expected = {60.0, 4.20071865};
    }

    const Track track =
        makePredictionTrack({60.0, 4.6}, 360.0, direction);

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, 120.0);

    REQUIRE(predicted.latDeg ==
            Catch::Approx(expected.latDeg).margin(1e-7));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(expected.lonDeg).margin(1e-7));
}

TEST_CASE("A stationary aircraft stays at its position", "[projection]")
{
    const Track track =
        makePredictionTrack({60.0, 4.6}, 0.0, 45.0);

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, 120.0);

    REQUIRE(predicted.latDeg == Catch::Approx(60.0));
    REQUIRE(predicted.lonDeg == Catch::Approx(4.6));
}

TEST_CASE("Varde accounts for latitude when moving east", "[projection]")
{
    double latitude = 0.0;
    double expectedLongitude = 0.0;

    SECTION("At the equator")
    {
        latitude = 0.0;
        expectedLongitude = 0.19964068;
    }

    SECTION("At 60 degrees north")
    {
        latitude = 60.0;
        expectedLongitude = 0.39928135;
    }

    SECTION("Near the North Pole at 80 degrees north")
    {
        latitude = 80.0;
        expectedLongitude = 1.14968483;
    }

    const Track track =
        makePredictionTrack({latitude, 0.0}, 360.0, 90.0);

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, 120.0);

    REQUIRE(predicted.latDeg ==
            Catch::Approx(latitude).margin(1e-7));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(expectedLongitude).margin(1e-7));
}

TEST_CASE("Varde rejects negative prediction time", "[projection]")
{
    const Track track =
        makePredictionTrack({60.0, 4.6}, 360.0, 90.0);

    const VardeEngine varde;

    REQUIRE_THROWS_AS(
        varde.predictPosition(track, -1.0),
        std::invalid_argument
    );
}


TEST_CASE("Varde finds enabled zones containing an aircraft", "[zones]")
{
    const std::vector<Position> vertices{
        {59.95, 5.00},
        {59.95, 5.20},
        {60.05, 5.20},
        {60.05, 5.00},
        {59.95, 5.00}
    };

    // Two enabled zones overlap. A disabled zone has the same boundary.
    const std::vector<Zone> zones{
        Zone("D-101", ZoneType::Danger, vertices),
        Zone("R-201", ZoneType::Restricted, vertices),
        Zone("D-102", ZoneType::Disabled, vertices)
    };

    Position position{60.0, 5.1};
    std::vector<std::string> expected{"D-101", "R-201"};

    SECTION("Inside both enabled zones")
    {
        position = {60.0, 5.1};
    }

    SECTION("Outside all zones")
    {
        position = {60.0, 4.6};
        expected.clear();
    }

    SECTION("On the boundary of both enabled zones")
    {
        position = {60.0, 5.0};
    }

    const Track track = makePredictionTrack(position, 360.0, 90.0);
    const VardeEngine varde;

    const auto matches = varde.getZonesContainingTrack(track, zones);

    REQUIRE(matches == expected);
}

TEST_CASE("Varde returns no matches when there are no zones", "[zones]")
{
    const Track track =
        makePredictionTrack({60.0, 5.1}, 360.0, 90.0);

    const VardeEngine varde;

    REQUIRE(varde.getZonesContainingTrack(track, {}).empty());
}