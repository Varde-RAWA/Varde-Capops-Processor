#include "publish/RedisPublisher.hpp"
#include "publish/ProtoMapper.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

RedisPublisher::RedisPublisher(
    const Configuration &config,
    std::string alertStream)
    : config_(config),
      redis_(config.getRedisUrl()),
      channel_(config.getRedisChannel()),
      alertStream_(std::move(alertStream))
{
}

void RedisPublisher::publish(const ProcessingResult &result)
{
    varde::events::FlightDataProto proto = mapToProto(result, config_);

    std::string serialized;
    bool ok = proto.SerializeToString(&serialized);

    if (!ok){
        throw std::runtime_error("Failed to serialize FlightDataProtof");
    }

    redis_.publish(channel_, serialized);
}

std::string RedisPublisher::publishAlert(const AlertEvent &event)
{
    const auto proto = mapToProto(event, config_);

    std::string serialized;
    if (!proto.SerializeToString(&serialized))
    {
        throw std::runtime_error("Failed to serialize ZoneAlertEvent.");
    }

    const std::vector<std::pair<std::string, std::string>> fields{
        {"data", serialized}
    };

    constexpr long long maximumStreamLength = 10000;
    return redis_.xadd(alertStream_, "*", fields.begin(), fields.end(), maximumStreamLength, true);
}