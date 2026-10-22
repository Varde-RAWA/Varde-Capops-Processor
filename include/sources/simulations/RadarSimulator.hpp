#pragma once

#include "config/Config.hpp"
#include "domain/types/Position.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <optional>
#include <utility>

class RadarSimulator
{
  public:
    using FlightPosition = std::pair<double, double>;
    using FlightVelocity = std::pair<double, double>;
    using FlightID = std::string;

    RadarSimulator(GridConfig config);

    void start();

    void stop();

    void tick(double timeStep);

    void initializeFlights(int numFlights);

    void initializeFlights(std::vector<FlightPosition> startPositions,
                           std::vector<FlightPosition> endPositions, std::vector<double> speeds);

    bool containsFlight(const FlightID &flightId) const;
    std::vector<FlightID> getFlights() const;

    FlightPosition getPosition(const FlightID &flightId) const;
    FlightVelocity getVelocity(const FlightID &flightId) const;
    double getSpeed(const FlightID &flightId) const;

    std::optional<Position> getDestination(const FlightID &id) const;
    bool applyWaypoints(const FlightID &id, const std::vector<Position> &waypoints);

  private:
    void initializeFlights();

    FlightPosition enforcePeriodicBoundary(FlightPosition pos, double latWidth, double lonWidth);

    std::string getId(int row, int col);

    GridConfig gridConfig_;

    std::vector<FlightID> flightIds_;
    std::unordered_map<FlightID, FlightPosition> flightPositions_;
    std::unordered_map<FlightID, FlightVelocity> flightVelocities_;

    bool initialized = false;
    std::atomic<bool> isRunning_ = false;
    double currentTime_;
    double timeStep_;

    void updateVelocity(const FlightID &id);

    std::unordered_map<FlightID, FlightPosition> destinations_;
    std::unordered_map<FlightID, double> speedsMps_;
    std::unordered_map<FlightID, std::deque<FlightPosition>> remainingWaypoints_;
};
