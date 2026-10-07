#include "../test_helpers.hpp"
#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/Position.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "publish/RedisPublisher.hpp"
#include <catch2/catch_test_macros.hpp>
#include "proto/FlightData.pb.h"
#include <sw/redis++/redis++.h>
#include <string>

// ============================================================================
// REDIS PUBLISHER TESTS
// ============================================================================

TEST_CASE("RedisPublisher initialization")
{
    Configuration config = createTestConfig();
    requireRedisAvailable(config);

    REQUIRE_NOTHROW(RedisPublisher{config});
}

TEST_CASE("RedisPublisher publishes processing result")
{
    Configuration config = createTestConfig();
    requireRedisAvailable(config);

    RedisPublisher publisher(config);

    ProcessingResult result;
    Position pos{59.5, 5.0};

    Track track(
        "TEST001",
        "2024-01-01T12:00:00Z",
        pos,
        10000.0,
        450.0,
        0.0,
        180.0,
        175.0
    );

    result.tracks.push_back(track);

    REQUIRE_NOTHROW(publisher.publish(result));
}

TEST_CASE("RedisPublisher delivers the expected Protobuf message")
{
    Configuration config = createTestConfig();
    requireRedisAvailable(config);

    // Limit how long the subscriber waits for a message.
    std::string url = config.getRedisUrl();
    url += (url.find('?') == std::string::npos) ? "?" : "&";
    url += "connect_timeout=1000ms&socket_timeout=2000ms";

    sw::redis::Redis redis(url);
    auto subscriber = redis.subscriber();

    bool subscribed = false;
    bool received = false;
    std::string receivedChannel;
    std::string receivedPayload;

    subscriber.on_meta(
        [&](sw::redis::Subscriber::MsgType type,
            sw::redis::OptionalString channel,
            long long)
        {
            if (type == sw::redis::Subscriber::MsgType::SUBSCRIBE &&
                channel && *channel == config.getRedisChannel())
            {
                subscribed = true;
            }
        }
    );

    subscriber.on_message(
        [&](std::string channel, std::string payload)
        {
            received = true;
            receivedChannel = channel;
            receivedPayload = payload;
        }
    );

    // Wait for confirmation before publishing.
    subscriber.subscribe(config.getRedisChannel());
    REQUIRE_NOTHROW(subscriber.consume());
    REQUIRE(subscribed);

    ProcessingResult result;
    result.tracks.emplace_back(
        "TEST001",
        "2024-01-01T12:00:00Z",
        Position{59.5, 5.0},
        10000.0,
        450.0,
        0.0,
        180.0,
        175.0
    );

    RedisPublisher publisher(config);
    REQUIRE_NOTHROW(publisher.publish(result));

    // No message within two seconds means the test fails.
    REQUIRE_NOTHROW(subscriber.consume());
    REQUIRE(received);
    REQUIRE(receivedChannel == config.getRedisChannel());

    FlightDataProto decoded;
    REQUIRE(decoded.ParseFromString(receivedPayload));

    REQUIRE(decoded.metadata().version() == config.getProtobufVersion());
    REQUIRE_FALSE(decoded.metadata().timestamp().empty());
    REQUIRE(decoded.trackdata().coordinatesystem() ==
            config.getCoordinateSystem());
    REQUIRE(decoded.trackdata().totalaircraftcount() == 1);
    REQUIRE(decoded.trackdata().tracks_size() == 1);

    const auto &track = decoded.trackdata().tracks(0);

    REQUIRE(track.icao24() == "TEST001");
    REQUIRE(track.timestamp() == "2024-01-01T12:00:00Z");
    REQUIRE(track.position().latitudedegrees() == 59.5);
    REQUIRE(track.position().longitudedegrees() == 5.0);
    REQUIRE(track.position().altitudefeet() == 10000.0);
    REQUIRE(track.velocity().groundspeedknots() == 450.0);
    REQUIRE(track.velocity().verticalspeedfeetperminute() == 0.0);
    REQUIRE(track.headingdegrees() == 180.0);
    REQUIRE(track.groundtrackdegrees() == 175.0);

    REQUIRE(decoded.riskeventdata().riskeventcount() == 0);
    REQUIRE(decoded.riskeventdata().riskevents_size() == 0);
    REQUIRE(decoded.sectorsummarydata().sectorsummaries_size() == 0);
}