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
#include <QUuid>
#include <iterator>
#include <utility>
#include <vector>

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

    varde::events::FlightDataProto decoded;
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

}

TEST_CASE("Vektor proposals are stored in a Redis stream")
{
    const auto config = createTestConfig();
    requireRedisAvailable(config);
    sw::redis::Redis redis(config.getRedisUrl());
    const std::string stream = "test:vektor:" +
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    RedisPublisher publisher(config, stream + ":alerts", stream);
    const VektorProposal proposal{
        "proposal-1", "alert-1", "SIM-0", "D-101",
        Detour{{{60.10, 4.90}, {60.10, 5.30}}, 4.45, 24.03}};
    const auto id = publisher.publishProposal(proposal);
    // Expire only this test's unique key, even if an assertion fails below.
    redis.expire(stream, 60);
    using Fields = std::vector<std::pair<std::string, std::string>>;
    std::vector<std::pair<std::string, Fields>> entries;
    redis.xrange(stream, "-", "+", std::back_inserter(entries));
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].first == id);
    REQUIRE(entries[0].second.size() == 1);
    REQUIRE(entries[0].second[0].first == "data");
    varde::events::VektorProposalEvent decoded;
    REQUIRE(decoded.ParseFromString(entries[0].second[0].second));
    REQUIRE(decoded.proposalid() == proposal.proposalId);
    REQUIRE(decoded.alertid() == proposal.alertId);
    REQUIRE(decoded.metadata().version() == 3);
    REQUIRE(decoded.waypoints_size() == 2);
    REQUIRE(decoded.waypoints(0).longitudedegrees() == 4.90);
    REQUIRE(decoded.waypoints(0).latitudedegrees() == 60.10);
    REQUIRE(decoded.extradistancekm() == 4.45);
    REQUIRE(decoded.extratimeseconds() == 24.03);
    redis.del(stream);
}