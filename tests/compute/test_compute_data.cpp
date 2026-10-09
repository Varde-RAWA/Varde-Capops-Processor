#include "../test_helpers.hpp"
#include "compute/ComputeData.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// COMPUTEDATA TESTS - Main processing logic
// ============================================================================

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

    Position pos{59.5, 5.0};
    Track track("ABC123", "2024-01-01T12:00:00Z", pos, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(track);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == 1);
    REQUIRE(result.tracks[0].getIcao() == "ABC123");
}

TEST_CASE("ComputeData handles multiple track updates")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    Position pos1{59.5, 5.0};
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1, 10000.0, 450.0, 0.0, 180.0, 175.0);

    Position pos2{59.6, 5.1};
    Track track2("DEF456", "2024-01-01T12:00:00Z", pos2, 8000.0, 400.0, -100.0, 270.0, 265.0);

    computeData.handleTrackUpdate(track1);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == 2);
}

TEST_CASE("ComputeData handles track update to same aircraft")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);

    Position pos1{59.5, 5.0};
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1, 10000.0, 450.0, 0.0, 180.0, 175.0);
    computeData.handleTrackUpdate(track1);

    Position pos2{59.51, 5.01};
    Track track2("ABC123", "2024-01-01T12:00:01Z", pos2, 10050.0, 450.0, 50.0, 180.0, 175.0);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result = computeData.collectProcessingResult();
    REQUIRE(result.tracks.size() == 1);
    REQUIRE(result.tracks[0].getTimestamp() == "2024-01-01T12:00:01Z");
    REQUIRE(result.tracks[0].getAltitudeFeet() == 10050.0);
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

    Position pos1 = grid.sectorCenter(0);
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1, 10000.0, 450.0, 0.0, 180.0, 175.0);
    computeData.handleTrackUpdate(track1);

    ProcessingResult result1 = computeData.collectProcessingResult();
    REQUIRE(result1.tracks.size() == 1);

    Position pos2 = grid.sectorCenter(5);
    Track track2("ABC123", "2024-01-01T12:00:01Z", pos2, 10050.0, 450.0, 0.0, 180.0, 175.0);
    computeData.handleTrackUpdate(track2);

    ProcessingResult result2 = computeData.collectProcessingResult();

    REQUIRE(result2.tracks.size() == 1);
    REQUIRE(result2.tracks[0].getPosition().latDeg == pos2.latDeg);

    Position pos3{10, 10};
    Track track3("ABC123", "2024-01-01T12:00:01Z", pos3, 10050.0, 450.0, 0.0, 180.0, 175.0);
    computeData.handleTrackUpdate(track3);

    ProcessingResult result3 = computeData.collectProcessingResult();

    REQUIRE(result3.tracks.size() == 0);
}

TEST_CASE("handleTrackUpdate replaces an aircraft snapshot without duplicating it")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    Position firstPosition = grid.sectorCenter(0);
    Position secondPosition{
        firstPosition.latDeg + 0.01,
        firstPosition.lonDeg + 0.01
    };

    REQUIRE(grid.determineSector(firstPosition) == 0);
    REQUIRE(grid.determineSector(secondPosition) == 0);

    Track first("ABC123", "2024-01-01T12:00:00Z",
                firstPosition, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(first);

    Track updated("ABC123", "2024-01-01T12:00:01Z",
                  secondPosition, 10050.0, 450.0, 50.0, 180.0, 175.0);

    computeData.handleTrackUpdate(updated);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 1);
    REQUIRE(result.tracks[0].getIcao() == "ABC123");
    REQUIRE(result.tracks[0].getTimestamp() == "2024-01-01T12:00:01Z");
    REQUIRE(result.tracks[0].getAltitudeFeet() == 10050.0);
    REQUIRE(result.tracks[0].getPosition().latDeg == secondPosition.latDeg);
    REQUIRE(result.tracks[0].getPosition().lonDeg == secondPosition.lonDeg);
}

TEST_CASE("handleTrackUpdate ignores older and equal-timestamp snapshots")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    Position originalPosition = grid.sectorCenter(0);
    Position changedPosition = grid.sectorCenter(1);

    Track original("ABC123", "2024-01-01T12:00:01Z",
                   originalPosition, 10000.0, 450.0, 0.0, 180.0, 175.0);

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
                   changedPosition, 20000.0, 500.0, 100.0, 90.0, 90.0);

    computeData.handleTrackUpdate(incoming);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 1);

    const Track &stored = result.tracks[0];
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

    Position firstPosition = grid.sectorCenter(0);
    Position secondPosition = grid.sectorCenter(5);

    Track first("ABC123", "2024-01-01T12:00:00Z",
                firstPosition, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(first);

    ProcessingResult before = computeData.collectProcessingResult();
    REQUIRE(before.tracks.size() == 1);
    REQUIRE(before.tracks[0].getIcao() == "ABC123");

    // Move the same aircraft to sector 5.
    Track moved("ABC123", "2024-01-01T12:00:01Z",
                secondPosition, 10050.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(moved);

    ProcessingResult after = computeData.collectProcessingResult();

    REQUIRE(after.tracks.size() == 1);
    REQUIRE(after.tracks[0].getIcao() == "ABC123");
    REQUIRE(after.tracks[0].getTimestamp() == moved.getTimestamp());
    REQUIRE(after.tracks[0].getPosition().latDeg == secondPosition.latDeg);
    REQUIRE(after.tracks[0].getPosition().lonDeg == secondPosition.lonDeg);
}

TEST_CASE("handleTrackUpdate removes aircraft outside the grid and restores them on re-entry")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    Position inside = grid.sectorCenter(5);
    Position outside{10.0, 10.0};

    REQUIRE(grid.determineSector(outside) == -1);

    Track initial("ABC123", "2024-01-01T12:00:00Z",
                  inside, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(initial);

    ProcessingResult before = computeData.collectProcessingResult();
    REQUIRE(before.tracks.size() == 1);

    Track departed("ABC123", "2024-01-01T12:00:01Z",
                   outside, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(departed);

    ProcessingResult afterDeparture = computeData.collectProcessingResult();
    REQUIRE(afterDeparture.tracks.empty());

    Track stillOutside("ABC123", "2024-01-01T12:00:02Z",
                       outside, 10000.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(stillOutside);

    ProcessingResult absent = computeData.collectProcessingResult();
    REQUIRE(absent.tracks.empty());

    Track returned("ABC123", "2024-01-01T12:00:03Z",
                   inside, 10500.0, 450.0, 0.0, 180.0, 175.0);

    computeData.handleTrackUpdate(returned);

    ProcessingResult afterReturn = computeData.collectProcessingResult();

    REQUIRE(afterReturn.tracks.size() == 1);
    REQUIRE(afterReturn.tracks[0].getIcao() == "ABC123");
    REQUIRE(afterReturn.tracks[0].getTimestamp() == returned.getTimestamp());
    REQUIRE(afterReturn.tracks[0].getPosition().latDeg == inside.latDeg);
    REQUIRE(afterReturn.tracks[0].getPosition().lonDeg == inside.lonDeg);
    REQUIRE(afterReturn.tracks[0].getAltitudeFeet() == 10500.0);
}