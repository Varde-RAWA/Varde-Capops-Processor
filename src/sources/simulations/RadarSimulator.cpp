#include "sources/simulations/RadarSimulator.hpp"
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <unordered_set>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

RadarSimulator::RadarSimulator(GridConfig config)
{
    timeStep_ = 1.0;
    currentTime_ = 0.0;
    gridConfig_ = config;
}

void RadarSimulator::initializeFlights(int numFlights)
{
    std::random_device device;
    std::mt19937 generator(device());

    std::uniform_real_distribution<double> dist(0, 1);

    double minLat = gridConfig_.minLat;
    double minLon = gridConfig_.minLon;
    double latWidth = gridConfig_.maxLat - gridConfig_.minLat;
    double lonWidth = gridConfig_.maxLon - gridConfig_.minLon;

    std::vector<FlightPosition> startPositions;
    std::vector<FlightPosition> endPositions;
    std::vector<double> speeds;

    for (int i = 0; i < numFlights; ++i)
    {
        double latStart = minLat + latWidth * dist(generator);
        double lonStart = minLon + lonWidth * dist(generator);

        double latEnd = minLat + latWidth * dist(generator);
        double lonEnd = minLon + lonWidth * dist(generator);

        double speed = 200 + 100 * dist(generator); // ~390–580 kts

        startPositions.push_back({latStart, lonStart});
        endPositions.push_back({latEnd, lonEnd});
        speeds.push_back(speed);
    }

    initializeFlights(startPositions, endPositions, speeds);
}

void RadarSimulator::initializeFlights(
    std::vector<FlightPosition> starts,
    std::vector<FlightPosition> destinations,
    std::vector<double> speeds,
    std::vector<FlightID> ids)
{
    if (ids.empty())
    {
        for (std::size_t i = 0; i < starts.size(); ++i)
        {
            ids.push_back("SIM-" + std::to_string(i));
        }
    }

    std::unordered_set<FlightID> uniqueIds;

    for (const auto& id : ids)
    {
        if (id.empty() || !uniqueIds.insert(id).second)
        {
            throw std::invalid_argument(
                "Simulated flight IDs must be nonempty and unique: " + id);
        }
    }

    if (starts.size() != destinations.size() ||
        starts.size() != speeds.size() ||
        starts.size() != ids.size())
    {
        throw std::invalid_argument("Input vectors must have the same size");
    }

    const auto validPosition = [](FlightPosition position)
    {
        return std::isfinite(position.first) &&
               std::isfinite(position.second) &&
               std::abs(position.first) < 89.0 &&
               std::abs(position.second) <= 180.0;
    };

    for (std::size_t i = 0; i < starts.size(); ++i)
    {
        if (!validPosition(starts[i]) ||
            !validPosition(destinations[i]) ||
            !std::isfinite(speeds[i]) ||
            speeds[i] <= 0.0)
        {
            throw std::invalid_argument("Invalid simulated flight");
        }
    }

    flightIds_.clear();
    flightPositions_.clear();
    flightVelocities_.clear();
    destinations_.clear();
    speedsMps_.clear();
    remainingWaypoints_.clear();
    currentTime_ = 0.0;

    for (std::size_t i = 0; i < starts.size(); ++i)
    {
        const FlightID& id = ids[i];

        flightIds_.push_back(id);
        flightPositions_[id] = starts[i];
        destinations_[id] = destinations[i];
        speedsMps_[id] = speeds[i];

        // Initially, fly directly to the destination.
        remainingWaypoints_[id] = {destinations[i]};
        updateVelocity(id);
    }

    initialized = true;
}

void RadarSimulator::start()
{

    if (!initialized)
    {
        int numFlights = 20;
        initializeFlights(numFlights);
    }

    isRunning_ = true;

    while (isRunning_)
        tick(timeStep_);
}

void RadarSimulator::stop()
{
    isRunning_ = false;
}

