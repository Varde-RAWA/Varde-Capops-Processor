#pragma once

#include "compute/AlertManager.hpp"
#include "compute/ComputeData.hpp"
#include "compute/VardeEngine.hpp"
#include "compute/VektorManager.hpp"
#include "ingest/VektorDecisionConsumer.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

class Configuration;
class EventDispatcher;
class IngestService;
class RadarSimulator;
class ZoneCache;

class ProcessorCycle
{
public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;
    using MakeId = std::function<std::string()>;

    ProcessorCycle(
        const Configuration& config,
        IngestService& ingest,
        ZoneCache& zoneCache,
        EventDispatcher& dispatcher,
        VektorDecisionConsumer& decisionConsumer,
        RadarSimulator* simulator,
        MakeId makeAlertId,
        MakeId makeProposalId,
        Now now = [] { return Clock::now(); });

    void runOnce();

private:
    void applyAlert(const AlertEvent& event);
    void evaluateTracks(const std::vector<Track>& tracks);
    void createProposals(const std::vector<Track>& tracks);
    void pollDecisions();
    void handleDecision(const varde::events::VektorDecisionEvent& event);

    IngestService& ingest_;
    ZoneCache& zoneCache_;
    EventDispatcher& dispatcher_;
    VektorDecisionConsumer& decisionConsumer_;
    RadarSimulator* simulator_;
    double timeStep_;
    Now now_;

    ComputeData computeData_;
    VardeEngine engine_;
    AlertManager alertManager_;
    VektorManager vektorManager_;
    std::map<std::string, AlertEvent> openAlerts_;
    std::set<std::string> previousAircraft_;
    Clock::time_point nextDecisionAttempt_{};
    bool decisionsUnavailable_ = false;
};
