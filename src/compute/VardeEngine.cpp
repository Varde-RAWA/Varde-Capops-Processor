#include "compute/VardeEngine.hpp"

#include <cmath>
#include <stdexcept>


Position VardeEngine::predictPosition(const Track& track, double secondsAhead) const {

    if (secondsAhead < 0.0) {
        throw std::invalid_argument("Prediction time must be non-negative.");
    }

    const double pi = std::acos(-1.0);
    const double kilometresPerLatitudeDegree = 111.32;

    const Position currentPosition = track.getPosition();

    const double speedkmPerSecond = track.getGroundSpeedKnots() * (1.852 / 3600.0); // Convert knots to km/s
    const double distanceKm = speedkmPerSecond * secondsAhead; // Distance traveled in km

    const double directionRadians = track.getGroundTrackDegrees() * pi / 180.0; // Convert degrees to radians

    const double northKm = distanceKm * std::cos(directionRadians);
    const double eastKm = distanceKm * std::sin(directionRadians);

    const double latitudeRadians = currentPosition.latDeg * pi / 180.0;

    const double kilometresPerLongitudeDegree = kilometresPerLatitudeDegree * std::cos(latitudeRadians);

    return Position{
        currentPosition.latDeg + northKm / kilometresPerLatitudeDegree,
        currentPosition.lonDeg + eastKm / kilometresPerLongitudeDegree
    };
}



std::vector<std::string> VardeEngine::getZonesContainingTrack(const Track& track, const std::vector<Zone>& zones) const{
    std::vector<std::string> matchingZoneIds;
    const Position trackPosition = track.getPosition();

    for (const Zone& zone : zones) {
        if (zone.isEnabled() && zone.containsPosition(trackPosition)) {
            matchingZoneIds.push_back(zone.getId());
        }
    }
    return matchingZoneIds;
}




