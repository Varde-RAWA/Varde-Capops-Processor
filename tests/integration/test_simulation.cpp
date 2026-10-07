#include "../test_helpers.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/simulations/RadarSimulator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>

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

    // Characterise the current source's approximate speed conversion.
    REQUIRE(after->getGroundSpeedKnots() == Catch::Approx(216.0));
    REQUIRE(after->getHeadingDegrees() == Catch::Approx(0.0));
    REQUIRE(after->getGroundTrackDegrees() == Catch::Approx(0.0));
    REQUIRE(after->getVerticalSpeedFeetPerMinute() == 0.0);
}

TEST_CASE("Simulated aircraft wrap across the grid boundaries periodically")
{
    Configuration config = createTestConfig();
    RadarSimulator simulator(config.grid());

    RadarSimulator::FlightPosition start;
    RadarSimulator::FlightPosition destination;
    Position expected;

    SECTION("Crossing the northern edge")
    {
        start = {60.9995, 5.0};
        destination = {61.9995, 5.0};
        expected = {59.0005, 5.0};
    }

    SECTION("Crossing the southern edge")
    {
        start = {59.0005, 5.0};
        destination = {58.0005, 5.0};
        expected = {60.9995, 5.0};
    }

    SECTION("Crossing the eastern edge")
    {
        start = {60.0, 5.9995};
        destination = {60.0, 6.9995};
        expected = {60.0, 4.0015};
    }

    SECTION("Crossing the western edge")
    {
        start = {60.0, 4.0005};
        destination = {60.0, 3.0005};
        expected = {60.0, 5.9985};
    }

    // 111.32 m/s gives 0.001 latitude degrees/s,
    // or 0.002 longitude degrees/s at 60°N.
    simulator.initializeFlights({start}, {destination}, {111.32});

    TrackSourceSimulated source(simulator);
    simulator.tick(config.getTimestepSize());

    auto track = source.getTrack("SIM-0");
    REQUIRE(track.has_value());
    REQUIRE(track->getPosition().latDeg ==
            Catch::Approx(expected.latDeg));
    REQUIRE(track->getPosition().lonDeg ==
            Catch::Approx(expected.lonDeg));
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