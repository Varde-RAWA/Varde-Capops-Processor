#include "domain/Zone.hpp"

#include <string>
#include <utility>
#include <algorithm>
#include <cmath>
#include <stdexcept>


Zone::Zone(std::string id, ZoneType zoneType, std::vector<Position> vertices) : 
    id_(std::move(id)), type_(zoneType), vertices_(std::move(vertices)){
        
        const Position &first = vertices_.front();
        const Position &last = vertices_.back();
        if (first.latDeg != last.latDeg || first.lonDeg != last.lonDeg) {
            throw std::invalid_argument("The last polygon vertex must repeat the first vertex.");
        }
        if (vertices_.size() < 3 + 1) {
            throw std::invalid_argument("A zone must have at least 3 vertices.");
        }

        enabled_ = (zoneType != ZoneType::Disabled);
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

