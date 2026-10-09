#include "publish/RedisPublisher.hpp"
#include "publish/ProtoMapper.hpp"

RedisPublisher::RedisPublisher(const Configuration &config)
    : config_(config), redis_(config.getRedisUrl()),
      channel_(config.getRedisChannel()){}

void RedisPublisher::publish(const ProcessingResult &result)
{
    FlightDataProto proto = mapToProto(result, config_);

    std::string serialized;
    bool ok = proto.SerializeToString(&serialized);

    if (!ok)
    {
        throw std::runtime_error("Failed to serialize FlightDataProtof");
    }

    // std::cout << "\n--- PUBLISH UPDATE ---\n";

    // // Tracks
    // std::cout << "\nTracks (" << result.tracks.size() << "):\n";
    // std::cout << std::fixed << std::setprecision(5);

    // for (const auto &track : result.tracks)
    // {
    //     const auto &pos = track.getPosition();

    //     std::cout << "  " << track.getIcao() << " @ (" << pos.latDeg << ", " << pos.lonDeg << ")"
    //               << " time=" << track.getTimestamp() << "\n";
    // }


    // std::cout << "----------------------\n";

    redis_.publish(channel_, serialized);
}