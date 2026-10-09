#pragma once

#include "config/Config.hpp"
#include "domain/types/ProcessingResult.hpp"
#include <sw/redis++/redis++.h>

class RedisPublisher
{
  public:
    RedisPublisher(const Configuration &config);

    void publish(const ProcessingResult &result);

  private:
    const Configuration &config_;
    sw::redis::Redis redis_;
    std::string channel_;
};