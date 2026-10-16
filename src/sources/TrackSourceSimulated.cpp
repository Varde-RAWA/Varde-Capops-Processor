
#include "sources/TrackSourceSimulated.hpp"
#include "domain/Track.hpp"
#include "utils/time/IsoTimestamp.hpp"
#include <cmath>
#include <functional>
#include <utility>

TrackSourceSimulated::TrackSourceSimulated(RadarSimulator &simulator) : simulator_(simulator)
{
}

std::optional<Track> TrackSourceSimulated::getTrack(const std::string &icao) const
{
    constexpr double metresPerLatitudeDegree = 111320.0;
    constexpr double metresPerNauticalMile = 1852.0;
    constexpr double secondsPerHour = 3600.0;

    if (!simulator_.containsFlight(icao))
    {
        return std::nullopt;
    }

    auto velSim = simulator_.getVelocity(icao);
    auto posSim = simulator_.getPosition(icao);

    std::string timestamp = createIsoTimestamp();
    Position position{posSim.first, posSim.second};
    // Derive a deterministic cruise altitude per flight from its ICAO id
    //this is just for simulation purposes, in reality the altitude would be part of the track data
    std::hash<std::string> hasher;
    double altitudeFeet = 25000.0 + (hasher(icao) % 150) * 100.0;

    const double pi = std::acos(-1.0);
    const double latitudeRadians = position.latDeg * pi / 180.0;
    
    // Simulator velocity is latitude/longitude degrees per second.
    // Convert both components to metres per second.
    const double northMetresPerSecond = velSim.first * metresPerLatitudeDegree;
    const double eastMetresPerSecond =
        velSim.second * metresPerLatitudeDegree * std::cos(latitudeRadians);

    // One knot is one nautical mile (1852 metres) per hour.
    const double groundSpeedKnots =
        std::hypot(northMetresPerSecond, eastMetresPerSecond)
        * secondsPerHour / metresPerNauticalMile;

    const double verticalSpeed = 0.0;

    // Compass direction: north = 0°, east = 90°.
    double groundTrackDegrees =
        std::atan2(eastMetresPerSecond, northMetresPerSecond)
        * 180.0 / pi;

    if (groundTrackDegrees < 0.0)
        groundTrackDegrees += 360.0;

    const double headingDegrees = groundTrackDegrees;

    return Track(icao, timestamp, position, altitudeFeet, groundSpeedKnots, verticalSpeed,
                 headingDegrees, groundTrackDegrees);
}

std::vector<Track> TrackSourceSimulated::getAllTracks() const
{
    std::vector<Track> tracks;

    for (const auto &icao : simulator_.getFlights())
    {
        auto trackOpt = getTrack(icao);

        if (trackOpt)
        {
            tracks.push_back(*trackOpt);
        }
    }
    return tracks;
}
