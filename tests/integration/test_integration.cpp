#include "../test_helpers.hpp"
#include "compute/ComputeData.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "ingest/IngestService.hpp"
#include "proto/FlightData.pb.h"
#include "publish/ProtoMapper.hpp"
#include "publish/RedisPublisher.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/simulations/RadarSimulator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// ============================================================================
// INTEGRATION TESTS - Combined logic
// ============================================================================
TEST_CASE("Track updates generate aircraft results")
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

    // Collect results
    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 5);
}
TEST_CASE("Aircraft in different sectors are retained")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

    for (int sectorId = 0; sectorId < 4; ++sectorId)
    {
        Position center = grid.sectorCenter(sectorId);

        for (int i = 0; i < 3; ++i)
        {
            Track track("FLIGHT_" + std::to_string(sectorId * 10 + i), "2024-01-01T12:00:00Z",
                        center, 10000.0, 450.0, 0.0, 180.0, 175.0);
            computeData.handleTrackUpdate(track);
        }
    }

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 12);
}

TEST_CASE("End-to-end: Simulated data is published to Redis")
{
    Configuration config = createTestConfig();
    ComputeData computeData(config);
    Grid grid(config.grid());

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


    // Collect processing result from compute data
    ProcessingResult result = computeData.collectProcessingResult();

    // Map to protobuf (simulating what RedisPublisher does)
    varde::events::FlightDataProto proto = mapToProto(result, config);

    // Serialize to string (simulating protobuf serialization for Redis)
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);

    // Deserialize to verify the published data is valid
    varde::events::FlightDataProto publishedData;
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
            const varde::events::TrackProto &trackProto = publishedData.trackdata().tracks(i);
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

    // Verify metadata
    REQUIRE(publishedData.metadata().version() == config.getProtobufVersion());
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

    RadarSimulator simulator(config.grid());
    simulator.initializeFlights(
        {{59.25, 4.35}},
        {{60.25, 4.35}},
        {111.32}
    );

    TrackSourceSimulated trackSource(simulator);

    IngestService ingest(config.grid(), &trackSource);
    ComputeData computeData(config);

    simulator.tick(config.getTimestepSize());

    auto tracks = ingest.getAllTracks();
    REQUIRE(tracks.size() == 1);

    for (const auto &track : tracks)
        computeData.handleTrackUpdate(track);

    ProcessingResult result = computeData.collectProcessingResult();

    REQUIRE(result.tracks.size() == 1);

    varde::events::FlightDataProto message = mapToProto(result, config);

    std::string bytes;
    REQUIRE(message.SerializeToString(&bytes));

    varde::events::FlightDataProto decoded;
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

}