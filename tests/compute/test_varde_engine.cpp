#include "compute/VardeEngine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <stdexcept>

namespace
{
// Comparison allowances: about 1.1 cm in latitude and 10 microseconds.
constexpr double positionToleranceDegrees = 1e-7;
constexpr double entryTimeToleranceSeconds = 1e-5;

// Only supplies the ID, timestamp and unrelated altitude/vertical-speed fields.
// Each test supplies its own position, speed in knots and direction in degrees.
Track makePredictionTrack(
    Position position,
    double speedKnots,
    double groundTrackDegrees)
{
    return Track(
        "SIM-0",
        "2026-10-05T12:00:00Z",
        position,
        15000.0, // Altitude in feet; the zones are two-dimensional.
        speedKnots,
        0.0, // No vertical movement.
        groundTrackDegrees,
        groundTrackDegrees
    );
}
}

TEST_CASE("Varde projects an eastward aircraft 120 seconds ahead")
{
    const Position startPosition{60.0, 4.6};
    constexpr double speedKnots = 360.0;
    constexpr double predictionSeconds = 120.0;

    const Track track(
        "SIM-0",
        "2026-10-05T12:00:00Z",
        startPosition,
        15000.0, // Altitude in feet.
        speedKnots,
        0.0,     // Vertical speed.
        90.0,    // Heading.
        90.0     // Ground-track direction.
    );

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, predictionSeconds);

    // At latitude 60°, one longitude degree is 55.66 km.
    constexpr double kilometresPerLongitudeDegree = 55.66;
    constexpr double kilometresPerNauticalMile = 1.852;
    constexpr double secondsPerHour = 3600.0;

    // 360 knots for 120 seconds gives 22.224 km.
    const double distanceKm =
        speedKnots * kilometresPerNauticalMile * predictionSeconds / secondsPerHour;

    const double expectedLongitude =
        startPosition.lonDeg + distanceKm / kilometresPerLongitudeDegree;

    REQUIRE(predicted.latDeg ==
            Catch::Approx(startPosition.latDeg).margin(positionToleranceDegrees));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(expectedLongitude).margin(positionToleranceDegrees));
}

TEST_CASE("Varde predicts movement in all four directions")
{
    const Position startPosition{60.0, 4.6};
    const double speedKnots = 360.0;
    const double predictionSeconds = 120.0;

    // One knot is 1.852 km/hour; divide by 3600 for km/second.
    const double distanceKm = speedKnots * 1.852 / 3600.0 * predictionSeconds;
    // VD-FUN-021: 111.32 km per latitude degree; at 60 N, 55.66 km per longitude degree.
    const double kilometresPerLatitudeDegree = 111.32;
    const double kilometresPerLongitudeDegree = 55.66;
    const double latitudeChange = distanceKm / kilometresPerLatitudeDegree;
    const double longitudeChange = distanceKm / kilometresPerLongitudeDegree;

    double direction = 0.0;
    Position expected = startPosition;

    SECTION("North")
    {
        direction = 0.0;
        expected.latDeg += latitudeChange;
    }

    SECTION("East")
    {
        direction = 90.0;
        expected.lonDeg += longitudeChange;
    }

    SECTION("South")
    {
        direction = 180.0;
        expected.latDeg -= latitudeChange;
    }

    SECTION("West")
    {
        direction = 270.0;
        expected.lonDeg -= longitudeChange;
    }

    const Track track =
        makePredictionTrack(startPosition, speedKnots, direction);

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, predictionSeconds);

    REQUIRE(predicted.latDeg ==
            Catch::Approx(expected.latDeg).margin(positionToleranceDegrees));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(expected.lonDeg).margin(positionToleranceDegrees));
}

TEST_CASE("A stationary aircraft stays at its position")
{
    const Position startPosition{60.0, 4.6};
    const Track track =
        makePredictionTrack(startPosition, 0.0, 45.0);

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, 120.0);

    REQUIRE(predicted.latDeg == Catch::Approx(startPosition.latDeg).margin(positionToleranceDegrees));
    REQUIRE(predicted.lonDeg == Catch::Approx(startPosition.lonDeg).margin(positionToleranceDegrees));
}

