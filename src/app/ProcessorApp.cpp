#include "app/ProcessorApp.hpp"

#include "compute/AlertManager.hpp"
#include "compute/ComputeData.hpp"
#include "compute/VardeEngine.hpp"
#include "config/Config.hpp"
#include "domain/Zone.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "ingest/IngestService.hpp"
#include "publish/RedisPublisher.hpp"
#include "publish/SerializationError.hpp"
#include "sources/TrackSourceOpenSky.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/interfaces/ITrackSource.hpp"
#include "sources/simulations/RadarSimulator.hpp"
#include "sources/ZoneSourceGeoService.hpp"

#include <QUuid>
#include <set>
#include <utility>
#include <vector>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <future>
#include <optional>
#include <deque>

ProcessorApp::ProcessorApp() = default;
ProcessorApp::~ProcessorApp() = default;

void ProcessorApp::run()
{
    Configuration config("configuration.cfg");

    initializeSources(config);

    IngestService ingestService(config.grid(), trackSource_.get());
    ComputeData computeData(config);
    RedisPublisher publisher(config);

    VardeEngine engine;
    AlertManager alertManager([]
    {
        return QUuid::createUuid()
            .toString(QUuid::WithoutBraces)
            .toStdString();
    });

    // Used in file mode and until the first API response arrives.
    std::vector<Zone> zones = ZoneSourceGeoService::loadFile(config.getZoneFile());

    std::future<std::vector<Zone>> pendingZones;
    auto nextZoneRefresh = std::chrono::steady_clock::now();
    const auto zoneRefreshInterval =std::chrono::seconds(config.getZoneRefreshSeconds());

    std::set<std::string> previousAircraft;

    using Clock = std::chrono::steady_clock;

    std::deque<AlertEvent> pendingAlerts;
    constexpr std::size_t maxPendingAlerts = 10'000;
    std::size_t droppedAlerts = 0;

    auto queueAlert = [&](const AlertEvent &event)
    {
        if (pendingAlerts.size() >= maxPendingAlerts)
        {
            pendingAlerts.pop_front(); // Make room by dropping the oldest.
            ++droppedAlerts;

            if (droppedAlerts == 1 || droppedAlerts % 100 == 0)
                std::cerr << "Alert queue full; dropped "
                          << droppedAlerts << " alerts\n";
        }

        pendingAlerts.push_back(event);
    };



    Clock::time_point nextRedisAttempt{};
    bool redisWasUnavailable = false;

    const double timeStep = config.getTimestepSize();
    const auto sleepDuration =
        std::chrono::milliseconds(config.getLoopInterval());

    while (true)
    {
        if (config.getZoneSource() == "api")
{
    const auto now = std::chrono::steady_clock::now();

    if (!pendingZones.valid() && now >= nextZoneRefresh)
        {
            pendingZones = std::async(
                std::launch::async,
                [url = config.getZoneApiUrl()]
                {
                    ZoneSourceGeoService source;
                    return source.getActiveZones(url);
                });
            
            nextZoneRefresh = now + zoneRefreshInterval;
        }
    
        if (pendingZones.valid() &&
            pendingZones.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready)
        {
            std::optional<std::vector<Zone>> refreshed;
        
            try
            {
                refreshed = pendingZones.get();
            }
            catch (const std::exception &error)
            {
                std::cerr
                    << "Zone refresh failed; keeping cached zones: "
                    << error.what() << '\n';
            }
        
            if (refreshed)
            {
                std::set<std::string> activeIds;
            
                for (const Zone &zone : *refreshed)
                    activeIds.insert(zone.getId());
            
                // A successfully refreshed set may deactivate zones.
                for (const Zone &oldZone : zones)
                {
                    if (activeIds.count(oldZone.getId()) == 0)
                    {
                        const auto events =
                            alertManager.handleZoneDeactivation(
                                oldZone.getId());
                            
                        for (const AlertEvent &event : events)
                            queueAlert(event);
                    }
                }
            
                zones = std::move(*refreshed);
            }
        }
    }



        if (simulationMode_)
            tickSimulators(timeStep);

        processTrackUpdates(ingestService, computeData);

        const ProcessingResult result =
            computeData.collectProcessingResult();

        std::set<std::string> currentAircraft;

        for (const Track &track : result.tracks)
        {
            currentAircraft.insert(track.getIcao());

            const auto checks = engine.evaluateTrack(track, zones);

            for (const ZoneCheckResult &check : checks)
            {
                const auto event = alertManager.update(check);

                if (event)
                    queueAlert(*event);
            }
        }

        for (const std::string &icao : previousAircraft)
        {
            if (currentAircraft.count(icao) == 0)
            {
                const auto events =
                    alertManager.handleTrackDisappearance(icao);

                for (const AlertEvent &event : events)
                    queueAlert(event);
            }
        }

        previousAircraft = std::move(currentAircraft);

        if (Clock::now() >= nextRedisAttempt)
        {
            try
            {
                // Snapshots are current state: after an outage, send the latest one.
                // A failed snapshot can be replaced by the next cycle's snapshot.
                try
                {
                    publisher.publish(result);
                }
                catch (const SerializationError &error)
                {
                    std::cerr << "Skipping flight snapshot: " << error.what() << '\n';
                }

                constexpr std::size_t maxAlertsPerCycle = 100;
                std::size_t sent = 0;

                while (!pendingAlerts.empty() && sent < maxAlertsPerCycle)
                {
                    try
                    {
                        publisher.publishAlert(pendingAlerts.front());
                        pendingAlerts.pop_front(); // Redis replied successfully.
                    }
                    catch (const SerializationError &error)
                    {
                        std::cerr << "Cannot serialize alert "
                                  << pendingAlerts.front().alertId << ": "
                                  << error.what() << '\n';
                    
                        // Only do this if you accept losing this alert:
                        pendingAlerts.pop_front();
                    }
                
                    ++sent;
                }

            
                if (redisWasUnavailable)
                    std::cerr << "Redis publication recovered\n";
            
                redisWasUnavailable = false;
            }
            catch (const sw::redis::Error &error)
            {
                if (!redisWasUnavailable)
                    std::cerr << "Redis publication failed: "
                              << error.what() << '\n';
            
                redisWasUnavailable = true;
                nextRedisAttempt = Clock::now() + std::chrono::seconds(1);
            }
        }
        std::this_thread::sleep_for(sleepDuration);
    }
}

