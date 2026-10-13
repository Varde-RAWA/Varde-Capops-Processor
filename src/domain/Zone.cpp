#include "domain/Zone.hpp"

#include <string>
#include <utility>
#include <algorithm>
#include <cmath>
#include <stdexcept>


Zone::Zone(
    std::string id,
    ZoneType zoneType,
    std::vector<Position> vertices)
    : id_(std::move(id)),
      type_(zoneType),
      enabled_(zoneType != ZoneType::Disabled),
      vertices_(std::move(vertices))
{
    if (vertices_.size() < 4)
    {
        throw std::invalid_argument(
            "A zone needs three vertices and a closing point.");
    }

    const Position &first = vertices_.front();
    const Position &last = vertices_.back();

    if (first.latDeg != last.latDeg || first.lonDeg != last.lonDeg)
    {
        throw std::invalid_argument(
            "The last polygon vertex must repeat the first vertex.");
    }
}

const std::string& Zone::getId() const{
    return id_;
}

bool Zone::isEnabled() const{
    return enabled_;
}

ZoneType Zone::getZoneType() const{
    return type_;
}

/// usual ray-casting algorithm for point-in-polygon, with a tolerance for points on edges or vertices
bool Zone::containsPosition(const Position &position) const
{

    const double x = position.lonDeg;
    const double y = position.latDeg;
    const double tolerance = 1e-9;

    bool inside = false;

    for (std::size_t i = 0; i < vertices_.size(); ++i)
    {
        const Position &a = vertices_[i];
        const Position &b = vertices_[(i + 1) % vertices_.size()];

        const double ax = a.lonDeg;
        const double ay = a.latDeg;
        const double bx = b.lonDeg;
        const double by = b.latDeg;

        // Find the closest point on this edge to the aircraft position.
        const double dx = bx - ax;
        const double dy = by - ay;
        const double lengthSquared = dx * dx + dy * dy;

        double closestX = ax;
        double closestY = ay;

        if (lengthSquared > 0.0)
        {
            const double projection =
                ((x - ax) * dx + (y - ay) * dy) / lengthSquared;

            const double t = std::clamp(projection, 0.0, 1.0);

            closestX = ax + t * dx;
            closestY = ay + t * dy;
        }

        // Points on an edge or vertex count as inside.
        if (std::hypot(x - closestX, y - closestY) <= tolerance)
        {
            return true;
        }

        // Check whether an eastward ray crosses this edge.
        if ((ay > y) != (by > y))
        {
            const double crossingX =
                ax + (y - ay) * (bx - ax) / (by - ay);

            if (crossingX > x)
            {
                inside = !inside;
            }
        }
    }

    return inside;
}

std::optional<double> Zone::firstEntryFraction(
    const Position &start,
    const Position &end) const
{
    if (containsPosition(start))
    {
        return 0.0;
    }

    const double rx = end.lonDeg - start.lonDeg;
    const double ry = end.latDeg - start.latDeg;
    const double lengthSquared = rx * rx + ry * ry;

    if (lengthSquared == 0.0)
    {
        return std::nullopt;
    }

    const double pathLength = std::sqrt(lengthSquared);

    // Coordinate differences below this count as boundary contact.
    // 1e-9 degrees is approximately 0.11 mm in latitude.
    constexpr double coordinateToleranceDegrees = 1e-9;

    // Treat segments as parallel when the absolute sine of their
    // angle is below this threshold. Allows for floating-point rounding.
    constexpr double parallelSinTolerance = 1e-12;

    const double pathTolerance = coordinateToleranceDegrees / pathLength;

    const auto cross = [](double ax, double ay, double bx, double by)
    {
        return ax * by - ay * bx;
    };

    std::optional<double> firstEntry;

    for (std::size_t i = 0; i + 1 < vertices_.size(); ++i)
    {
        const Position &a = vertices_[i];
        const Position &b = vertices_[i + 1];

        const double sx = b.lonDeg - a.lonDeg;
        const double sy = b.latDeg - a.latDeg;
        const double edgeLength = std::hypot(sx, sy);

        if (edgeLength == 0.0)
        {
            continue;
        }

        const double qx = a.lonDeg - start.lonDeg;
        const double qy = a.latDeg - start.latDeg;

        const double denominator = cross(rx, ry, sx, sy);
        double entry = 0.0;

        if (std::abs(denominator) <=
            parallelSinTolerance * pathLength * edgeLength)
        {
            // Parallel segments only meet if they lie on the same line.
            const double distanceToLine =
                std::abs(cross(qx, qy, rx, ry)) / pathLength;

            if (distanceToLine > coordinateToleranceDegrees)
            {
                continue;
            }

            // Find the overlapping part when travelling along an edge.
            const double t0 =
                (qx * rx + qy * ry) / lengthSquared;

            const double t1 =
                t0 + (sx * rx + sy * ry) / lengthSquared;

            entry = std::max(0.0, std::min(t0, t1));
            const double exit = std::min(1.0, std::max(t0, t1));

            if (entry > exit + pathTolerance)
            {
                continue;
            }
        }
        else
        {
            // t is the intersection's fraction along the aircraft path.
            // u is its fraction along this polygon edge.
            const double t = cross(qx, qy, sx, sy) / denominator;
            const double u = cross(qx, qy, rx, ry) / denominator;

            const double edgeTolerance =
                coordinateToleranceDegrees / edgeLength;

            if (t < -pathTolerance || t > 1.0 + pathTolerance ||
                u < -edgeTolerance || u > 1.0 + edgeTolerance)
            {
                continue;
            }

            entry = t;
        }

        entry = std::clamp(entry, 0.0, 1.0);

        if (!firstEntry || entry < *firstEntry)
        {
            firstEntry = entry;
        }
    }

    return firstEntry;
}





