#pragma once

#include "sources/interfaces/ITrackSource.hpp"
#include <memory>

class Configuration;
class RadarSimulator;

class ProcessorApp
{
public:
    ProcessorApp();
    ~ProcessorApp();

    void run();

private:
    void initializeSources(const Configuration& config);

    std::unique_ptr<ITrackSource> trackSource_;
    std::unique_ptr<RadarSimulator> radarSimulator_;
    bool simulationMode_ = false;
};
