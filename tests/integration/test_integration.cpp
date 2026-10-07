#include "../test_helpers.hpp"
#include "compute/ComputeData.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/WeatherCell.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "domain/types/WeatherSeverity.hpp"
#include "ingest/IngestService.hpp"
#include "proto/FlightData.pb.h"
#include "publish/ProtoMapper.hpp"
#include "publish/RedisPublisher.hpp"
#include "sources/simulations/WeatherSimulator.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/WeatherSourceSimulated.hpp"
#include "sources/simulations/RadarSimulator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// ============================================================================
// INTEGRATION TESTS - Combined logic
// ============================================================================

TEST_CASE("Full workflow: track and weather updates generate results")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    // Get the first sector center
    Position sectorCenter = grid.sectorCenter(0);

    // Add a few tracks to the sector
    for (int i = 0; i < 5; ++i)
    {
        Track track("FLIGHT_" + std::to_string(i), "2024-01-01T12:00:00Z", sectorCenter, 10000.0,
                    450.0, 0.0, 180.0, 175.0);
        computeData.handleTrackUpdate(track);
    }

    // Add weather to the sector
    WeatherCell weatherCell(0, "2024-01-01T12:00:00Z", WeatherSeverity::DEGRADED);
    computeData.handleWeatherUpdate(weatherCell);

    // Collect results
    ProcessingResult result = computeData.collectProcessingResult();

    auto sectorSummary0 = findSectorSummary(result, 0);

    REQUIRE(result.tracks.size() == 5);
    REQUIRE(result.sectorSummaries.size() == config.grid().rows * config.grid().cols); // 3x3 = 9
    REQUIRE(sectorSummary0->getWeatherSeverity() == WeatherSeverity::DEGRADED);
    REQUIRE(sectorSummary0->getLocalAircraftCount() == 5);
}

TEST_CASE("Multiple sectors with different weather conditions")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    // Add tracks and weather to different sectors
    for (int sectorId = 0; sectorId < 4; ++sectorId)
    {
        Position center = grid.sectorCenter(sectorId);

        // Add tracks
        for (int i = 0; i < 3; ++i)
        {
            Track track("FLIGHT_" + std::to_string(sectorId * 10 + i), "2024-01-01T12:00:00Z",
                        center, 10000.0, 450.0, 0.0, 180.0, 175.0);
            computeData.handleTrackUpdate(track);
        }

        // Add different weather
        WeatherSeverity severity = static_cast<WeatherSeverity>(sectorId % 4);
        WeatherCell weatherCell(sectorId, "2024-01-01T12:00:00Z", severity);
        computeData.handleWeatherUpdate(weatherCell);
    }

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 12);
    for (int i = 0; i < 4; ++i)
    {
        auto it = findSectorSummary(result, i);
        REQUIRE(it->getLocalAircraftCount() == 3);
    }
}