void RadarSimulator::tick(double timeStep)
{
    if (!std::isfinite(timeStep) || timeStep < 0.0)
    {
        throw std::invalid_argument(
            "Time step must be finite and non-negative");
    }

    currentTime_ += timeStep;

    constexpr double metresPerLatitudeDegree = 111320.0;
    const double pi = std::acos(-1.0);

    for (const FlightID &id : flightIds_)
    {
        double remainingSeconds = timeStep;

        auto &route = remainingWaypoints_.at(id);
        auto &position = flightPositions_.at(id);

        while (!route.empty() && remainingSeconds > 0.0)
        {
            const auto target = route.front();

            const double metresPerLongitudeDegree =
                metresPerLatitudeDegree *
                std::cos(position.first * pi / 180.0);

            const double distanceMetres = std::hypot(
                (target.first - position.first) * metresPerLatitudeDegree,
                (target.second - position.second) * metresPerLongitudeDegree);

            const double secondsToTarget =
                distanceMetres / speedsMps_.at(id);

            if (secondsToTarget <= remainingSeconds)
            {
                position = target;
                route.pop_front();
                remainingSeconds -= secondsToTarget;
            }
            else
            {
                const double fraction = remainingSeconds / secondsToTarget;

                position.first +=
                    fraction * (target.first - position.first);

                position.second +=
                    fraction * (target.second - position.second);

                remainingSeconds = 0.0;
            }
        }

        updateVelocity(id);
    }
        // Completed flights must disappear from the next live snapshot.
    flightIds_.erase(std::remove_if(flightIds_.begin(), flightIds_.end(),
        [this](const FlightID &id)
        {
            if (!remainingWaypoints_.at(id).empty())
                return false;
 
            flightPositions_.erase(id);
            flightVelocities_.erase(id);
            destinations_.erase(id);
            speedsMps_.erase(id);
            remainingWaypoints_.erase(id);
            return true;
        }), flightIds_.end());
}

RadarSimulator::FlightPosition
RadarSimulator::enforcePeriodicBoundary(FlightPosition pos, double latWidth, double lonWidth)
{
    double newLat = pos.first;
    double newLon = pos.second;

    newLat = gridConfig_.minLat + std::fmod(newLat - gridConfig_.minLat, latWidth);
    if (newLat < gridConfig_.minLat)
        newLat += latWidth;

    newLon = gridConfig_.minLon + std::fmod(newLon - gridConfig_.minLon, lonWidth);
    if (newLon < gridConfig_.minLon)
        newLon += lonWidth;

    return {newLat, newLon};
}

bool RadarSimulator::containsFlight(const FlightID &flightId) const
{
    return flightPositions_.find(flightId) != flightPositions_.end();
}

std::vector<RadarSimulator::FlightID> RadarSimulator::getFlights() const
{
    return flightIds_;
}

RadarSimulator::FlightPosition RadarSimulator::getPosition(const FlightID &flightId) const
{
    return flightPositions_.at(flightId);
}

RadarSimulator::FlightVelocity RadarSimulator::getVelocity(const FlightID &flightId) const
{
    return flightVelocities_.at(flightId);
}

double RadarSimulator::getSpeed(const FlightID &flightId) const
{
    auto vel = flightVelocities_.at(flightId);
    return std::sqrt(vel.first * vel.first + vel.second * vel.second);
}

std::optional<Position> RadarSimulator::getDestination(
    const FlightID &id) const
{
    const auto it = destinations_.find(id);

    if (it == destinations_.end())
    {
        return std::nullopt;
    }

    return Position{it->second.first, it->second.second};
}

bool RadarSimulator::applyWaypoints(
    const FlightID &id,
    const std::vector<Position> &waypoints)
{
    if (!containsFlight(id))
    {
        return false;
    }

    std::deque<FlightPosition> route;

    for (const Position &position : waypoints)
    {
        if (!std::isfinite(position.latDeg) ||
            !std::isfinite(position.lonDeg) ||
            std::abs(position.latDeg) >= 89.0 ||
            std::abs(position.lonDeg) > 180.0)
        {
            return false;
        }

        route.emplace_back(position.latDeg, position.lonDeg);
    }

    route.push_back(destinations_.at(id));
    remainingWaypoints_[id] = std::move(route);
    updateVelocity(id);

    return true;
}

void RadarSimulator::updateVelocity(const FlightID &id)
{
    flightVelocities_[id] = {0.0, 0.0};

    auto &route = remainingWaypoints_.at(id);
    const auto position = flightPositions_.at(id);

    // Skip waypoints that are already reached, including duplicates.
    while (!route.empty() && route.front() == position)
    {
        route.pop_front();
    }

    if (route.empty())
    {
        return;
    }

    const auto target = route.front();

    constexpr double metresPerLatitudeDegree = 111320.0;
    const double pi = std::acos(-1.0);
    const double metresPerLongitudeDegree =
        metresPerLatitudeDegree * std::cos(position.first * pi / 180.0);

    const double latitudeDifference = target.first - position.first;
    const double longitudeDifference = target.second - position.second;

    const double distanceMetres = std::hypot(
        latitudeDifference * metresPerLatitudeDegree,
        longitudeDifference * metresPerLongitudeDegree);

    if (distanceMetres > 0.0)
    {
        const double seconds = distanceMetres / speedsMps_.at(id);

        // Velocity remains latitude/longitude degrees per second.
        flightVelocities_[id] = {
            latitudeDifference / seconds,
            longitudeDifference / seconds
        };
    }
}

