#include "../test_helpers.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/simulations/RadarSimulator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <limits>
#include <stdexcept>

namespace
{
GridConfig waypointGrid()
{
    GridConfig grid{};
    grid.minLat = 59.0;
    grid.maxLat = 61.0;
    grid.minLon = 3.0;
    grid.maxLon = 7.0;
    return grid;
}
}

TEST_CASE("Configured simulation produces the requested aircraft")
{
    Configuration config = createTestConfig();

    RadarSimulator simulator(config.grid());
    simulator.initializeFlights(config.getNumFlights());

    TrackSourceSimulated source(simulator);
    auto tracks = source.getAllTracks();

    REQUIRE(tracks.size() == 3);

    std::set<std::string> identifiers;

    for (const auto &track : tracks)
    {
        identifiers.insert(track.getIcao());

        Position position = track.getPosition();

        REQUIRE(position.latDeg >= config.grid().minLat);
        REQUIRE(position.latDeg < config.grid().maxLat);
        REQUIRE(position.lonDeg >= config.grid().minLon);
        REQUIRE(position.lonDeg < config.grid().maxLon);
        REQUIRE_FALSE(track.getTimestamp().empty());
    }

    const std::set<std::string> expected{"SIM-0", "SIM-1", "SIM-2"};
    REQUIRE(identifiers == expected);
}

TEST_CASE("Simulation movement appears in track snapshots")
{
    Configuration config = createTestConfig();
    RadarSimulator simulator(config.grid());

    // Fly north at 111.32 metres/second:
    // equivalent to 0.001 degrees latitude/second.
    simulator.initializeFlights(
        {{59.25, 4.35}},
        {{60.25, 4.35}},
        {111.32}
    );

    TrackSourceSimulated source(simulator);

    auto before = source.getTrack("SIM-0");
    REQUIRE(before.has_value());
    REQUIRE(before->getPosition().latDeg == Catch::Approx(59.25));
    REQUIRE(before->getPosition().lonDeg == Catch::Approx(4.35));

    // The test configuration advances one simulated second.
    simulator.tick(config.getTimestepSize());

    auto after = source.getTrack("SIM-0");
    REQUIRE(after.has_value());

    REQUIRE(after->getIcao() == before->getIcao());
    REQUIRE(after->getPosition().latDeg == Catch::Approx(59.251));
    REQUIRE(after->getPosition().lonDeg == Catch::Approx(4.35));

    REQUIRE(after->getGroundSpeedKnots() == Catch::Approx(111.32 * 3600.0 / 1852.0));
    REQUIRE(after->getHeadingDegrees() == Catch::Approx(0.0));
    REQUIRE(after->getGroundTrackDegrees() == Catch::Approx(0.0));
    REQUIRE(after->getVerticalSpeedFeetPerMinute() == 0.0);
}

TEST_CASE("Multiple simulation ticks accumulate aircraft movement")
{
    Configuration config = createTestConfig();
    RadarSimulator simulator(config.grid());

    // Fly north at 0.001 latitude degrees per simulated second.
    simulator.initializeFlights(
        {{59.25, 4.35}},
        {{60.25, 4.35}},
        {111.32}
    );

    TrackSourceSimulated source(simulator);

    for (int tick = 1; tick <= 5; ++tick)
    {
        simulator.tick(config.getTimestepSize());

        auto track = source.getTrack("SIM-0");
        REQUIRE(track.has_value());
        REQUIRE(track->getIcao() == "SIM-0");

        double expectedLatitude =
            59.25 + tick * 0.001 * config.getTimestepSize();

        REQUIRE(track->getPosition().latDeg ==
                Catch::Approx(expectedLatitude));
        REQUIRE(track->getPosition().lonDeg ==
                Catch::Approx(4.35));
    }

    REQUIRE(source.getAllTracks().size() == 1);
}



