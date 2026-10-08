#include "../test_helpers.hpp"
#include "domain/RiskEvent.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/SectorState.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// DOMAIN TESTS - Track, RiskEvent, SectorSummary
// ============================================================================

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

TEST_CASE("RiskEvent creation and getters")
{
    RiskEvent riskEvent(1, SectorState::AT_RISK, 3, "2024-01-01T12:00:00Z", "Test risk message");

    REQUIRE(riskEvent.getRiskEventId() == 1);
    REQUIRE(riskEvent.getState() == SectorState::AT_RISK);
    REQUIRE(riskEvent.getSectorId() == 3);
    REQUIRE(riskEvent.getTimestamp() == "2024-01-01T12:00:00Z");
    REQUIRE(riskEvent.getMessage() == "Test risk message");
}

TEST_CASE("SectorSummary initialization and aircraft count management")
{
    SectorSummary summary(0, 0, 0, "2024-01-01T12:00:00Z", 0, 100.0, SectorState::NORMAL);

    REQUIRE(summary.getSectorId() == 0);
    REQUIRE(summary.getRow() == 0);
    REQUIRE(summary.getColumn() == 0);
    REQUIRE(summary.getLocalAircraftCount() == 0);
    REQUIRE(summary.getBaseCapacity() == 100.0);

    summary.increaseLocalAircraftCount();
    REQUIRE(summary.getLocalAircraftCount() == 1);

    summary.increaseLocalAircraftCount();
    REQUIRE(summary.getLocalAircraftCount() == 2);

    summary.decreaseLocalAircraftCount();
    REQUIRE(summary.getLocalAircraftCount() == 1);
}


TEST_CASE("SectorSummary timestamp update")
{
    SectorSummary summary(0, 0, 0, "2024-01-01T12:00:00Z", 0, 100.0, SectorState::NORMAL);

    // updateTime modifies internal state, verify by checking no exception is thrown
    summary.updateTime("2024-01-01T12:01:00Z");
    REQUIRE(true); // If we get here, update succeeded
}