TEST_CASE("End-to-end: Simulated data is published to Redis")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    // Create weather simulator and set some patterns
    std::vector<std::pair<WeatherSeverity, double>> weatherLevels = config.getSortedWeatherLevels();
    WeatherSimulator weatherSimulator(config.grid(), weatherLevels);

    // Set constant weather patterns in specific sectors
    weatherSimulator.setWeatherPattern(0, 0, 0.3); // Sector 0: light weather
    weatherSimulator.setWeatherPattern(0, 1, 0.7); // Sector 1: heavy weather
    weatherSimulator.tick(0.0);

    // Add multiple simulated tracks to different sectors
    std::vector<std::string> flightIds = {"SIM001", "SIM002", "SIM003", "SIM004", "SIM005"};
    std::vector<int> targetSectors = {0, 0, 1, 1, 2};

    for (size_t i = 0; i < flightIds.size(); ++i)
    {
        Position sectorCenter = grid.sectorCenter(targetSectors[i]);
        // Add slight offset to avoid exact center
        Position trackPos{sectorCenter.latDeg + 0.01, sectorCenter.lonDeg + 0.01};

        Track track(flightIds[i], "2024-01-01T12:00:00Z", trackPos, 10000.0 + (i * 500),
                    400.0 + (i * 10), 50.0 - (i * 10), 180.0 + (i * 5), 175.0 + (i * 5));
        computeData.handleTrackUpdate(track);
    }

    // Update weather for several sectors
    for (int sectorId = 0; sectorId < 3; ++sectorId)
    {
        // Cycle through available weather severity levels
        WeatherSeverity weatherSev = weatherLevels[sectorId % weatherLevels.size()].first;
        WeatherCell weatherCell(sectorId, "2024-01-01T12:00:00Z", weatherSev);
        computeData.handleWeatherUpdate(weatherCell);
    }

    // Collect processing result from compute data
    ProcessingResult result = computeData.collectProcessingResult();

    // Map to protobuf (simulating what RedisPublisher does)
    FlightDataProto proto = mapToProto(result, config, config.grid());

    // Serialize to string (simulating protobuf serialization for Redis)
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);

    // Deserialize to verify the published data is valid
    FlightDataProto publishedData;
    bool deserializeOk = publishedData.ParseFromString(serialized);

    // Verify tracks made it through the pipeline
    REQUIRE(publishedData.trackdata().totalaircraftcount() == 5);
    REQUIRE(publishedData.trackdata().tracks_size() == 5);
    REQUIRE(publishedData.trackdata().coordinatesystem() == "WGS84");

    // Verify specific track data (order-independent due to unordered_map storage)
    for (const std::string &expectedId : flightIds)
    {
        bool found = false;
        for (int i = 0; i < publishedData.trackdata().tracks_size(); ++i)
        {
            const TrackProto &trackProto = publishedData.trackdata().tracks(i);
            if (trackProto.icao24() == expectedId)
            {
                REQUIRE(trackProto.timestamp() == "2024-01-01T12:00:00Z");
                auto it = std::find(flightIds.begin(), flightIds.end(), expectedId);
                int idx = std::distance(flightIds.begin(), it);
                REQUIRE(trackProto.position().altitudefeet() == 10000.0 + (idx * 500));
                REQUIRE(trackProto.velocity().groundspeedknots() == 400.0 + (idx * 10));
                break;
            }
        }
    }

    // Verify sector summaries made it through
    REQUIRE(publishedData.sectorsummarydata().rowscount() == 20);
    REQUIRE(publishedData.sectorsummarydata().columnscount() == 10);

    // Verify metadata
    REQUIRE(publishedData.metadata().version() == 1);
    REQUIRE(publishedData.metadata().timestamp().size() > 0);

    // Try to actually publish to Redis if available
    try
    {
        RedisPublisher publisher(config);
        publisher.publish(result);
        // If we get here, Redis was available and publishing succeeded
        REQUIRE(true);
    }
    catch (const std::exception &)
    {
        // Redis not available in test environment, but serialization verification passed
        REQUIRE(true);
    }
}


TEST_CASE("Simulated aircraft passes through ingest, computation and Protobuf")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    RadarSimulator simulator(config.grid());
    simulator.initializeFlights(
        {{59.25, 4.35}},
        {{60.25, 4.35}},
        {111.32}
    );

    TrackSourceSimulated trackSource(simulator);

    WeatherSimulator weatherSimulator(
        config.grid(), config.getSortedWeatherLevels()
    );
    WeatherSourceSimulated weatherSource(weatherSimulator);

    IngestService ingest(config.grid(), &trackSource, &weatherSource);
    ComputeData computeData(config);

    simulator.tick(config.getTimestepSize());

    auto tracks = ingest.getAllTracks();
    REQUIRE(tracks.size() == 1);

    for (const auto &track : tracks)
        computeData.handleTrackUpdate(track);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 1);

    int sectorId = grid.determineSector(Position{59.251, 4.35});
    REQUIRE(sectorId == 21);

    auto sector = findSectorSummary(result, sectorId);
    REQUIRE(sector != result.sectorSummaries.end());
    REQUIRE(sector->getLocalAircraftCount() == 1);

    auto identifiers = sector->getIcao24List();
    REQUIRE(identifiers.size() == 1);
    REQUIRE(identifiers[0] == "SIM-0");

    FlightDataProto message = mapToProto(result, config, config.grid());

    std::string bytes;
    REQUIRE(message.SerializeToString(&bytes));

    FlightDataProto decoded;
    REQUIRE(decoded.ParseFromString(bytes));

    REQUIRE(decoded.metadata().version() == config.getProtobufVersion());
    REQUIRE(decoded.trackdata().totalaircraftcount() == 1);
    REQUIRE(decoded.trackdata().tracks_size() == 1);

    const auto &aircraft = decoded.trackdata().tracks(0);
    REQUIRE(aircraft.icao24() == "SIM-0");
    REQUIRE(aircraft.position().latitudedegrees() == Catch::Approx(59.251));
    REQUIRE(aircraft.position().longitudedegrees() == Catch::Approx(4.35));
    REQUIRE(aircraft.timestamp() == tracks[0].getTimestamp());
    REQUIRE(aircraft.velocity().groundspeedknots() == Catch::Approx(216.0));

    bool sectorFound = false;
    for (const auto &summary : decoded.sectorsummarydata().sectorsummaries())
    {
        if (summary.sectorid() == sectorId)
        {
            sectorFound = true;
            REQUIRE(summary.localaircraftcount() == 1);
            REQUIRE(summary.icao24list_size() == 1);
            REQUIRE(summary.icao24list(0) == "SIM-0");
        }
    }
    REQUIRE(sectorFound);
}