TEST_CASE("Simulation removes aircraft on arrival")
{
    RadarSimulator simulator(waypointGrid());

    // Northbound: 111.32 m/s equals 0.001 latitude degrees/s.
    simulator.initializeFlights(
        {{60.0, 4.6}},
        {{60.01, 4.6}},
        {111.32});

    const auto destination = simulator.getDestination("SIM-0");

    REQUIRE(destination);
    REQUIRE(destination->latDeg == Catch::Approx(60.01));
    REQUIRE(destination->lonDeg == Catch::Approx(4.6));

    // Destination is approximately ten seconds away.
    simulator.tick(20.0);

    REQUIRE_FALSE(simulator.containsFlight("SIM-0"));
    REQUIRE(simulator.getFlights().empty());
    REQUIRE_FALSE(simulator.getDestination("SIM-0"));

    TrackSourceSimulated source(simulator);
    REQUIRE_FALSE(source.getTrack("SIM-0"));

    // Further ticks must not restore the completed aircraft.
    simulator.tick(20.0);

    REQUIRE_FALSE(simulator.containsFlight("SIM-0"));
    REQUIRE(simulator.getFlights().empty());
}

TEST_CASE("Simulation follows waypoints and carries time across turns")
{
    RadarSimulator simulator(waypointGrid());

    simulator.initializeFlights(
        {{60.0, 4.6}},
        {{60.02, 4.61}},
        {111.32});

    REQUIRE(simulator.applyWaypoints(
        "SIM-0",
        {{60.01, 4.6}, {60.01, 4.61}}));

    // First leg travels north.
    simulator.tick(5.0);

    REQUIRE(simulator.getPosition("SIM-0").first == Catch::Approx(60.005));
    REQUIRE(simulator.getPosition("SIM-0").second == Catch::Approx(4.6));

    // Five more seconds reach the waypoint; one second continues east.
    simulator.tick(6.0);

    REQUIRE(simulator.getPosition("SIM-0").first == Catch::Approx(60.01));
    REQUIRE(simulator.getPosition("SIM-0").second > 4.6);
    REQUIRE(simulator.getPosition("SIM-0").second < 4.61);

    TrackSourceSimulated source(simulator);
    const auto track = source.getTrack("SIM-0");

    REQUIRE(track);
    REQUIRE(track->getGroundTrackDegrees() == Catch::Approx(90.0));
    REQUIRE(track->getGroundSpeedKnots() ==
            Catch::Approx(111.32 * 3600.0 / 1852.0));

    // One large tick completes the remaining legs and removes the aircraft.
    simulator.tick(1000.0);

    REQUIRE_FALSE(simulator.containsFlight("SIM-0"));
    REQUIRE(simulator.getFlights().empty());
    REQUIRE_FALSE(simulator.getDestination("SIM-0"));
    REQUIRE_FALSE(source.getTrack("SIM-0"));
}

TEST_CASE("Simulation ignores unknown flights and rejects invalid routes")
{
    RadarSimulator simulator(waypointGrid());

    simulator.initializeFlights(
        {{60.0, 4.6}},
        {{60.02, 4.6}},
        {111.32});

    REQUIRE_FALSE(simulator.getDestination("missing"));

    REQUIRE_FALSE(simulator.applyWaypoints(
        "missing", {{60.01, 4.6}}));

    REQUIRE_FALSE(simulator.applyWaypoints(
        "SIM-0",
        {{std::numeric_limits<double>::quiet_NaN(), 4.6}}));

    // The rejected route leaves the original northbound route usable.
    simulator.tick(1.0);

    REQUIRE(simulator.getPosition("SIM-0").first == Catch::Approx(60.001));
    REQUIRE(simulator.getPosition("SIM-0").second == Catch::Approx(4.6));
}

TEST_CASE("Repeated waypoints and an empty detour preserve the destination")
{
    RadarSimulator simulator(waypointGrid());

    simulator.initializeFlights(
        {{60.0, 4.6}},
        {{60.02, 4.6}},
        {111.32});

    REQUIRE(simulator.applyWaypoints(
        "SIM-0", {{60.0, 4.6}, {60.0, 4.6}}));

    REQUIRE(simulator.getVelocity("SIM-0").first > 0.0);

    // Empty detour means continue directly to the retained destination.
    REQUIRE(simulator.applyWaypoints("SIM-0", {}));

    simulator.tick(1.0);

    REQUIRE(simulator.getPosition("SIM-0").first == Catch::Approx(60.001));

    REQUIRE_THROWS_AS(simulator.tick(-1.0), std::invalid_argument);

    REQUIRE_THROWS_AS(
        simulator.tick(std::numeric_limits<double>::infinity()),
        std::invalid_argument);

    REQUIRE(simulator.getPosition("SIM-0").first == Catch::Approx(60.001));
}
