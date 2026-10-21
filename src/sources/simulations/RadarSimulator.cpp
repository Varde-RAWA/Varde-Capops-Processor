#include "sources/simulations/RadarSimulator.hpp"
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>

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
    std::vector<double> speeds)
{
    if (starts.size() != destinations.size() ||
        starts.size() != speeds.size())
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
        const FlightID id = "SIM-" + std::to_string(i);

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