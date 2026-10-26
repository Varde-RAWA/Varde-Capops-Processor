#include "sources/ZoneCache.hpp"

#include "config/Config.hpp"
#include "sources/ZoneSourceGeoService.hpp"

#include <exception>
#include <iostream>
#include <set>
#include <utility>

ZoneCache::ZoneCache(const Configuration& config, Fetch fetch, Now now)
    : apiMode_(config.getZoneSource() == "api"),
      zoneApiUrl_(config.getZoneApiUrl()),
      avoidanceApiUrl_(config.getAvoidanceApiUrl()),
      refreshInterval_(config.getZoneRefreshSeconds()),
      fetch_(std::move(fetch)),
      now_(std::move(now)),
      nextZoneRefresh_(now_()),
      nextAvoidanceRefresh_(now_())
{
    if (!apiMode_)
    {
        zones_ = ZoneSourceGeoService::loadFile(
            config.getZoneFile());

        avoidanceAreas_ = ZoneSourceGeoService::loadFile(
            config.getAvoidanceFile());

        std::clog << "Zone source: local files. Restricted: "
                  << zones_.size() << ", avoidance: "
                  << avoidanceAreas_->size() << '\n';
        return;
    }

    if (!fetch_)
    {
        fetch_ = [](const std::string& url)
        {
            return std::async(std::launch::async, [url]
            {
                ZoneSourceGeoService source;
                return source.getActiveZones(url);
            });
        };
    }

    auto initialZones = fetch_(zoneApiUrl_);
    auto initialAvoidance = fetch_(avoidanceApiUrl_);

    zones_ = initialZones.get();
    avoidanceAreas_ = initialAvoidance.get();

    const auto nextRefresh = now_() + refreshInterval_;
    nextZoneRefresh_ = nextRefresh;
    nextAvoidanceRefresh_ = nextRefresh;

    std::clog << "Zone source: API. Restricted: "
              << zones_.size() << ", avoidance: "
              << avoidanceAreas_->size() << '\n';
}

const ZoneCache::Zones& ZoneCache::zones() const
{
    return zones_;
}

const std::optional<ZoneCache::Zones>& ZoneCache::avoidanceAreas() const
{
    return avoidanceAreas_;
}

std::vector<std::string> ZoneCache::poll()
{
    if (!apiMode_)
        return {};

    auto deactivated = refreshZones();
    refreshAvoidanceAreas();
    return deactivated;
}

std::vector<std::string> ZoneCache::refreshZones()
{
    const auto now = now_();

    if (!pendingZones_.valid() && now >= nextZoneRefresh_)
    {
        pendingZones_ = fetch_(zoneApiUrl_);
        nextZoneRefresh_ = now + refreshInterval_;
    }

    if (!pendingZones_.valid() ||
        pendingZones_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return {};

    std::optional<Zones> refreshed;

    try
    {
        refreshed = pendingZones_.get();
    }
    catch (const std::exception& error)
    {
        zoneRefreshHadFailed_ = true;
        std::cerr << "Zone refresh failed; keeping cached zones: "
                  << error.what() << '\n';
        return {};
    }

    std::set<std::string> activeIds;
    for (const auto& zone : *refreshed)
        activeIds.insert(zone.getId());

    std::vector<std::string> deactivated;
    for (const auto& oldZone : zones_)
    {
        if (activeIds.count(oldZone.getId()) == 0)
            deactivated.push_back(oldZone.getId());
    }

    zones_ = std::move(*refreshed);

    if (zoneRefreshHadFailed_)
    {
        std::cerr << "Zone refresh recovered; cached zones updated\n";
        zoneRefreshHadFailed_ = false;
    }

    return deactivated;
}

void ZoneCache::refreshAvoidanceAreas()
{
    const auto now = now_();

    if (!pendingAvoidanceAreas_.valid() && now >= nextAvoidanceRefresh_)
    {
        pendingAvoidanceAreas_ = fetch_(avoidanceApiUrl_);
        nextAvoidanceRefresh_ = now + refreshInterval_;
    }

    if (!pendingAvoidanceAreas_.valid() ||
        pendingAvoidanceAreas_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    try
    {
        avoidanceAreas_ = pendingAvoidanceAreas_.get();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Avoidance refresh failed; "
                  << (avoidanceAreas_ ? "keeping cached areas: "
                                     : "no avoidance data available: ")
                  << error.what() << '\n';
    }
}
