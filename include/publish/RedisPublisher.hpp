#pragma once

#include "config/Config.hpp"
#include "publish/IEventPublisher.hpp"

#include <sw/redis++/redis++.h>
#include <string>


class RedisPublisher : public IEventPublisher
{
public:
    RedisPublisher(
        const Configuration &config,
        std::string alertStream = "varde:alerts",
        std::string proposalStream = "varde:vektor-proposals");

    void publish(const ProcessingResult &result) override;
    std::string publishAlert(const AlertEvent &event) override;
    std::string publishProposal(const VektorProposal &proposal) override;

private:
    const Configuration &config_;
    sw::redis::Redis redis_;
    std::string channel_;
    std::string alertStream_;
    std::string proposalStream_;
};