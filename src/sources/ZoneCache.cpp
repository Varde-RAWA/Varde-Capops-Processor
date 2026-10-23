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
      zones_(ZoneSourceGeoService::loadFile(config.getZoneFile())),
      nextZoneRefresh_(now_()),
      nextAvoidanceRefresh_(now_())
{
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

    try
    {
        avoidanceAreas_ = ZoneSourceGeoService::loadFile(config.getAvoidanceFile());
    }
    catch (const std::exception& error)
    {
        std::cerr << "Avoidance data unavailable; continuing zone warnings: "
                  << error.what() << '\n';
    }
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
