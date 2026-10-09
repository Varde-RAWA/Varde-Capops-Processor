#include "app/ProcessorApp.hpp"

#include "compute/ComputeData.hpp"
#include "config/Config.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "ingest/IngestService.hpp"
#include "publish/RedisPublisher.hpp"

#include "sources/interfaces/ITrackSource.hpp"

#include "sources/simulations/RadarSimulator.hpp"

#include "sources/TrackSourceOpenSky.hpp"
#include "sources/TrackSourceSimulated.hpp"

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

    double timeStep = config.getTimestepSize();
    auto sleepDuration = std::chrono::milliseconds(config.getLoopInterval());

    while (true)
    {
        if (simulationMode_)
            tickSimulators(timeStep);
        processTrackUpdates(ingestService, computeData);
        publishResults(computeData, publisher);

        std::this_thread::sleep_for(sleepDuration);
    }
}

void ProcessorApp::initializeSources(const Configuration &config)
{
    simulationMode_ = (config.getSourceType() == SourceType::Simulation);

    if (simulationMode_)
    {
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


void ProcessorApp::publishResults(ComputeData &computeData, RedisPublisher &publisher)
{
    ProcessingResult result = computeData.collectProcessingResult();
    publisher.publish(result);
}