TEST_CASE("Varde accounts for latitude when moving east")
{
    const double speedKnots = 360.0;
    const double predictionSeconds = 120.0;
    double latitude = 0.0;

    SECTION("At the equator")
    {
        latitude = 0.0;
    }

    SECTION("At 60 degrees north")
    {
        latitude = 60.0;
    }

    SECTION("Near the North Pole at 80 degrees north")
    {
        latitude = 80.0;
    }

    const Track track =
        makePredictionTrack({latitude, 0.0}, speedKnots, 90.0);

    // Convert knots to km/second, then multiply by the prediction duration.
    const double distanceKm = speedKnots * 1.852 / 3600.0 * predictionSeconds;
    // VD-FUN-021: longitude degrees cover fewer kilometres at higher latitudes.
    const double kilometresPerLatitudeDegree = 111.32;
    const double radiansPerDegree = std::acos(-1.0) / 180.0;
    const double kilometresPerLongitudeDegree =
        kilometresPerLatitudeDegree * std::cos(latitude * radiansPerDegree);
    const double expectedLongitude = distanceKm / kilometresPerLongitudeDegree;

    const VardeEngine varde;
    const Position predicted = varde.predictPosition(track, predictionSeconds);

    REQUIRE(predicted.latDeg ==
            Catch::Approx(latitude).margin(positionToleranceDegrees));

    REQUIRE(predicted.lonDeg ==
            Catch::Approx(expectedLongitude).margin(positionToleranceDegrees));
}

TEST_CASE("Varde rejects negative prediction time")
{
    const Track track =
        makePredictionTrack({60.0, 4.6}, 360.0, 90.0);

    const VardeEngine varde;

    REQUIRE_THROWS_AS(
        varde.predictPosition(track, -1.0),
        std::invalid_argument
    );
}


