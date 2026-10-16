#pragma once

#include "config/Config.hpp"
#include "domain/AlertEvent.hpp"
#include "domain/types/ProcessingResult.hpp"

#include <sw/redis++/redis++.h>
#include <string>


class RedisPublisher
{
public:
    RedisPublisher(
        const Configuration &config,
        std::string alertStream = "varde:alerts");

    void publish(const ProcessingResult &result);
    std::string publishAlert(const AlertEvent &event);

private:
    const Configuration &config_;
    sw::redis::Redis redis_;
    std::string channel_;
    std::string alertStream_;
};