void RadarSimulator::initializeFromScenarioFiles(const std::string& paths)
{
    std::vector<FlightPosition> starts;
    std::vector<FlightPosition> destinations;
    std::vector<double> speeds;
    std::vector<FlightID> ids;
    std::unordered_set<FlightID> seenIds;

    const auto filenames =
        QString::fromStdString(paths).split(',', Qt::KeepEmptyParts);

    for (const auto& entry : filenames)
    {
        const QString filename = entry.trimmed();
        const std::string path = filename.toStdString();

        if (filename.isEmpty())
        {
            throw std::runtime_error(
                "simulation.scenarioFiles contains an empty filename");
        }

        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly))
        {
            throw std::runtime_error(
                path + ": cannot open scenario: " +
                file.errorString().toStdString());
        }

        QJsonParseError parseError;
        const auto document =
            QJsonDocument::fromJson(file.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            throw std::runtime_error(
                path + ": invalid JSON: " +
                parseError.errorString().toStdString());
        }

        const auto root = document.object();
        if (!document.isObject() ||
            root.value("type").toString() != "FeatureCollection" ||
            !root.value("features").isArray())
        {
            throw std::runtime_error(
                path + ": expected a GeoJSON FeatureCollection");
        }

        const auto features = root.value("features").toArray();
        if (features.isEmpty())
        {
            throw std::runtime_error(path + ": scenario has no aircraft");
        }

        for (qsizetype i = 0; i < features.size(); ++i)
        {
            const std::string context =
                path + ": feature " + std::to_string(i + 1);

            const auto feature = features.at(i).toObject();
            const auto geometry = feature.value("geometry").toObject();
            const auto properties = feature.value("properties").toObject();

            if (feature.value("type").toString() != "Feature" ||
                geometry.value("type").toString() != "Point" ||
                !feature.value("properties").isObject())
            {
                throw std::runtime_error(
                    context + ": expected a Point feature with properties");
            }

            const auto icaoValue = properties.value("icao24");
            const std::string icao =
                icaoValue.toString().toLower().toStdString();

            const bool validIcao =
                icaoValue.isString() && icao.size() == 6 &&
                std::all_of(icao.begin(), icao.end(), [](char c)
                {
                    return (c >= '0' && c <= '9') ||
                           (c >= 'a' && c <= 'f');
                });

            if (!validIcao)
            {
                throw std::runtime_error(
                    context + ": icao24 must contain six hexadecimal characters");
            }

            if (!seenIds.insert(icao).second)
            {
                throw std::runtime_error(
                    context + ": duplicate icao24 across selected scenarios: " + icao);
            }

            const auto readPosition =
                [&](const QJsonValue& value,
                    const std::string& field) -> FlightPosition
            {
                const auto coordinates = value.toArray();

                if (!value.isArray() || coordinates.size() != 2 ||
                    !coordinates.at(0).isDouble() ||
                    !coordinates.at(1).isDouble())
                {
                    throw std::runtime_error(
                        context + ": " + field +
                        " must contain [longitude, latitude]");
                }

                const double lon = coordinates.at(0).toDouble();
                const double lat = coordinates.at(1).toDouble();

                // Match the geographic limits supported by the simulator.
                if (!std::isfinite(lat) || !std::isfinite(lon) ||
                    std::abs(lat) >= 89.0 || std::abs(lon) > 180.0)
                {
                    throw std::runtime_error(
                        context + ": invalid " + field);
                }

                // GeoJSON uses longitude, latitude.
                // RadarSimulator uses latitude, longitude.
                return {lat, lon};
            };

            const auto start =
                readPosition(geometry.value("coordinates"), "coordinates");
            const auto destination =
                readPosition(properties.value("destination"), "destination");

            const auto speedValue = properties.value("groundSpeedKt");
            const double knots = speedValue.toDouble();

            if (!speedValue.isDouble() ||
                !std::isfinite(knots) || knots <= 0.0)
            {
                throw std::runtime_error(
                    context + ": groundSpeedKt must be a positive number");
            }

            // Delayed activation needs additional simulator support.
            // Reject it explicitly instead of starting those flights early.
            const auto delay = properties.value("startDelaySeconds");
            if (!delay.isUndefined() &&
                (!delay.isDouble() || delay.toDouble() != 0.0))
            {
                throw std::runtime_error(
                    context + ": only startDelaySeconds=0 is supported");
            }

            ids.push_back(icao);
            starts.push_back(start);
            destinations.push_back(destination);

            speeds.push_back(5 * knots * 1852.0 / 3600.0);
        }
    }

    // Initialize once, after collecting every file's aircraft.
    // Calling this inside the loop would replace the previous file's flights.
    initializeFlights(
        std::move(starts),
        std::move(destinations),
        std::move(speeds),
        std::move(ids));
}