void ProcessorApp::initializeSources(const Configuration &config)
{
    simulationMode_ = (config.getSourceType() == SourceType::Simulation);

    if (simulationMode_)
    {
        radarSimulator_ = std::make_unique<RadarSimulator>(config.grid());
        radarSimulator_->initializeFlights(
            {{60.0, 4.6}}, // Start: latitude 60.0°, longitude 4.6° from Scenario 1.
            {{60.0, 5.6}}, // Destination: latitude 60.0°, longitude 5.6° from Scenario 1.
            {
                // Scenario speed: 360 knots (nautical miles per hour).
                // Multiply by 1852 metres per nautical mile, then divide
                // by 3600 seconds per hour to get 185.2 metres per second.
                360.0 * 1852.0 / 3600.0
            }
);

        trackSource_ = std::make_unique<TrackSourceSimulated>(*radarSimulator_);
        return;
    }

    if (config.getSourceType() == SourceType::Api)
    {

        auto openSky = std::make_unique<TrackSourceOpenSky>();
        openSky->setRegion(config.grid());
        trackSource_ = std::move(openSky);
        return;
    }

    throw std::runtime_error("Unsupported source type in configuration.");
}

void ProcessorApp::tickSimulators(double timeStep)
{

    radarSimulator_->tick(timeStep);
}

void ProcessorApp::processTrackUpdates(IngestService &ingestService, ComputeData &computeData)
{
    auto tracks = ingestService.getAllTracks();

    for (const auto &track : tracks)
    {
        computeData.handleTrackUpdate(track);
    }
}
