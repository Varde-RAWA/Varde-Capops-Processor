#include "../test_helpers.hpp"
#include "compute/ComputeData.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

// ============================================================================
// COMPUTEDATA TESTS - Main processing logic
// ============================================================================

namespace
{
constexpr double altitudeFeet = 10000.0;
constexpr double groundSpeedKnots = 450.0;
constexpr double verticalSpeedFeetPerMinute = 0.0;
constexpr double headingDegrees = 180.0;
constexpr double groundTrackDegrees = 175.0;
constexpr double updatedAltitudeFeet = 10050.0;
constexpr double climbRateFeetPerMinute = 50.0;

constexpr Position initialPosition{59.5, 5.0};
constexpr Position nearbyPosition{59.51, 5.01};
constexpr Position outsidePosition{10.0, 10.0};
constexpr int initialSectorId = 0;
constexpr int destinationSectorId = 5;
constexpr std::size_t singleAircraftCount = 1;
} // namespace

TEST_CASE("ComputeData initialization without aircraft")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    // Initially, there should be no tracks
    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.empty());
}

TEST_CASE("ComputeData handles track update")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    Track track("ABC123", "2024-01-01T12:00:00Z", initialPosition,
                altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(track);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == singleAircraftCount);
    REQUIRE(result.tracks.front().getIcao() == "ABC123");
}

TEST_CASE("ComputeData handles multiple track updates")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    constexpr Position secondPosition{59.6, 5.1};
    constexpr double secondAltitudeFeet = 8000.0;
    constexpr double secondGroundSpeedKnots = 400.0;
    constexpr double descentRateFeetPerMinute = -100.0;
    constexpr double secondHeadingDegrees = 270.0;
    constexpr double secondGroundTrackDegrees = 265.0;
    constexpr std::size_t expectedAircraftCount = 2;

    Track track1("ABC123", "2024-01-01T12:00:00Z", initialPosition,
                 altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                 headingDegrees, groundTrackDegrees);

    Track track2("DEF456", "2024-01-01T12:00:00Z", secondPosition,
                 secondAltitudeFeet, secondGroundSpeedKnots, descentRateFeetPerMinute,
                 secondHeadingDegrees, secondGroundTrackDegrees);

    computeData.handleTrackUpdate(track1);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == expectedAircraftCount);
}

TEST_CASE("ComputeData handles track update to same aircraft")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    Track track1("ABC123", "2024-01-01T12:00:00Z", initialPosition,
                 altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                 headingDegrees, groundTrackDegrees);
    computeData.handleTrackUpdate(track1);

    Track track2("ABC123", "2024-01-01T12:00:01Z", nearbyPosition,
                 updatedAltitudeFeet, groundSpeedKnots, climbRateFeetPerMinute,
                 headingDegrees, groundTrackDegrees);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == singleAircraftCount);
    REQUIRE(result.tracks.front().getTimestamp() == "2024-01-01T12:00:01Z");
    REQUIRE(result.tracks.front().getAltitudeFeet() == updatedAltitudeFeet);
}


TEST_CASE("Empty processing result on startup")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.empty());
}

TEST_CASE("Track removal when aircraft leaves grid")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    Position pos1 = grid.sectorCenter(initialSectorId);
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1,
                 altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                 headingDegrees, groundTrackDegrees);
    computeData.handleTrackUpdate(track1);

    ProcessingResult result1 = computeData.collectProcessingResult();
    REQUIRE(result1.tracks.size() == singleAircraftCount);

    Position pos2 = grid.sectorCenter(destinationSectorId);
    Track track2("ABC123", "2024-01-01T12:00:01Z", pos2,
                 updatedAltitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                 headingDegrees, groundTrackDegrees);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result2 = computeData.collectProcessingResult();

    REQUIRE(result2.tracks.size() == singleAircraftCount);
    REQUIRE(result2.tracks.front().getPosition().latDeg == pos2.latDeg);

    Track track3("ABC123", "2024-01-01T12:00:01Z", outsidePosition,
                 updatedAltitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                 headingDegrees, groundTrackDegrees);
    computeData.handleTrackUpdate(track3);

    ProcessingResult result3 = computeData.collectProcessingResult();

    REQUIRE(result3.tracks.empty());
}

