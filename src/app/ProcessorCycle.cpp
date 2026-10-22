#include "app/ProcessorCycle.hpp"

#include "app/EventDispatcher.hpp"
#include "config/Config.hpp"
#include "ingest/IngestService.hpp"
#include "sources/ZoneCache.hpp"
#include "sources/simulations/RadarSimulator.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

ProcessorCycle::ProcessorCycle(
    const Configuration& config,
    IngestService& ingest,
    ZoneCache& zoneCache,
    EventDispatcher& dispatcher,
    VektorDecisionConsumer& decisionConsumer,
    RadarSimulator* simulator,
    MakeId makeAlertId,
    MakeId makeProposalId,
    Now now)
    : ingest_(ingest),
      zoneCache_(zoneCache),
      dispatcher_(dispatcher),
      decisionConsumer_(decisionConsumer),
      simulator_(simulator),
      timeStep_(config.getTimestepSize()),
      now_(std::move(now)),
      computeData_(config),
      alertManager_(std::move(makeAlertId)),
      vektorManager_(std::move(makeProposalId))
{
}

void ProcessorCycle::runOnce()
{
    for (const auto& zoneId : zoneCache_.poll())
    {
        for (const auto& event : alertManager_.handleZoneDeactivation(zoneId))
            applyAlert(event);
    }

    if (simulator_)
        simulator_->tick(timeStep_);

    computeData_.handleTrackSnapshot(ingest_.getAllTracks());
    const auto snapshot = computeData_.collectProcessingResult();

    evaluateTracks(snapshot.tracks);
    createProposals(snapshot.tracks);
    pollDecisions();
    dispatcher_.flush(snapshot, openAlerts_, vektorManager_);
}

void ProcessorCycle::applyAlert(const AlertEvent& event)
{
    if (event.type == AlertEventType::CLOSED)
    {
        openAlerts_.erase(event.alertId);
        vektorManager_.closeAlert(event.alertId);
    }
    else
    {
        openAlerts_.insert_or_assign(event.alertId, event);
    }

    dispatcher_.enqueueAlert(event);
}

void ProcessorCycle::evaluateTracks(const std::vector<Track>& tracks)
{
    std::set<std::string> currentAircraft;

    for (const auto& track : tracks)
    {
        currentAircraft.insert(track.getIcao());

        for (const auto& check : engine_.evaluateTrack(track, zoneCache_.zones()))
        {
            if (const auto event = alertManager_.update(check))
                applyAlert(*event);
        }
    }

    for (const auto& icao : previousAircraft_)
    {
        if (currentAircraft.count(icao) == 0)
        {
            for (const auto& event : alertManager_.handleTrackDisappearance(icao))
                applyAlert(event);
        }
    }

    previousAircraft_ = std::move(currentAircraft);
}

void ProcessorCycle::createProposals(const std::vector<Track>& tracks)
{
    const auto& areas = zoneCache_.avoidanceAreas();
    if (!simulator_ || !areas)
        return;

    for (const auto& entry : openAlerts_)
    {
        if (!dispatcher_.canQueueProposal())
            break;

        const auto& alert = entry.second;
        const auto track = std::find_if(tracks.begin(), tracks.end(),
            [&](const Track& candidate) { return candidate.getIcao() == alert.icao; });
        const auto area = std::find_if(areas->begin(), areas->end(),
            [&](const Zone& candidate) { return candidate.getId() == alert.zoneId; });

        if (track == tracks.end() || area == areas->end())
            continue;

        const auto proposal = vektorManager_.consider(
            alert, *track, true, simulator_->getDestination(alert.icao), *area);

        if (proposal)
            dispatcher_.enqueueProposal(*proposal);
    }
}

void ProcessorCycle::pollDecisions()
{
    if (now_() < nextDecisionAttempt_)
        return;

    try
    {
        decisionConsumer_.poll([this](const varde::events::VektorDecisionEvent& event)
        {
            handleDecision(event);
        });
        decisionsUnavailable_ = false;
    }
    catch (const sw::redis::Error& error)
    {
        if (!decisionsUnavailable_)
            std::cerr << "Vektor decision reading failed: " << error.what() << '\n';

        decisionsUnavailable_ = true;
        nextDecisionAttempt_ = now_() + std::chrono::seconds(1);
    }
}

void ProcessorCycle::handleDecision(const varde::events::VektorDecisionEvent& event)
{
    ProposalState decision;

    switch (event.decision())
    {
    case varde::events::VEKTOR_DECISION_APPROVED:
        decision = ProposalState::Approved;
        break;
    case varde::events::VEKTOR_DECISION_REJECTED:
        decision = ProposalState::Rejected;
        break;
    case varde::events::VEKTOR_DECISION_EXPIRED:
        decision = ProposalState::Expired;
        break;
    default:
        std::cerr << "Ignoring unsupported Vektor decision\n";
        return;
    }

    const auto approved = vektorManager_.decide(event.proposalid(), decision);
    if (!approved)
    {
        if (decision == ProposalState::Approved)
            std::cerr << "Ignoring unknown or no-longer-pending approval "
                      << event.proposalid() << '\n';
        return;
    }

    if (!simulator_ || !simulator_->containsFlight(approved->icao))
    {
        std::cerr << "Ignoring approval for absent simulated track " << approved->icao << '\n';
        return;
    }

    if (!simulator_->applyWaypoints(approved->icao, approved->detour.waypoints))
        std::cerr << "Could not apply Vektor route " << approved->proposalId << '\n';
}