TEST_CASE("Varde finds enabled zones containing an aircraft")
{
    // SRS zone D-101: latitude 59.95–60.05 N, longitude 5.00–5.20 E.
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

TEST_CASE("Varde returns no matches when there are no zones")
{
    const Track track =
        makePredictionTrack({60.0, 5.1}, 360.0, 90.0);

    const VardeEngine varde;

    REQUIRE(varde.getZonesContainingTrack(track, {}).empty());
}


TEST_CASE("Varde calculates the SRS warning levels")
{
    // VD-FUN-022's worked example: aircraft flies east at 360 knots and latitude 60 N.
    const double speedKnots = 360.0;
    const double speedKmPerSecond = speedKnots * 1.852 / 3600.0;
    const double kilometresPerLongitudeDegree = 55.66; // 111.32 * cos(60 degrees).
    const double zoneWestLongitude = 5.00;

    // D-101 is a closed rectangle: latitude 59.95–60.05 N, longitude 5.00–5.20 E.
    const Zone zone(
        "D-101",
        ZoneType::Danger,
        {
            {59.95, 5.00},
            {59.95, 5.20},
            {60.05, 5.20},
            {60.05, 5.00},
            {59.95, 5.00}
        });

    Position position{60.0, 4.6};
    WarningLevel expectedLevel = WarningLevel::NONE;
    std::optional<double> expectedTime;

    SECTION("Entry is beyond 120 seconds")
    {
        position = {60.0, 4.6};
    }

    SECTION("Entry is just within 120 seconds")
    {
        position = {60.0, 4.6017};
        expectedLevel = WarningLevel::PREDICTED;
        expectedTime = (zoneWestLongitude - position.lonDeg) *
            kilometresPerLongitudeDegree / speedKmPerSecond;
    }

    SECTION("Entry is beyond 30 seconds")
    {
        position = {60.0, 4.89};
        expectedLevel = WarningLevel::PREDICTED;
        expectedTime = (zoneWestLongitude - position.lonDeg) *
            kilometresPerLongitudeDegree / speedKmPerSecond;
    }

    SECTION("Entry is within 30 seconds")
    {
        position = {60.0, 4.91};
        expectedLevel = WarningLevel::IMMINENT;
        expectedTime = (zoneWestLongitude - position.lonDeg) *
            kilometresPerLongitudeDegree / speedKmPerSecond;
    }

    SECTION("Aircraft is already inside")
    {
        position = {60.0, 5.01};
        expectedLevel = WarningLevel::INFRINGEMENT;
    }

    SECTION("Aircraft passes north of the zone")
    {
        position = {60.1, 4.91};
    }

    const Track track = makePredictionTrack(position, speedKnots, 90.0);
    const VardeEngine varde;
    const auto results = varde.evaluateTrack(track, {zone});

    REQUIRE(results.size() == 1);
    REQUIRE(results[0].getIcao() == track.getIcao());
    REQUIRE(results[0].getZoneId() == "D-101");
    REQUIRE(results[0].getLevel() == expectedLevel);

    const auto actualTime = results[0].getTimeToEntrySeconds();

    if (expectedTime)
    {
        REQUIRE(actualTime.has_value());
        REQUIRE(*actualTime ==
                Catch::Approx(*expectedTime).margin(entryTimeToleranceSeconds));
    }
    else
    {
        REQUIRE_FALSE(actualTime.has_value());
    }
}


TEST_CASE("Varde detects a path crossing a zone with both endpoints outside")
{
    // Closed rectangle from SRS D-101: west edge 5.00 E, east edge 5.20 E.
    const Zone zone(
        "D-101",
        ZoneType::Danger,
        {
            {59.95, 5.00},
            {59.95, 5.20},
            {60.05, 5.20},
            {60.05, 5.00},
            {59.95, 5.00}
        });

    const Position startPosition{60.0, 4.91};
    const double speedKnots = 360.0;
    const Track track = makePredictionTrack(startPosition, speedKnots, 90.0);
    const VardeEngine varde;

    const Position end = varde.predictPosition(track, 120.0);
    REQUIRE_FALSE(zone.containsPosition(track.getPosition()));
    REQUIRE_FALSE(zone.containsPosition(end));

    const auto results = varde.evaluateTrack(track, {zone});

    REQUIRE(results.size() == 1);
    REQUIRE(results[0].getZoneId() == "D-101");
    REQUIRE(results[0].getLevel() == WarningLevel::IMMINENT);
    REQUIRE(results[0].getTimeToEntrySeconds().has_value());
    // Expected time to the western edge = distance in km / speed in km/second.
    const double zoneWestLongitude = 5.00;
    const double kilometresPerLongitudeDegree = 55.66; // At latitude 60 N.
    const double speedKmPerSecond = speedKnots * 1.852 / 3600.0;
    const double expectedEntryTimeSeconds = (zoneWestLongitude - startPosition.lonDeg) *
        kilometresPerLongitudeDegree / speedKmPerSecond;

    REQUIRE(*results[0].getTimeToEntrySeconds() ==
            Catch::Approx(expectedEntryTimeSeconds).margin(entryTimeToleranceSeconds));
}

TEST_CASE("Varde evaluates enabled zones separately")
{
    // Aircraft at 4.91 E approaches D-101, whose western edge is 5.00 E.
    const std::vector<Position> approachingZone{
        {59.95, 5.00},
        {59.95, 5.20},
        {60.05, 5.20},
        {60.05, 5.00},
        {59.95, 5.00}
    };

    // This rectangle surrounds the aircraft: longitude 4.85–4.95 E.
    const std::vector<Position> currentZone{
        {59.95, 4.85},
        {59.95, 4.95},
        {60.05, 4.95},
        {60.05, 4.85},
        {59.95, 4.85}
    };

    const std::vector<Zone> zones{
        Zone("D-101", ZoneType::Danger, approachingZone),
        Zone("R-201", ZoneType::Restricted, currentZone),
        Zone("T-301", ZoneType::Disabled, currentZone)
    };

    const Track track =
        makePredictionTrack({60.0, 4.91}, 360.0, 90.0);
    const VardeEngine varde;

    const auto results = varde.evaluateTrack(track, zones);

    REQUIRE(results.size() == 2); // Disabled zone is skipped.

    REQUIRE(results[0].getIcao() == track.getIcao());
    REQUIRE(results[0].getZoneId() == "D-101");
    REQUIRE(results[0].getLevel() == WarningLevel::IMMINENT);
    REQUIRE(results[0].getTimeToEntrySeconds().has_value());

    REQUIRE(results[1].getIcao() == track.getIcao());
    REQUIRE(results[1].getZoneId() == "R-201");
    REQUIRE(results[1].getLevel() == WarningLevel::INFRINGEMENT);
    REQUIRE_FALSE(results[1].getTimeToEntrySeconds().has_value());
}

