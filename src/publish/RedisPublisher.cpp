#include "publish/RedisPublisher.hpp"
#include "publish/ProtoMapper.hpp"
#include "publish/SerializationError.hpp"
#include "utils/time/IsoTimestamp.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

RedisPublisher::RedisPublisher(
    const Configuration &config,
    std::string alertStream,
    std::string proposalStream)
    : config_(config), redis_(config.getRedisUrl()),
      channel_(config.getRedisChannel()),
      alertStream_(std::move(alertStream)),
      proposalStream_(std::move(proposalStream))
{
}


void RedisPublisher::publish(const ProcessingResult &result)
{
    varde::events::FlightDataProto proto = mapToProto(result, config_);
    proto.mutable_metadata()->set_runid(runId_);

    std::string serialized;
    if (!proto.SerializeToString(&serialized))
    {
        throw SerializationError("Failed to serialize FlightDataProto");
    }

    const std::vector<std::pair<std::string, std::string>> fields{
        {"data", serialized}
    };

    constexpr const char *trackStream = "varde:tracks";
    constexpr long long maximumSnapshots = 10'000;
    constexpr bool approximateTrimming = true;

    // This is just to save flight snapshots in the redis stream.
    redis_.xadd(
        trackStream,
        "*",
        fields.begin(),
        fields.end(),
        maximumSnapshots,
        approximateTrimming);

    redis_.publish(channel_, serialized);
}

std::string RedisPublisher::publishAlert(const AlertEvent &event)
{
    auto proto = mapToProto(event, config_);
    proto.mutable_metadata()->set_runid(runId_);

    std::string serialized;
    if (!proto.SerializeToString(&serialized))
    {
    throw SerializationError("Failed to serialize ZoneAlertEvent");
    }

    const std::vector<std::pair<std::string, std::string>> fields{
        {"data", serialized}
    };

    constexpr long long maximumStreamLength = 10000;
    return redis_.xadd(alertStream_, "*", fields.begin(), fields.end(), maximumStreamLength, true);
}

std::string RedisPublisher::publishProposal(const VektorProposal &proposal)
{
    auto proto = mapToProto(proposal, config_);
    proto.mutable_metadata()->set_runid(runId_);
    std::string bytes;
    if (!proto.SerializeToString(&bytes))
        throw SerializationError("Failed to serialize VektorProposalEvent");
    const std::vector<std::pair<std::string, std::string>> fields{{"data", bytes}};
    return redis_.xadd(proposalStream_, "*", fields.begin(), fields.end(), 10000, true);
}

void RedisPublisher::publishRunStarted()
{
    varde::events::ProcessorRunStarted proto;

    auto* metadata = proto.mutable_metadata();
    metadata->set_version(config_.getProtobufVersion());
    metadata->set_timestamp(createIsoTimestamp());
    metadata->set_runid(runId_);

    proto.set_runid(runId_);

    std::string bytes;
    if (!proto.SerializeToString(&bytes))
        throw SerializationError("Failed to serialize ProcessorRunStarted");

    const std::vector<std::pair<std::string, std::string>> fields{
        {"data", bytes}
    };

    redis_.xadd(
        "varde:processor-runs", "*",
        fields.begin(), fields.end());
}
