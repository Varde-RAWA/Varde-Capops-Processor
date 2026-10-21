#include "compute/VektorManager.hpp"

#include <catch2/catch_test_macros.hpp>

namespace
{
struct Fixture
{
    VektorManager manager{[next = 0]() mutable {
        return "proposal-" + std::to_string(++next);
    }};

    Track track{
        "SIM-0", "2026-10-21T12:00:00Z",
        {60.02, 4.60},
        10000.0, 360.0, 0.0, 90.0, 90.0
    };

    Zone area{
        "D-101", ZoneType::Danger,
        {
            {59.90, 4.90},
            {59.90, 5.30},
            {60.10, 5.30},
            {60.10, 4.90},
            {59.90, 4.90}
        }
    };

    AlertEvent alert{
        "alert-1",
        "SIM-0",
        "D-101",
        WarningLevel::PREDICTED,
        90.0,
        AlertEventType::OPENED
    };

    Position destination{60.02, 5.60};

    std::optional<VektorProposal> propose()
    {
        return manager.consider(
            alert, track, true, destination, area);
    }
};
}

TEST_CASE("Vektor creates a proposal for an eligible alert")
{
    Fixture f;

    SECTION("Predicted")
    {
    }

    SECTION("Imminent")
    {
        f.alert.level = WarningLevel::IMMINENT;
        f.alert.timeToEntrySeconds = 20.0;
    }

    const auto proposal = f.propose();

    REQUIRE(proposal);
    REQUIRE(proposal->proposalId == "proposal-1");
    REQUIRE(proposal->alertId == f.alert.alertId);
    REQUIRE(proposal->icao == f.track.getIcao());
    REQUIRE(proposal->zoneId == f.area.getId());
    REQUIRE(proposal->detour.waypoints.size() == 2);

    REQUIRE(f.manager.state(proposal->proposalId) ==
            ProposalState::Proposed);

    // Repeated evaluation must not create another pending proposal.
    REQUIRE_FALSE(f.propose());
}

TEST_CASE("Vektor rejects ineligible proposal requests")
{
    Fixture f;
    bool simulated = true;
    std::optional<Position> destination = f.destination;

    SECTION("Live track")
    {
        simulated = false;
    }

    SECTION("No destination")
    {
        destination.reset();
    }

    SECTION("No warning")
    {
        f.alert.level = WarningLevel::NONE;
    }

    SECTION("Already infringing")
    {
        f.alert.level = WarningLevel::INFRINGEMENT;
    }

    SECTION("Closed alert")
    {
        f.alert.type = AlertEventType::CLOSED;
    }

    SECTION("Different aircraft")
    {
        f.alert.icao = "SIM-1";
    }

    SECTION("Different zone")
    {
        f.alert.zoneId = "D-102";
    }

    REQUIRE_FALSE(f.manager.consider(
        f.alert, f.track, simulated, destination, f.area));
}

TEST_CASE("Vektor can retry after no route was produced")
{
    Fixture f;

    // This destination is west of the area: no detour is needed.
    REQUIRE_FALSE(f.manager.consider(
        f.alert, f.track, true, Position{60.02, 4.70}, f.area));

    // The unsuccessful attempt must not block a later valid proposal.
    REQUIRE(f.propose());
}

TEST_CASE("Vektor approval returns the route only once")
{
    Fixture f;

    const auto proposal = f.propose();
    REQUIRE(proposal);

    const auto approved = f.manager.decide(
        proposal->proposalId, ProposalState::Approved);

    REQUIRE(approved);
    REQUIRE(approved->proposalId == proposal->proposalId);
    REQUIRE(approved->detour.waypoints.size() ==
            proposal->detour.waypoints.size());

    for (std::size_t i = 0; i < approved->detour.waypoints.size(); ++i)
    {
        REQUIRE(approved->detour.waypoints[i].latDeg ==
                proposal->detour.waypoints[i].latDeg);

        REQUIRE(approved->detour.waypoints[i].lonDeg ==
                proposal->detour.waypoints[i].lonDeg);
    }

    REQUIRE(f.manager.state(proposal->proposalId) ==
            ProposalState::Approved);

    REQUIRE_FALSE(f.manager.decide(
        proposal->proposalId, ProposalState::Approved));

    REQUIRE_FALSE(f.manager.decide(
        proposal->proposalId, ProposalState::Rejected));

    REQUIRE(f.manager.state(proposal->proposalId) ==
            ProposalState::Approved);
}

TEST_CASE("Vektor rejection suppresses proposals for the same alert")
{
    Fixture f;

    const auto proposal = f.propose();
    REQUIRE(proposal);

    REQUIRE_FALSE(f.manager.decide(
        proposal->proposalId, ProposalState::Rejected));

    REQUIRE(f.manager.state(proposal->proposalId) ==
            ProposalState::Rejected);

    // Escalation must not cause a new proposal after rejection.
    f.alert.level = WarningLevel::IMMINENT;
    f.alert.type = AlertEventType::LEVEL_CHANGED;

    REQUIRE_FALSE(f.propose());

    REQUIRE_FALSE(f.manager.decide(
        proposal->proposalId, ProposalState::Approved));

    // A later alert episode for the aircraft may receive a proposal.
    f.manager.closeAlert(f.alert.alertId);
    f.alert.alertId = "alert-2";
    f.alert.type = AlertEventType::OPENED;

    REQUIRE(f.propose());
}

TEST_CASE("Vektor expired proposals cannot be approved")
{
    Fixture f;

    const auto proposal = f.propose();
    REQUIRE(proposal);

    SECTION("Alert closes")
    {
        f.manager.closeAlert(f.alert.alertId);
    }

    SECTION("Expiry decision is received")
    {
        REQUIRE_FALSE(f.manager.decide(
            proposal->proposalId, ProposalState::Expired));
    }

    REQUIRE(f.manager.state(proposal->proposalId) ==
            ProposalState::Expired);

    REQUIRE_FALSE(f.manager.decide(
        proposal->proposalId, ProposalState::Approved));
}

TEST_CASE("Vektor supersedes an older pending proposal for the same aircraft")
{
    Fixture f;

    const auto first = f.propose();
    REQUIRE(first);

    // A second alert for another zone affecting the same aircraft.
    f.alert.alertId = "alert-2";
    f.alert.zoneId = "D-102";

    const Zone secondArea(
        "D-102", ZoneType::Danger, f.area.getVertices());

    const auto second = f.manager.consider(
        f.alert, f.track, true, f.destination, secondArea);

    REQUIRE(second);
    REQUIRE(second->proposalId != first->proposalId);

    REQUIRE(f.manager.state(first->proposalId) ==
            ProposalState::Expired);

    REQUIRE(f.manager.state(second->proposalId) ==
            ProposalState::Proposed);

    REQUIRE_FALSE(f.manager.decide(
        first->proposalId, ProposalState::Approved));

    REQUIRE(f.manager.decide(
        second->proposalId, ProposalState::Approved));
}

TEST_CASE("Vektor ignores unknown proposal and alert identifiers")
{
    Fixture f;

    REQUIRE_FALSE(f.manager.state("missing"));

    REQUIRE_FALSE(f.manager.decide(
        "missing", ProposalState::Approved));

    REQUIRE_NOTHROW(f.manager.closeAlert("missing"));

    // Unknown identifiers must not prevent normal operation.
    REQUIRE(f.propose());
}