TEST_CASE("handleTrackUpdate replaces an aircraft snapshot without duplicating it")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    constexpr double withinSectorOffsetDegrees = 0.01;
    Position firstPosition = grid.sectorCenter(initialSectorId);
    Position secondPosition{
        firstPosition.latDeg + withinSectorOffsetDegrees,
        firstPosition.lonDeg + withinSectorOffsetDegrees
    };

    REQUIRE(grid.determineSector(firstPosition) == initialSectorId);
    REQUIRE(grid.determineSector(secondPosition) == initialSectorId);

    Track first("ABC123", "2024-01-01T12:00:00Z",
                firstPosition, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(first);

    Track updated("ABC123", "2024-01-01T12:00:01Z",
                  secondPosition, updatedAltitudeFeet, groundSpeedKnots, climbRateFeetPerMinute,
                  headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(updated);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == singleAircraftCount);
    REQUIRE(result.tracks.front().getIcao() == "ABC123");
    REQUIRE(result.tracks.front().getTimestamp() == "2024-01-01T12:00:01Z");
    REQUIRE(result.tracks.front().getAltitudeFeet() == updatedAltitudeFeet);
    REQUIRE(result.tracks.front().getPosition().latDeg == secondPosition.latDeg);
    REQUIRE(result.tracks.front().getPosition().lonDeg == secondPosition.lonDeg);
}

TEST_CASE("handleTrackUpdate ignores older and equal-timestamp snapshots")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    constexpr int changedSectorId = 1;
    constexpr double incomingAltitudeFeet = 20000.0;
    constexpr double incomingGroundSpeedKnots = 500.0;
    constexpr double incomingVerticalSpeedFeetPerMinute = 100.0;
    constexpr double incomingHeadingDegrees = 90.0;
    constexpr double incomingGroundTrackDegrees = 90.0;

    Position originalPosition = grid.sectorCenter(initialSectorId);
    Position changedPosition = grid.sectorCenter(changedSectorId);

    Track original("ABC123", "2024-01-01T12:00:01Z",
                   originalPosition, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                   headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(original);

    std::string timestamp;

    SECTION("Older snapshot")
    {
        timestamp = "2024-01-01T12:00:00Z";
    }

    SECTION("Equal-timestamp snapshot")
    {
        timestamp = "2024-01-01T12:00:01Z";
    }

    Track incoming("ABC123", timestamp,
                   changedPosition, incomingAltitudeFeet, incomingGroundSpeedKnots,
                   incomingVerticalSpeedFeetPerMinute, incomingHeadingDegrees,
                   incomingGroundTrackDegrees);

    computeData.handleTrackUpdate(incoming);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == singleAircraftCount);

    const Track &stored = result.tracks.front();
    REQUIRE(stored.getIcao() == original.getIcao());
    REQUIRE(stored.getTimestamp() == original.getTimestamp());
    REQUIRE(stored.getPosition().latDeg == originalPosition.latDeg);
    REQUIRE(stored.getPosition().lonDeg == originalPosition.lonDeg);
    REQUIRE(stored.getAltitudeFeet() == original.getAltitudeFeet());
    REQUIRE(stored.getGroundSpeedKnots() == original.getGroundSpeedKnots());
    REQUIRE(stored.getVerticalSpeedFeetPerMinute() ==
            original.getVerticalSpeedFeetPerMinute());
    REQUIRE(stored.getHeadingDegrees() == original.getHeadingDegrees());
    REQUIRE(stored.getGroundTrackDegrees() == original.getGroundTrackDegrees());
}

