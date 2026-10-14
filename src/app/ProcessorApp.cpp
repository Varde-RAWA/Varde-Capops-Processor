#include "app/ProcessorApp.hpp"

#include "compute/AlertManager.hpp"
#include "compute/ComputeData.hpp"
#include "compute/VardeEngine.hpp"
#include "config/Config.hpp"
#include "domain/Zone.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "ingest/IngestService.hpp"
#include "publish/RedisPublisher.hpp"
#include "sources/TrackSourceOpenSky.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/interfaces/ITrackSource.hpp"
#include "sources/simulations/RadarSimulator.hpp"

#include <QUuid>
#include <set>
#include <utility>
#include <vector>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

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

    const std::vector<Zone> zones{};

    std::set<std::string> previousAircraft;

    const double timeStep = config.getTimestepSize();
    const auto sleepDuration =
        std::chrono::milliseconds(config.getLoopInterval());

    while (true)
    {
        if (simulationMode_)
            tickSimulators(timeStep);

        processTrackUpdates(ingestService, computeData);

        const ProcessingResult result =
            computeData.collectProcessingResult();

        publisher.publish(result);

        std::set<std::string> currentAircraft;

        for (const Track &track : result.tracks)
        {
            currentAircraft.insert(track.getIcao());

            const auto checks = engine.evaluateTrack(track, zones);

            for (const ZoneCheckResult &check : checks)
            {
                const auto event = alertManager.update(check);

                if (event)
                    publisher.publishAlert(*event);
            }
        }

        for (const std::string &icao : previousAircraft)
        {
            if (currentAircraft.count(icao) == 0)
            {
                const auto events =
                    alertManager.handleTrackDisappearance(icao);

                for (const AlertEvent &event : events)
                    publisher.publishAlert(event);
            }
        }

        previousAircraft = std::move(currentAircraft);

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
