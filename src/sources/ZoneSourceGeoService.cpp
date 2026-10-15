#include "sources/ZoneSourceGeoService.hpp"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <set>
#include <stdexcept>

namespace
{
QJsonObject readCollection(const QByteArray &data)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("Invalid zone JSON.");

    const auto root = document.object();

    if (root.value("type").toString() != "FeatureCollection" ||
        !root.value("features").isArray())
    {
        throw std::runtime_error("Expected a GeoJSON FeatureCollection.");
    }

    return root;
}

ZoneType parseType(const QString &type)
{
    if (type == "DANGER")
        return ZoneType::Danger;
    if (type == "RESTRICTED")
        return ZoneType::Restricted;
    if (type == "TEMPORARY")
        return ZoneType::Temporary;

    throw std::runtime_error("Unknown zone type.");
}

QByteArray get(QNetworkAccessManager &network, const QUrl &url)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QNetworkRequest request{url};
    request.setRawHeader("Accept", "application/geo+json");

    QNetworkReply *reply = network.get(request);

    QObject::connect(reply, &QNetworkReply::finished,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,
                     reply, &QNetworkReply::abort);

    timeout.start(5000);
    loop.exec();
    timeout.stop();

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const bool success =
        reply->error() == QNetworkReply::NoError &&
        status >= 200 && status < 300;

    const std::string error = reply->errorString().toStdString();
    const QByteArray data = reply->readAll();
    delete reply;

    if (!success)
        throw std::runtime_error("Zone request failed: " + error);

    return data;
}
} // namespace

std::vector<Zone> ZoneSourceGeoService::parseActiveZones(
    const QByteArray &data)
{
    const auto root = readCollection(data);
    std::vector<Zone> zones;

    for (const auto &value : root.value("features").toArray())
    {
        const auto feature = value.toObject();
        const auto properties = feature.value("properties").toObject();

        if (!properties.value("active").isBool())
            throw std::runtime_error("Zone is missing its active flag.");

        if (!properties.value("active").toBool())
            continue;

        const std::string id = feature.value("id").toString().toStdString();

        if (id.empty())
            throw std::runtime_error("Zone is missing its ID.");

        const auto geometry = feature.value("geometry").toObject();
        const auto rings = geometry.value("coordinates").toArray();

        // The current Zone class supports one polygon ring.
        if (geometry.value("type").toString() != "Polygon" ||
            rings.size() != 1 || !rings.at(0).isArray())
        {
            throw std::runtime_error(
                "Expected a Polygon with one ring.");
        }

        std::vector<Position> vertices;

        for (const auto &point : rings.at(0).toArray())
        {
            const auto coordinate = point.toArray();

            if (coordinate.size() < 2 ||
                !coordinate.at(0).isDouble() ||
                !coordinate.at(1).isDouble())
            {
                throw std::runtime_error("Invalid zone coordinate.");
            }

            // GeoJSON is longitude, latitude.
            // Position is latitude, longitude.
            vertices.push_back({
                coordinate.at(1).toDouble(),
                coordinate.at(0).toDouble()
            });
        }

        zones.emplace_back(
            id,
            parseType(properties.value("type").toString()),
            std::move(vertices));
    }

    return zones;
}

std::vector<Zone> ZoneSourceGeoService::loadFile(
    const std::string &path)
{
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Could not open zone file: " + path);

    return parseActiveZones(file.readAll());
}

std::vector<Zone> ZoneSourceGeoService::getActiveZones(
    const std::string &url) const
{
    QNetworkAccessManager network;
    QUrl next(QString::fromStdString(url));

    std::set<QString> visitedPages;
    std::set<std::string> zoneIds;
    std::vector<Zone> zones;

    while (!next.isEmpty())
    {
        if (!visitedPages.insert(next.toString()).second)
            throw std::runtime_error("Repeated zone pagination link.");

        const QByteArray data = get(network, next);

        for (auto &zone : parseActiveZones(data))
        {
            if (!zoneIds.insert(zone.getId()).second)
                throw std::runtime_error("Duplicate zone ID.");

            zones.push_back(std::move(zone));
        }

        const auto root = readCollection(data);
        QUrl followingPage;

        for (const auto &value : root.value("links").toArray())
        {
            const auto link = value.toObject();

            if (link.value("rel").toString() == "next")
            {
                followingPage = next.resolved(
                    QUrl(link.value("href").toString()));

                if (followingPage.isEmpty())
                    throw std::runtime_error("Invalid next-page link.");

                break;
            }
        }

        next = followingPage;
    }

    return zones;
}



