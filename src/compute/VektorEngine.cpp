#include "compute/VektorEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr double epsilonKm = 1e-9;

struct Point
{
    double x, y;
    Position geographic;
};

double cross(const Point &a, const Point &b, const Point &c)
{
    return (b.x - a.x) * (c.y - a.y) -
           (b.y - a.y) * (c.x - a.x);
}

double distance(const Point &a, const Point &b)
{
    return std::hypot(b.x - a.x, b.y - a.y);
}

bool valid(Position p)
{
    return std::isfinite(p.latDeg) && std::isfinite(p.lonDeg) &&
           std::abs(p.latDeg) < 89.0 && std::abs(p.lonDeg) <= 180.0;
}

std::vector<Point> convexHull(std::vector<Point> points)
{
    std::sort(points.begin(), points.end(), [](const Point &a, const Point &b)
    {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    points.erase(std::unique(points.begin(), points.end(),
        [](const Point &a, const Point &b)
        { return a.x == b.x && a.y == b.y; }), points.end());
    if (points.size() < 3)
        return {};

    std::vector<Point> lower, upper;
    for (const Point &p : points)
    {
        while (lower.size() >= 2 &&
               cross(lower[lower.size() - 2], lower.back(), p) <= 0.0)
            lower.pop_back();
        lower.push_back(p);
    }
    for (auto it = points.rbegin(); it != points.rend(); ++it)
    {
        while (upper.size() >= 2 &&
               cross(upper[upper.size() - 2], upper.back(), *it) <= 0.0)
            upper.pop_back();
        upper.push_back(*it);
    }
    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower.size() >= 3 ? lower : std::vector<Point>{};
}

// Strict interior test for the original simple polygon, including concave ones.
// Boundary points are outside for routing purposes.
bool strictlyInside(const Point &p, const std::vector<Point> &polygon)
{
    bool inside = false;
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Point &a = polygon[i];
        const Point &b = polygon[(i + 1) % polygon.size()];
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double length2 = dx * dx + dy * dy;
        const double t = length2 > 0.0
            ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length2, 0.0, 1.0)
            : 0.0;
        if (std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy) <= epsilonKm)
            return false;
        if ((a.y > p.y) != (b.y > p.y) &&
            p.x < a.x + (p.y - a.y) * dx / dy)
            inside = !inside;
    }
    return inside;
}

bool crossesOriginalInterior(
    const Point &a, const Point &b, const std::vector<Point> &polygon)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length2 = dx * dx + dy * dy;
    if (length2 == 0.0)
        return strictlyInside(a, polygon);

    std::vector<double> cuts{0.0, 1.0};
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const Point &p = polygon[i];
        const Point &q = polygon[(i + 1) % polygon.size()];
        const double ex = q.x - p.x, ey = q.y - p.y;
        const double denominator = dx * ey - dy * ex;
        if (std::abs(denominator) > 1e-12 * std::sqrt(length2) * std::hypot(ex, ey))
        {
            const double t = ((p.x - a.x) * ey - (p.y - a.y) * ex) / denominator;
            const double u = ((p.x - a.x) * dy - (p.y - a.y) * dx) / denominator;
            if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0)
                cuts.push_back(t);
        }
        // Also splits collinear overlaps at their endpoints.
        if (std::abs(cross(a, b, p)) <= epsilonKm * std::sqrt(length2))
            cuts.push_back(std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length2, 0.0, 1.0));
    }
    std::sort(cuts.begin(), cuts.end());
    for (std::size_t i = 1; i < cuts.size(); ++i)
    {
        if (cuts[i] <= cuts[i - 1])
            continue;
        const double t = (cuts[i] + cuts[i - 1]) / 2.0;
        if (strictlyInside(Point{a.x + t * dx, a.y + t * dy, {}}, polygon))
            return true;
    }
    return false;
}

// Clip a segment against the strict interior of a counterclockwise hull.
// A tangent or a segment along an edge is permitted.
bool crossesHullInterior(
    const Point &a, const Point &b, const std::vector<Point> &hull)
{
    double low = 0.0, high = 1.0;
    for (std::size_t i = 0; i < hull.size(); ++i)
    {
        const Point &p = hull[i], &q = hull[(i + 1) % hull.size()];
        const double edgeLength = distance(p, q);
        const double da = cross(p, q, a) / edgeLength;
        const double db = cross(p, q, b) / edgeLength;
        if (da <= epsilonKm && db <= epsilonKm)
            return false;
        if (da > epsilonKm && db > epsilonKm)
            continue;
        const double t = (epsilonKm - da) / (db - da);
        if (db > da)
            low = std::max(low, t);
        else
            high = std::min(high, t);
        if (low >= high)
            return false;
    }
    return low < high;
}
} // namespace

