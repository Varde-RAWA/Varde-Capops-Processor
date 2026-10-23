#include "app/EventDispatcher.hpp"

#include "compute/VektorManager.hpp"
#include "publish/SerializationError.hpp"

#include <sw/redis++/redis++.h>
#include <iostream>
#include <stdexcept>
#include <utility>

EventDispatcher::EventDispatcher(IEventPublisher& publisher, Now now)
    : publisher_(publisher), now_(std::move(now))
{
}

void EventDispatcher::enqueueAlert(const AlertEvent& event)
{
    if (event.type == AlertEventType::CLOSED)
        publishedOpenAlerts_.erase(event.alertId);

    if (pendingAlerts_.size() >= maxPendingAlerts)
    {
        pendingAlerts_.pop_front();
        ++droppedAlerts_;

        if (droppedAlerts_ == 1 || droppedAlerts_ % 100 == 0)
            std::cerr << "Alert queue full; dropped " << droppedAlerts_ << " alerts\n";
    }

    pendingAlerts_.push_back(event);
}

bool EventDispatcher::canQueueProposal() const
{
    return pendingProposals_.size() < maxPendingProposals;
}

void EventDispatcher::enqueueProposal(const VektorProposal& proposal)
{
    if (!canQueueProposal())
        throw std::logic_error("Check proposal queue capacity before creating a proposal");

    pendingProposals_.push_back(proposal);
}

void EventDispatcher::flush(
    const ProcessingResult& snapshot,
    const OpenAlerts& openAlerts,
    VektorManager& vektorManager)
{
    if (now_() < nextRedisAttempt_)
        return;

    try
    {
        try
        {
            publisher_.publish(snapshot);
        }
        catch (const SerializationError& error)
        {
            std::cerr << "Skipping flight snapshot: " << error.what() << '\n';
        }

        publishAlerts(openAlerts);
        publishProposals(vektorManager);

        if (redisWasUnavailable_)
            std::cerr << "Redis publication recovered\n";

        redisWasUnavailable_ = false;
    }
    catch (const sw::redis::Error& error)
    {
        if (!redisWasUnavailable_)
            std::cerr << "Redis publication failed: " << error.what() << '\n';

        redisWasUnavailable_ = true;
        nextRedisAttempt_ = now_() + std::chrono::seconds(1);
    }
}

void EventDispatcher::publishAlerts(const OpenAlerts& openAlerts)
{
    std::size_t sent = 0;

    while (!pendingAlerts_.empty() && sent < maxPerCycle)
    {
        const auto& event = pendingAlerts_.front();

        try
        {
            publisher_.publishAlert(event);

            if (event.type == AlertEventType::OPENED && openAlerts.count(event.alertId) != 0)
                publishedOpenAlerts_.insert(event.alertId);
            else if (event.type == AlertEventType::CLOSED)
                publishedOpenAlerts_.erase(event.alertId);

            pendingAlerts_.pop_front();
        }
        catch (const SerializationError& error)
        {
            std::cerr << "Cannot serialize alert " << event.alertId
                      << ": " << error.what() << '\n';
            pendingAlerts_.pop_front();
        }

        ++sent;
    }
}

void EventDispatcher::publishProposals(VektorManager& vektorManager)
{
    std::size_t sent = 0;

    while (pendingAlerts_.empty() && !pendingProposals_.empty() && sent < maxPerCycle)
    {
        const auto& proposal = pendingProposals_.front();

        if (vektorManager.state(proposal.proposalId) != ProposalState::Proposed)
        {
            pendingProposals_.pop_front();
            continue;
        }

        if (publishedOpenAlerts_.count(proposal.alertId) == 0)
        {
            std::cerr << "Discarding proposal " << proposal.proposalId
                      << ": alert " << proposal.alertId
                      << " has no successfully published OPENED event\n";
            vektorManager.decide(proposal.proposalId, ProposalState::Expired);
            pendingProposals_.pop_front();
            continue;
        }

        try
        {
            publisher_.publishProposal(proposal);
            pendingProposals_.pop_front();
        }
        catch (const SerializationError& error)
        {
            std::cerr << "Discarding proposal " << proposal.proposalId
                      << ": " << error.what() << '\n';
            vektorManager.decide(proposal.proposalId, ProposalState::Expired);
            pendingProposals_.pop_front();
        }

        ++sent;
    }
}
