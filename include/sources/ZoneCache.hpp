#pragma once

#include "domain/Zone.hpp"

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

class Configuration;

class ZoneCache
{
public:
    using Zones = std::vector<Zone>;
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    using Fetch = std::function<std::future<Zones>(const std::string&)>;

    explicit ZoneCache(
        const Configuration& config,
        Fetch fetch = {},
        Now now = Clock::now);

    std::vector<std::string> poll();
    const Zones& zones() const;
    const std::optional<Zones>& avoidanceAreas() const;

private:
    std::vector<std::string> refreshZones();
    void refreshAvoidanceAreas();

    bool apiMode_;
    std::string zoneApiUrl_;
    std::string avoidanceApiUrl_;
    std::chrono::seconds refreshInterval_;
    Fetch fetch_;
    Now now_;

    Zones zones_;
    std::optional<Zones> avoidanceAreas_;
    std::future<Zones> pendingZones_;
    std::future<Zones> pendingAvoidanceAreas_;
    Clock::time_point nextZoneRefresh_;
    Clock::time_point nextAvoidanceRefresh_;
    bool zoneRefreshHadFailed_ = false;
};
