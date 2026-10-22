#pragma once

#include "config/Config.hpp"
#include "domain/AlertEvent.hpp"
#include "domain/VektorProposal.hpp"
#include "domain/types/ProcessingResult.hpp"

#include <sw/redis++/redis++.h>
#include <string>


class RedisPublisher
{
public:
    RedisPublisher(
    const Configuration &config,
    std::string alertStream = "varde:alerts",
    std::string proposalStream = "varde:vektor-proposals");

    void publish(const ProcessingResult &result);
    std::string publishAlert(const AlertEvent &event);
    std::string publishProposal(const VektorProposal &proposal);

private:
    const Configuration &config_;
    sw::redis::Redis redis_;
    std::string channel_;
    std::string alertStream_;
    std::string proposalStream_;
};