TEST_CASE("handleTrackUpdate updates aircraft position within the region")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    Position firstPosition = grid.sectorCenter(initialSectorId);
    Position secondPosition = grid.sectorCenter(destinationSectorId);

    Track first("ABC123", "2024-01-01T12:00:00Z",
                firstPosition, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(first);

    ProcessingResult before = computeData.collectProcessingResult();
    REQUIRE(before.tracks.size() == singleAircraftCount);
    REQUIRE(before.tracks.front().getIcao() == "ABC123");

    // Move the same aircraft to the destination sector.
    Track moved("ABC123", "2024-01-01T12:00:01Z",
                secondPosition, updatedAltitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(moved);

    ProcessingResult after = computeData.collectProcessingResult();

    REQUIRE(after.tracks.size() == singleAircraftCount);
    REQUIRE(after.tracks.front().getIcao() == "ABC123");
    REQUIRE(after.tracks.front().getTimestamp() == moved.getTimestamp());
    REQUIRE(after.tracks.front().getPosition().latDeg == secondPosition.latDeg);
    REQUIRE(after.tracks.front().getPosition().lonDeg == secondPosition.lonDeg);
}

TEST_CASE("handleTrackUpdate removes aircraft outside the grid and restores them on re-entry")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    constexpr int outsideGridSectorId = -1;
    constexpr double returnedAltitudeFeet = 10500.0;
    Position inside = grid.sectorCenter(destinationSectorId);

    REQUIRE(grid.determineSector(outsidePosition) == outsideGridSectorId);

    Track initial("ABC123", "2024-01-01T12:00:00Z",
                  inside, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                  headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(initial);

    ProcessingResult before = computeData.collectProcessingResult();
    REQUIRE(before.tracks.size() == singleAircraftCount);

    Track departed("ABC123", "2024-01-01T12:00:01Z",
                   outsidePosition, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                   headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(departed);

    ProcessingResult afterDeparture = computeData.collectProcessingResult();
    REQUIRE(afterDeparture.tracks.empty());

    Track stillOutside("ABC123", "2024-01-01T12:00:02Z",
                       outsidePosition, altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                       headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(stillOutside);

    ProcessingResult absent = computeData.collectProcessingResult();
    REQUIRE(absent.tracks.empty());

    Track returned("ABC123", "2024-01-01T12:00:03Z",
                   inside, returnedAltitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
                   headingDegrees, groundTrackDegrees);

    computeData.handleTrackUpdate(returned);

    ProcessingResult afterReturn = computeData.collectProcessingResult();

    REQUIRE(afterReturn.tracks.size() == singleAircraftCount);
    REQUIRE(afterReturn.tracks.front().getIcao() == "ABC123");
    REQUIRE(afterReturn.tracks.front().getTimestamp() == returned.getTimestamp());
    REQUIRE(afterReturn.tracks.front().getPosition().latDeg == inside.latDeg);
    REQUIRE(afterReturn.tracks.front().getPosition().lonDeg == inside.lonDeg);
    REQUIRE(afterReturn.tracks.front().getAltitudeFeet() == returnedAltitudeFeet);
}

TEST_CASE("A complete snapshot removes missing aircraft")
{
    Configuration config = createTestConfig();
    ComputeData data(config);
    Grid grid(config.grid());

    constexpr std::size_t initialAircraftCount = 2;
    constexpr std::size_t remainingAircraftCount = 1;

    const std::string missingAircraftIcao = "ABC123";
    const std::string remainingAircraftIcao = "DEF456";
    const std::string timestamp = "2024-01-01T12:00:00Z";
    const Position position = grid.sectorCenter(initialSectorId);

    const Track first(
        missingAircraftIcao, timestamp, position,
        altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
        headingDegrees, groundTrackDegrees);

    const Track second(
        remainingAircraftIcao, timestamp, position,
        altitudeFeet, groundSpeedKnots, verticalSpeedFeetPerMinute,
        headingDegrees, groundTrackDegrees);

    data.handleTrackSnapshot({first, second});
    REQUIRE(data.collectProcessingResult().tracks.size() == initialAircraftCount);

    // First aircraft disappears; second remains, even without a newer timestamp.
    data.handleTrackSnapshot({second});

    const auto remaining = data.collectProcessingResult();
    REQUIRE(remaining.tracks.size() == remainingAircraftCount);
    REQUIRE(remaining.tracks.front().getIcao() == remainingAircraftIcao);

    data.handleTrackSnapshot({});
    REQUIRE(data.collectProcessingResult().tracks.empty());
}
