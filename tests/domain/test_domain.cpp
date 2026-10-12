#include "../test_helpers.hpp"

#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/RestrictedZone.hpp"

#include <catch2/catch_test_macros.hpp>

// ============================================================================
// DOMAIN TESTS - Track, RestrictedZone, Position
// ============================================================================


RestrictedZone makeSquareZone()
{
    return RestrictedZone(
        "D-101",
        ZoneType::Danger,
        {
            {0.0, 0.0},
            {0.0, 10.0},
            {10.0, 10.0},
            {10.0, 0.0},
            {0.0, 0.0} // Repeat the first corner to close the polygon.
        });
}


TEST_CASE("Track initialization and getters")
{
    Position pos{59.5, 5.0};
    Track track("ABC123", "2024-01-01T12:00:00Z", pos, 10000.0, 450.0, 100.0, 180.0, 175.0);

    REQUIRE(track.getIcao() == "ABC123");
    REQUIRE(track.getTimestamp() == "2024-01-01T12:00:00Z");
    REQUIRE(track.getPosition().latDeg == 59.5);
    REQUIRE(track.getPosition().lonDeg == 5.0);
    REQUIRE(track.getAltitudeFeet() == 10000.0);
    REQUIRE(track.getGroundSpeedKnots() == 450.0);
    REQUIRE(track.getVerticalSpeedFeetPerMinute() == 100.0);
    REQUIRE(track.getHeadingDegrees() == 180.0);
    REQUIRE(track.getGroundTrackDegrees() == 175.0);
}

TEST_CASE("RestrictedZone initialization and getters")
{
    const RestrictedZone zone = makeSquareZone();

    REQUIRE(zone.getId() == "D-101");
    REQUIRE(zone.getZoneType() == ZoneType::Danger);
    REQUIRE(zone.isEnabled());
}

TEST_CASE("Disabled zone reports that it is disabled")
{
    const RestrictedZone zone(
        "D-102",
        ZoneType::Disabled,
        {
            {0.0, 0.0},
            {0.0, 10.0},
            {10.0, 10.0},
            {10.0, 0.0},
            {0.0, 0.0}
        });

    REQUIRE_FALSE(zone.isEnabled());
}

TEST_CASE("RestrictedZone detects a position outside the polygon")
{
    const RestrictedZone zone = makeSquareZone();

    REQUIRE_FALSE(zone.containsPosition({5.0, 12.0}));
}

TEST_CASE("RestrictedZone treats an edge and a corner as inside")
{
    const RestrictedZone zone = makeSquareZone();

    REQUIRE(zone.containsPosition({0.0, 5.0}));  // On an edge.
    REQUIRE(zone.containsPosition({0.0, 0.0}));  // At a corner.
}

TEST_CASE("RestrictedZone rejects a polygon without repeated vertices")
{
    REQUIRE_THROWS_AS(
        RestrictedZone(
            "D-103",
            ZoneType::Restricted,
            {
                {0.0, 0.0},
                {0.0, 10.0},
                {10.0, 10.0},
                {10.0, 0.0}
            }),
        std::invalid_argument);
}

TEST_CASE("RestrictedZone handles a concave polygon")
{
    const RestrictedZone zone(
        "D-104",
        ZoneType::Temporary,
        {
            {0.0, 0.0},
            {0.0, 10.0},
            {4.0, 10.0},
            {4.0, 4.0},
            {10.0, 4.0},
            {10.0, 0.0},
            {0.0, 0.0}
        });

    REQUIRE(zone.containsPosition({2.0, 8.0}));       // Inside the left arm.
    REQUIRE(zone.containsPosition({8.0, 2.0}));       // Inside the bottom arm.
    REQUIRE_FALSE(zone.containsPosition({8.0, 8.0})); // In the concave cutout.
}

TEST_CASE("RestrictedZone rejects fewer than three vertices")
{
    REQUIRE_THROWS_AS(
        RestrictedZone(
            "invalid",
            ZoneType::Danger,
            {
                {0.0, 0.0},
                {0.0, 10.0}
            }),
        std::invalid_argument);
}

