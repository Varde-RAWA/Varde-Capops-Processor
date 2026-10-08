#include "app/ProcessorApp.hpp"

#include "compute/ComputeData.hpp"
#include "compute/Grid.hpp"
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
    Grid grid(config.grid());

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
        radarSimulator_ = std::make_unique<RadarSimulator>(config.grid());
        radarSimulator_->initializeFlights(config.getNumFlights());

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