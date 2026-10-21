#include "compute/VektorEngine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
Track makeTrack(Position p = {60.02, 4.60}, double speed = 360.0)
{
    return Track(
        "SIM-0", "2026-10-21T12:00:00Z", p,
        10000.0, speed, 0.0, 90.0, 90.0);
}

std::vector<Position> rectangle()
{
    return {
        {59.90, 4.90},
        {59.90, 5.30},
        {60.10, 5.30},
        {60.10, 4.90},
        {59.90, 4.90}
    };
}
}

TEST_CASE("Vektor chooses the SRS northern detour")
{
    const auto track = makeTrack();
    const Position destination{60.02, 5.60};

    const auto result = VektorEngine{}.calculateDetour(
        track,
        destination,
        Zone("D-101", ZoneType::Danger, rectangle()));

    REQUIRE(result);
    REQUIRE(result->waypoints.size() == 2);

    REQUIRE(result->waypoints[0].latDeg == Catch::Approx(60.10));
    REQUIRE(result->waypoints[0].lonDeg == Catch::Approx(4.90));
    REQUIRE(result->waypoints[1].latDeg == Catch::Approx(60.10));
    REQUIRE(result->waypoints[1].lonDeg == Catch::Approx(5.30));

    // The printed SRS example rounds latitude to 60 degrees.
    // VD-FUN-021 requires using the actual track latitude: 60.02.
    const double scale =
        111.32 * std::cos(60.02 * std::acos(-1.0) / 180.0);

    const double expected =
        2.0 * std::hypot(0.30 * scale, 0.08 * 111.32)
        + 0.40 * scale
        - 1.00 * scale;

    REQUIRE(result->extraDistanceKm == Catch::Approx(expected));
    REQUIRE(result->extraDistanceKm ==
            Catch::Approx(4.45).margin(0.02));

    // 360 knots = 0.1852 km/s.
    REQUIRE(result->extraTimeSeconds ==
            Catch::Approx(expected / 0.1852));
    REQUIRE(result->extraTimeSeconds ==
            Catch::Approx(24.0).margin(0.2));

    // Independently check every segment against this rectangle.
    // Each must remain wholly west, east, or north of its interior.
    auto route = result->waypoints;
    route.insert(route.begin(), track.getPosition());
    route.push_back(destination);

    for (std::size_t i = 1; i < route.size(); ++i)
    {
        const Position a = route[i - 1];
        const Position b = route[i];

        const bool exterior =
            std::max(a.lonDeg, b.lonDeg) <= 4.90 ||
            std::min(a.lonDeg, b.lonDeg) >= 5.30 ||
            std::min(a.latDeg, b.latDeg) >= 60.10;

        REQUIRE(exterior);
    }
}

TEST_CASE("Vektor also chooses the shorter southern side")
{
    const auto result = VektorEngine{}.calculateDetour(
        makeTrack({59.98, 4.60}),
        {59.98, 5.60},
        Zone("D-101", ZoneType::Danger, rectangle()));

    REQUIRE(result);
    REQUIRE(result->waypoints.size() == 2);
    REQUIRE(result->waypoints[0].latDeg == Catch::Approx(59.90));
    REQUIRE(result->waypoints[1].latDeg == Catch::Approx(59.90));
}

TEST_CASE("Vektor normalizes ring direction and concave polygons")
{
    auto vertices = rectangle();

    SECTION("Clockwise ring")
    {
        std::reverse(vertices.begin(), vertices.end());
    }

    SECTION("Collinear and duplicate vertices")
    {
        vertices.insert(vertices.begin() + 1, {59.90, 5.10});
        vertices.insert(vertices.begin() + 1, vertices.front());
    }

    SECTION("Concave northern indentation")
    {
        vertices.insert(vertices.begin() + 3, {60.04, 5.10});
    }

    const auto result = VektorEngine{}.calculateDetour(
        makeTrack(),
        {60.02, 5.60},
        Zone("D-101", ZoneType::Danger, vertices));

    REQUIRE(result);
    REQUIRE(result->waypoints.size() == 2);
    REQUIRE(result->waypoints[0].latDeg == Catch::Approx(60.10));
    REQUIRE(result->waypoints[1].latDeg == Catch::Approx(60.10));
}

TEST_CASE("Vektor returns no unusable or unnecessary detour")
{
    Position start{60.02, 4.60};
    Position destination{60.02, 5.60};
    double speed = 360.0;
    auto type = ZoneType::Danger;
    auto polygon = rectangle();

    SECTION("Near miss")
    {
        start.latDeg = destination.latDeg = 60.20;
    }

    SECTION("Along boundary")
    {
        start.latDeg = destination.latDeg = 60.10;
    }

    SECTION("Touch one vertex")
    {
        start = {60.00, 4.80};
        destination = {60.20, 5.00};
    }

    SECTION("Start inside")
    {
        start = {60.02, 5.00};
    }

    SECTION("Destination inside")
    {
        destination = {60.02, 5.00};
    }

    SECTION("Zero speed")
    {
        speed = 0.0;
    }

    SECTION("Negative speed")
    {
        speed = -1.0;
    }

    SECTION("NaN speed")
    {
        speed = std::numeric_limits<double>::quiet_NaN();
    }

    SECTION("Invalid coordinates")
    {
        destination.latDeg = std::numeric_limits<double>::infinity();
    }

    SECTION("Disabled area")
    {
        type = ZoneType::Disabled;
    }

    SECTION("Degenerate polygon")
    {
        polygon = {
            {60.0, 4.9},
            {60.0, 5.1},
            {60.0, 5.3},
            {60.0, 4.9}
        };
    }

    SECTION("Inside concave hull but direct path misses the actual area")
    {
        polygon = {
            {59.9, 4.9},
            {59.9, 5.3},
            {60.1, 5.3},
            {60.0, 5.1},
            {60.1, 4.9},
            {59.9, 4.9}
        };

        start = {60.08, 5.08};
        destination = {60.08, 5.12};
    }

    REQUIRE_FALSE(VektorEngine{}.calculateDetour(
        makeTrack(start, speed),
        destination,
        Zone("D-101", type, polygon)));
}