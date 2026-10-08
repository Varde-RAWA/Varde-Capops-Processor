#include "../test_helpers.hpp"
#include "compute/Grid.hpp"
#include "config/Config.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "proto/FlightData.pb.h"
#include "publish/ProtoMapper.hpp"
#include <catch2/catch_test_macros.hpp>

// ============================================================================
// PROTOBUF SERIALIZATION TESTS
// ============================================================================

TEST_CASE("Protobuf serialization round-trip")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    // Create a processing result with test data
    ProcessingResult originalResult;

    // Add some tracks
    Position pos1{59.5, 5.0};
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1, 10000.0, 450.0, 100.0, 180.0, 175.0);
    originalResult.tracks.push_back(track1);

    Position pos2{59.6, 5.1};
    Track track2("DEF456", "2024-01-01T12:00:00Z", pos2, 8000.0, 400.0, -50.0, 270.0, 265.0);
    originalResult.tracks.push_back(track2);

    // Add sector summaries
    SectorSummary summary1(0, 0, 0, "2024-01-01T12:00:00Z", 1);
    originalResult.sectorSummaries.push_back(summary1);

    SectorSummary summary2(1, 0, 1, "2024-01-01T12:00:00Z", 0);
    originalResult.sectorSummaries.push_back(summary2);

    // Serialize to protobuf
    FlightDataProto proto = mapToProto(originalResult, config, config.grid());

    // Serialize to string
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);
    REQUIRE(serializeOk == true);
    REQUIRE(serialized.size() > 0);

    // Deserialize back
    FlightDataProto deserializedProto;
    bool deserializeOk = deserializedProto.ParseFromString(serialized);
    REQUIRE(deserializeOk == true);

    // Verify metadata
    REQUIRE(deserializedProto.metadata().version() == config.getProtobufVersion());
    REQUIRE(deserializedProto.metadata().timestamp().size() > 0);

    // Verify tracks
    REQUIRE(deserializedProto.trackdata().totalaircraftcount() == 2);
    REQUIRE(deserializedProto.trackdata().coordinatesystem() == config.getCoordinateSystem());
    REQUIRE(deserializedProto.trackdata().tracks_size() == 2);

    const TrackProto &trackProto1 = deserializedProto.trackdata().tracks(0);
    REQUIRE(trackProto1.icao24() == "ABC123");
    REQUIRE(trackProto1.timestamp() == "2024-01-01T12:00:00Z");
    REQUIRE(trackProto1.position().latitudedegrees() == 59.5);
    REQUIRE(trackProto1.position().longitudedegrees() == 5.0);
    REQUIRE(trackProto1.position().altitudefeet() == 10000.0);
    REQUIRE(trackProto1.velocity().groundspeedknots() == 450.0);
    REQUIRE(trackProto1.velocity().verticalspeedfeetperminute() == 100.0);
    REQUIRE(trackProto1.headingdegrees() == 180.0);
    REQUIRE(trackProto1.groundtrackdegrees() == 175.0);

    // Verify sector summaries
    REQUIRE(deserializedProto.sectorsummarydata().rowscount() == grid.rows());
    REQUIRE(deserializedProto.sectorsummarydata().columnscount() == grid.cols());
    REQUIRE(deserializedProto.sectorsummarydata().minlatitude() == config.grid().minLat);
    REQUIRE(deserializedProto.sectorsummarydata().maxlatitude() == config.grid().maxLat);
    REQUIRE(deserializedProto.sectorsummarydata().minlongitude() == config.grid().minLon);
    REQUIRE(deserializedProto.sectorsummarydata().maxlongitude() == config.grid().maxLon);
    REQUIRE(deserializedProto.sectorsummarydata().sectorsummaries_size() == 2);

    const SectorSummaryProto &summaryProto1 =
        deserializedProto.sectorsummarydata().sectorsummaries(0);
    REQUIRE(summaryProto1.sectorid() == 0);
    REQUIRE(summaryProto1.row() == 0);
    REQUIRE(summaryProto1.column() == 0);
    REQUIRE(summaryProto1.localaircraftcount() == 1);

    const SectorSummaryProto &summaryProto2 =
        deserializedProto.sectorsummarydata().sectorsummaries(1);
    REQUIRE(summaryProto2.sectorid() == 1);
}

TEST_CASE("Protobuf serialization with empty result")
{
    Configuration config = createTestConfig();
    Grid grid(config.grid());

    ProcessingResult emptyResult;

    // Serialize to protobuf
    FlightDataProto proto = mapToProto(emptyResult, config, config.grid());

    // Serialize to string
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);
    REQUIRE(serializeOk == true);

    // Deserialize back
    FlightDataProto deserializedProto;
    bool deserializeOk = deserializedProto.ParseFromString(serialized);
    REQUIRE(deserializeOk == true);

    // Verify empty collections
    REQUIRE(deserializedProto.trackdata().totalaircraftcount() == 0);
    REQUIRE(deserializedProto.trackdata().tracks_size() == 0);
    REQUIRE(deserializedProto.riskeventdata().riskeventcount() == 0);
    REQUIRE(deserializedProto.riskeventdata().riskevents_size() == 0);
    REQUIRE(deserializedProto.sectorsummarydata().sectorsummaries_size() == 0);
}
