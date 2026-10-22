#pragma once

#include "config/Config.hpp"
#include "proto/FlightData.pb.h"
#include <sw/redis++/redis++.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

class VektorDecisionConsumer
{
public:
    using Handler = std::function<void(const varde::events::VektorDecisionEvent &)>;
    VektorDecisionConsumer(const Configuration &config);

    void poll(const Handler &handle);

private:
    using Fields = std::vector<std::pair<std::string, std::string>>;
    using Entry = std::pair<std::string, sw::redis::Optional<Fields>>;

    void ensureGroup();
    std::vector<Entry> read(const std::string &id);

    sw::redis::Redis redis_;
    bool groupReady_ = false;

    const std::string stream_ = "varde:vektor-decisions";
    const std::string group_ = "processor-vektor";
    const std::string consumer_ = "processor-main";
};