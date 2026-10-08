#include "config/Config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
std::string trim(const std::string &s)
{
    auto start =
        std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto end =
        std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); })
            .base();

    if (start >= end)
        return "";
    return std::string(start, end);
}

std::string stripComment(const std::string &s)
{
    auto pos = s.find('#');
    if (pos == std::string::npos)
        return s;
    return s.substr(0, pos);
}
} // namespace

Configuration::Configuration(const std::string &path)
{
    load(path);
    grid_.computeDimensions();
}

const GridConfig &Configuration::grid() const
{
    return grid_;
}

double Configuration::defaultBaseCapacity() const
{
    return defaultBaseCapacity_;
}

void Configuration::load(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open config file");
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line))
    {
        line = trim(stripComment(line));
        if (line.empty())
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        // Env var name: section_key in UPPER_SNAKE_CASE, e.g. GRID_MINLAT
        std::string envKey = currentSection + "_" + key;                  
        std::transform(envKey.begin(), envKey.end(), envKey.begin(), ::toupper);
        if (const char *envVal = std::getenv(envKey.c_str()))
            value = envVal;

        if (currentSection == "grid")
        {
            if (key == "minLat")
                grid_.minLat = std::stod(value);
            else if (key == "maxLat")
                grid_.maxLat = std::stod(value);
            else if (key == "minLon")
                grid_.minLon = std::stod(value);
            else if (key == "maxLon")
                grid_.maxLon = std::stod(value);
            else if (key == "cellSizeDeg")
                grid_.cellSizeDeg = std::stod(value);

            else if (key == "coordinateSystem")
                coordinateSystem_ = value;
        }
        else if (currentSection == "capacity")
        {
            if (key == "defaultBaseCapacity")
                defaultBaseCapacity_ = std::stod(value);
        }
        else if (currentSection == "protobufVersion")
        {
            if (key == "version")
                protobufVersion_ = std::stoi(value);
        }
        else if (currentSection == "redis")
        {
            if (key == "redisUrl")
                redisUrl_ = value;
            else if (key == "redisChannel")
                redisChannel_ = value;
        }
        else if (currentSection == "dataSource")
        {
            if (key == "type")
            {
                if (value == "sim")
                    sourceType_ = SourceType::Simulation;
                else if (value == "api")
                    sourceType_ = SourceType::Api;
                else
                    throw std::invalid_argument("Invalid type '" + value +
                                                "'. Expected 'sim' or 'api'.");
            }
        }
        else if (currentSection == "simulation")
        {
            if (key == "numFlights")
                numFlights_ = std::stoi(value);
            if (key == "timestepSize")
                timestepSize_ = std::stod(value);
        }
        else if (currentSection == "execution")
        {
            if (key == "loopIntervalMs")
                loopIntervalMs_ = std::stoi(value);
        }
    }
}

int Configuration::getProtobufVersion() const
{
    return protobufVersion_;
}

int Configuration::getLoopInterval() const
{
    return loopIntervalMs_;
}

std::string Configuration::getCoordinateSystem() const
{
    return coordinateSystem_;
}

std::string Configuration::getRedisUrl() const
{
    return redisUrl_;
}

std::string Configuration::getRedisChannel() const
{
    return redisChannel_;
}

SourceType Configuration::getSourceType() const
{
    return sourceType_;
}

int Configuration::getNumFlights() const
{
    return numFlights_;
}

double Configuration::getTimestepSize() const
{
    return timestepSize_;
}


void GridConfig::computeDimensions()
{
    double latRange = maxLat - minLat;
    double lonRange = maxLon - minLon;

    rows = std::max(1, static_cast<int>(std::round(latRange / cellSizeDeg)));

    // Adjust cols for mercator: longitude degrees are shorter at higher latitudes
    double midLat = (minLat + maxLat) / 2.0;
    double lonScale = std::cos(midLat * M_PI / 180.0);
    double effectiveLonRange = lonRange * lonScale;
    double effectiveLatRange = latRange;

    cols =
        std::max(1, static_cast<int>(std::round(effectiveLonRange / (effectiveLatRange / rows))));
}