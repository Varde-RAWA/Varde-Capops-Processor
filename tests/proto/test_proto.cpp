#include "../test_helpers.hpp"
#include "config/Config.hpp"
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

    // Create a processing result with test data
    ProcessingResult originalResult;

    // Add some tracks
    Position pos1{59.5, 5.0};
    Track track1("ABC123", "2024-01-01T12:00:00Z", pos1, 10000.0, 450.0, 100.0, 180.0, 175.0);
    originalResult.tracks.push_back(track1);

    Position pos2{59.6, 5.1};
    Track track2("DEF456", "2024-01-01T12:00:00Z", pos2, 8000.0, 400.0, -50.0, 270.0, 265.0);
    originalResult.tracks.push_back(track2);

    // Serialize to protobuf
    varde::events::FlightDataProto proto = mapToProto(originalResult, config);

    // Serialize to string
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);
    REQUIRE(serializeOk == true);
    REQUIRE(serialized.size() > 0);

    // Deserialize back
    varde::events::FlightDataProto deserializedProto;
    bool deserializeOk = deserializedProto.ParseFromString(serialized);
    REQUIRE(deserializeOk == true);

    // Verify metadata
    REQUIRE(deserializedProto.metadata().version() == config.getProtobufVersion());
    REQUIRE(deserializedProto.metadata().timestamp().size() > 0);

    // Verify tracks
    REQUIRE(deserializedProto.trackdata().totalaircraftcount() == 2);
    REQUIRE(deserializedProto.trackdata().coordinatesystem() == config.getCoordinateSystem());
    REQUIRE(deserializedProto.trackdata().tracks_size() == 2);

    const varde::events::TrackProto &trackProto1 = deserializedProto.trackdata().tracks(0);
    REQUIRE(trackProto1.icao24() == "ABC123");
    REQUIRE(trackProto1.timestamp() == "2024-01-01T12:00:00Z");
    REQUIRE(trackProto1.position().latitudedegrees() == 59.5);
    REQUIRE(trackProto1.position().longitudedegrees() == 5.0);
    REQUIRE(trackProto1.position().altitudefeet() == 10000.0);
    REQUIRE(trackProto1.velocity().groundspeedknots() == 450.0);
    REQUIRE(trackProto1.velocity().verticalspeedfeetperminute() == 100.0);
    REQUIRE(trackProto1.headingdegrees() == 180.0);
    REQUIRE(trackProto1.groundtrackdegrees() == 175.0);
}

TEST_CASE("Protobuf serialization with empty result")
{
    Configuration config = createTestConfig();

    ProcessingResult emptyResult;

    // Serialize to protobuf
    varde::events::FlightDataProto proto = mapToProto(emptyResult, config);

    // Serialize to string
    std::string serialized;
    bool serializeOk = proto.SerializeToString(&serialized);
    REQUIRE(serializeOk == true);

    // Deserialize back
    varde::events::FlightDataProto deserializedProto;
    bool deserializeOk = deserializedProto.ParseFromString(serialized);
    REQUIRE(deserializeOk == true);

    // Verify empty collections
    REQUIRE(deserializedProto.trackdata().totalaircraftcount() == 0);
    REQUIRE(deserializedProto.trackdata().tracks_size() == 0);
}

TEST_CASE("Zone alert Protobuf preserves fields and optional entry time")
{
    const Configuration config = createTestConfig();

    AlertEvent event{
        "alert-1",
        "SIM-0",
        "D-101",
        WarningLevel::PREDICTED,
        60.0,
        AlertEventType::OPENED
    };

    auto expectedLevel = varde::events::WARNING_LEVEL_PREDICTED;
    auto expectedType = varde::events::ZONE_ALERT_EVENT_OPENED;

    SECTION("Opened alert")
    {
    }

    SECTION("Level changed")
    {
        event.level = WarningLevel::IMMINENT;
        event.timeToEntrySeconds = 20.0;
        event.type = AlertEventType::LEVEL_CHANGED;

        expectedLevel = varde::events::WARNING_LEVEL_IMMINENT;
        expectedType = varde::events::ZONE_ALERT_EVENT_LEVEL_CHANGED;
    }

    SECTION("Aircraft already inside")
    {
        event.level = WarningLevel::INFRINGEMENT;
        event.timeToEntrySeconds.reset();

        expectedLevel = varde::events::WARNING_LEVEL_INFRINGEMENT;
    }

    SECTION("Closed alert")
    {
        event.level = WarningLevel::NONE;
        event.timeToEntrySeconds.reset();
        event.type = AlertEventType::CLOSED;

        expectedLevel = varde::events::WARNING_LEVEL_NONE;
        expectedType = varde::events::ZONE_ALERT_EVENT_CLOSED;
    }

    const auto proto = mapToProto(event, config);

    std::string bytes;
    REQUIRE(proto.SerializeToString(&bytes));

    varde::events::ZoneAlertEvent decoded;
    REQUIRE(decoded.ParseFromString(bytes));

    REQUIRE(decoded.metadata().version() == 3);
    REQUIRE_FALSE(decoded.metadata().timestamp().empty());
    REQUIRE(decoded.alertid() == event.alertId);
    REQUIRE(decoded.icao24() == event.icao);
    REQUIRE(decoded.zoneid() == event.zoneId);
    REQUIRE(decoded.level() == expectedLevel);
    REQUIRE(decoded.eventtype() == expectedType);
    REQUIRE(decoded.has_timetoentryseconds() == event.timeToEntrySeconds.has_value());

    if (event.timeToEntrySeconds)
    {
        REQUIRE(decoded.timetoentryseconds() == *event.timeToEntrySeconds);
    }
}