std::optional<Detour> VektorEngine::calculateDetour(
    const Track &track, Position destination, const Zone &avoidance) const
{
    const Position start = track.getPosition();
    const double speed = track.getGroundSpeedKnots() * 1.852 / 3600.0;
    if (!avoidance.isEnabled() || !valid(start) || !valid(destination) ||
        !std::isfinite(speed) || speed <= 0.0)
        return std::nullopt;

    // One fixed local coordinate system for the entire route (VD-FUN-021).
    const double longitudeScale = 111.32 * std::cos(start.latDeg * std::acos(-1.0) / 180.0);
    const auto project = [&](Position p)
    {
        return Point{(p.lonDeg - start.lonDeg) * longitudeScale,
                     (p.latDeg - start.latDeg) * 111.32, p};
    };
    const Point p = project(start), d = project(destination);
    std::vector<Point> polygon;
    for (Position vertex : avoidance.getVertices())
    {
        if (!valid(vertex))
            return std::nullopt;
        polygon.push_back(project(vertex));
    }
    const auto hull = convexHull(polygon);
    if (hull.empty() || !crossesOriginalInterior(p, d, polygon))
        return std::nullopt;
    // There is no exterior hull route when an endpoint is inside the hull.
    if (crossesHullInterior(p, p, hull) || crossesHullInterior(d, d, hull))
        return std::nullopt;

    std::vector<Point> nodes{p, d};
    nodes.insert(nodes.end(), hull.begin(), hull.end());
    const std::size_t count = nodes.size();
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<std::vector<std::pair<std::size_t, double>>> edges(count);
    const auto connect = [&](std::size_t a, std::size_t b)
    {
        const double weight = distance(nodes[a], nodes[b]);
        edges[a].push_back({b, weight});
        edges[b].push_back({a, weight});
    };
    // Only hull edges and visible endpoint-to-hull links. The shortest path
    // therefore follows one of the two hull chains between tangent vertices.
    for (std::size_t i = 0; i < hull.size(); ++i)
    {
        connect(i + 2, (i + 1) % hull.size() + 2);
        if (!crossesHullInterior(p, hull[i], hull))
            connect(0, i + 2);
        if (!crossesHullInterior(d, hull[i], hull))
            connect(1, i + 2);
    }

    std::vector<double> best(count, infinity);
    std::vector<std::size_t> previous(count, count);
    std::vector<bool> visited(count, false);
    best[0] = 0.0;
    for (std::size_t step = 0; step < count; ++step)
    {
        std::size_t u = count;
        for (std::size_t i = 0; i < count; ++i)
            if (!visited[i] && (u == count || best[i] < best[u]))
                u = i;
        if (u == count || !std::isfinite(best[u]))
            break;
        if (u == 1)
            break;
        visited[u] = true;
        for (const auto &[v, weight] : edges[u])
            if (best[u] + weight < best[v])
            {
                best[v] = best[u] + weight;
                previous[v] = u;
            }
    }
    if (!std::isfinite(best[1]))
        return std::nullopt;

    Detour result;
    for (std::size_t v = previous[1]; v != 0; v = previous[v])
        result.waypoints.push_back(nodes[v].geographic);
    std::reverse(result.waypoints.begin(), result.waypoints.end());

    // Adding extra waypoint to show direction of flight after detour
    // Show approximately 60 seconds of flight after the detour.
    constexpr double continuationSeconds = 12.0;

    if (!result.waypoints.empty())
    {
        const Position last = result.waypoints.back();
        const double remainingKm = distance(project(last), d);

        if (remainingKm > epsilonKm)
        {
            const double fraction =
                std::min(1.0, speed * continuationSeconds / remainingKm);

            const Position directionPoint =
                fraction == 1.0
                    ? destination
                    : Position{
                          last.latDeg + fraction * (destination.latDeg - last.latDeg),
                          last.lonDeg + fraction * (destination.lonDeg - last.lonDeg)
                      };

            result.waypoints.push_back(directionPoint);
        }
    }

    result.extraDistanceKm = std::max(0.0, best[1] - distance(p, d));
    result.extraTimeSeconds = result.extraDistanceKm / speed;
    return result;
}
