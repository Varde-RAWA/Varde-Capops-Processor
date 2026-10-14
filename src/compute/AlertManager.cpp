#include "compute/AlertManager.hpp"

#include <algorithm>
#include <utility>


AlertManager::AlertManager(std::function<std::string()> makeAlertId)
    : makeAlertId_(std::move(makeAlertId))
{
}

std::optional<AlertEvent> AlertManager::update(
    const ZoneCheckResult &result)
{
    const Key key{result.getIcao(), result.getZoneId()};
    auto it = states_.find(key);

    // A pair with no warning has no open alert.
    if (it == states_.end())
    {
        if (result.getLevel() == WarningLevel::NONE)
            return std::nullopt;

        const std::string alertId = makeAlertId_();
        states_.emplace(
            key,
            State{alertId, result.getLevel(), {}}
        );

        return AlertEvent{
            alertId, key.first, key.second,
            result.getLevel(),
            result.getTimeToEntrySeconds(),
            AlertEventType::OPENED
        };
    }

    State &state = it->second;

    // The enum values are ordered from NONE to INFRINGEMENT.
    if (result.getLevel() > state.publishedLevel)
    {
        state.publishedLevel = result.getLevel();
        state.lowerResults.clear();

        return AlertEvent{
            state.alertId, key.first, key.second,
            state.publishedLevel,
            result.getTimeToEntrySeconds(),
            AlertEventType::LEVEL_CHANGED
        };
    }

    if (result.getLevel() == state.publishedLevel)
    {
        // Lower results must occur in consecutive cycles.
        state.lowerResults.clear();
        return std::nullopt;
    }

    state.lowerResults.push_back(result);

    if (state.lowerResults.size() < 3)
        return std::nullopt;

    // Fall to the highest of the three lower computed levels.
    const auto highest = std::max_element(
        state.lowerResults.begin(),
        state.lowerResults.end(),
        [](const ZoneCheckResult &a, const ZoneCheckResult &b)
        {
            return a.getLevel() < b.getLevel();
        }
    );

    const WarningLevel newLevel = highest->getLevel();
    const auto entryTime = highest->getTimeToEntrySeconds();
    const std::string alertId = state.alertId;

    state.lowerResults.clear();
    state.publishedLevel = newLevel;

    if (newLevel == WarningLevel::NONE)
    {
        states_.erase(it);

        return AlertEvent{
            alertId, key.first, key.second,
            WarningLevel::NONE, std::nullopt,
            AlertEventType::CLOSED
        };
    }

    return AlertEvent{
        alertId, key.first, key.second,
        newLevel, entryTime,
        AlertEventType::LEVEL_CHANGED
    };
}


std::vector<AlertEvent> AlertManager::handleTrackDisappearance(
    const std::string &icao)
{
    std::vector<AlertEvent> events;

    for (auto it = states_.begin(); it != states_.end();)
    {
        if (it->first.first == icao)
        {
            events.push_back({
                it->second.alertId,
                it->first.first,
                it->first.second,
                WarningLevel::NONE,
                std::nullopt,
                AlertEventType::CLOSED
            });
            it = states_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return events;
}

std::vector<AlertEvent> AlertManager::handleZoneDeactivation(
    const std::string &zoneId)
{
    std::vector<AlertEvent> events;

    for (auto it = states_.begin(); it != states_.end();)
    {
        if (it->first.second == zoneId)
        {
            events.push_back({
                it->second.alertId,
                it->first.first,
                it->first.second,
                WarningLevel::NONE,
                std::nullopt,
                AlertEventType::CLOSED
            });
            it = states_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return events;
}







