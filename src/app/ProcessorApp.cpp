#include "app/ProcessorApp.hpp"

#include "app/EventDispatcher.hpp"
#include "app/ProcessorCycle.hpp"
#include "config/Config.hpp"
#include "ingest/IngestService.hpp"
#include "ingest/VektorDecisionConsumer.hpp"
#include "publish/RedisPublisher.hpp"
#include "sources/TrackSourceOpenSky.hpp"
#include "sources/TrackSourceSimulated.hpp"
#include "sources/ZoneCache.hpp"
#include "sources/simulations/RadarSimulator.hpp"

#include <QUuid>
#include <chrono>
#include <stdexcept>
#include <thread>

ProcessorApp::ProcessorApp() = default;
ProcessorApp::~ProcessorApp() = default;

void ProcessorApp::run()
{
    Configuration config("configuration.cfg");
    initializeSources(config);

    IngestService ingest(config.grid(), trackSource_.get());
    RedisPublisher publisher(config);
    VektorDecisionConsumer decisionConsumer(config);
    ZoneCache zoneCache(config);
    EventDispatcher dispatcher(publisher);

    const auto makeId = []
    {
        return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    };

    // Construct the cycle after the objects it references.
    ProcessorCycle cycle(
        config, ingest, zoneCache, dispatcher, decisionConsumer,
        simulationMode_ ? radarSimulator_.get() : nullptr,
        makeId, makeId);

    const auto sleepDuration = std::chrono::milliseconds(config.getLoopInterval());

    publisher.publishRunStarted();

    while (true)
    {
        cycle.runOnce();
        std::this_thread::sleep_for(sleepDuration);
    }
}

void ProcessorApp::initializeSources(const Configuration& config)
{
    simulationMode_ = (config.getSourceType() == SourceType::Simulation);

    if (simulationMode_)
    {
        radarSimulator_ = std::make_unique<RadarSimulator>(config.grid());
        radarSimulator_->initializeFromScenarioFiles(config.getScenarioFiles());

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
