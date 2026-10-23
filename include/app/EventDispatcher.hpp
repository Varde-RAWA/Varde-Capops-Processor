#pragma once

#include "publish/IEventPublisher.hpp"

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>

class VektorManager;

class EventDispatcher
{
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    using OpenAlerts = std::map<std::string, AlertEvent>;

    explicit EventDispatcher(IEventPublisher& publisher, Now now = Clock::now);

    void enqueueAlert(const AlertEvent& event);
    bool canQueueProposal() const;
    void enqueueProposal(const VektorProposal& proposal);

    void flush(
        const ProcessingResult& snapshot,
        const OpenAlerts& openAlerts,
        VektorManager& vektorManager);

private:
    void publishAlerts(const OpenAlerts& openAlerts);
    void publishProposals(VektorManager& vektorManager);

    static constexpr std::size_t maxPendingAlerts = 10'000;
    static constexpr std::size_t maxPendingProposals = 10'000;
    static constexpr std::size_t maxPerCycle = 100;

    IEventPublisher& publisher_;
    Now now_;
    std::deque<AlertEvent> pendingAlerts_;
    std::deque<VektorProposal> pendingProposals_;
    std::set<std::string> publishedOpenAlerts_;
    std::size_t droppedAlerts_ = 0;
    Clock::time_point nextRedisAttempt_{};
    bool redisWasUnavailable_ = false;
};
