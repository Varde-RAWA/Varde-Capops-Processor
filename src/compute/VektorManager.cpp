#include "compute/VektorManager.hpp"

#include <stdexcept>
#include <utility>

VektorManager::VektorManager(std::function<std::string()> makeId)
    : makeId_(std::move(makeId))
{
}

std::optional<VektorProposal> VektorManager::consider(
    const AlertEvent &alert,
    const Track &track,
    bool simulated,
    std::optional<Position> destination,
    const Zone &avoidance)
{
    if (!simulated || !destination ||
        alert.type == AlertEventType::CLOSED ||
        (alert.level != WarningLevel::PREDICTED &&
         alert.level != WarningLevel::IMMINENT) ||
        alert.icao != track.getIcao() ||
        alert.zoneId != avoidance.getId() ||
        byAlert_.count(alert.alertId) != 0)
    {
        return std::nullopt;
    }

    const auto detour =
        engine_.calculateDetour(track, *destination, avoidance);

    if (!detour)
    {
        return std::nullopt;
    }

    const std::string id = makeId_();

    if (id.empty() || proposals_.count(id))
    {
        throw std::invalid_argument(
            "Proposal ID must be unique and nonempty");
    }

    VektorProposal proposal{
        id,
        alert.alertId,
        alert.icao,
        alert.zoneId,
        *detour
    };

    for (auto &[oldId, record] : proposals_)
    {
        if (record.proposal.icao == alert.icao &&
            record.state == ProposalState::Proposed)
        {
            record.state = ProposalState::Expired;
        }
    }

    proposals_.emplace(
        id, Record{proposal, ProposalState::Proposed});

    byAlert_.emplace(alert.alertId, id);

    return proposal;
}

std::optional<VektorProposal> VektorManager::decide(
    const std::string &id,
    ProposalState decision)
{
    auto it = proposals_.find(id);

    if (it == proposals_.end() ||
        it->second.state != ProposalState::Proposed ||
        decision == ProposalState::Proposed)
    {
        return std::nullopt;
    }

    it->second.state = decision;

    if (decision == ProposalState::Approved)
    {
        return it->second.proposal;
    }

    return std::nullopt;
}

void VektorManager::closeAlert(const std::string &alertId)
{
    auto it = byAlert_.find(alertId);

    if (it == byAlert_.end())
    {
        return;
    }

    auto &record = proposals_.at(it->second);

    if (record.state == ProposalState::Proposed)
    {
        record.state = ProposalState::Expired;
    }
}

std::optional<ProposalState> VektorManager::state(
    const std::string &id) const
{
    const auto it = proposals_.find(id);

    if (it == proposals_.end())
    {
        return std::nullopt;
    }

    return it->second.state;
}