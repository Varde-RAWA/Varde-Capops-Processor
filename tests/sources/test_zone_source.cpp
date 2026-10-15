#include "sources/ZoneSourceGeoService.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <stdexcept>

namespace
{
QJsonObject makeZoneFeature(
    const QString &id,
    const QString &type,
    bool active)
{
    auto feature = QJsonDocument::fromJson(R"json(
    {
        "type": "Feature",
        "geometry": {
            "type": "Polygon",
            "coordinates": [
                [
                    [5.00, 59.95],
                    [5.20, 59.95],
                    [5.20, 60.05],
                    [5.00, 60.05],
                    [5.00, 59.95]
                ]
            ]
        }
    }
    )json").object();

    feature["id"] = id;
    feature["properties"] = QJsonObject{
        {"type", type},
        {"active", active}
    };

    return feature;
}

QByteArray makeZoneCollection(const QJsonArray &features)
{
    return QJsonDocument(
        QJsonObject{
            {"type", "FeatureCollection"},
            {"features", features}
        }
    ).toJson();
}
}

TEST_CASE("Zone source parses active zones and preserves their information")
{
    const QByteArray data = makeZoneCollection({
        makeZoneFeature("D-101", "DANGER", true),
        makeZoneFeature("R-201", "RESTRICTED", true),
        makeZoneFeature("T-301", "TEMPORARY", true),
        makeZoneFeature("T-302", "TEMPORARY", false)
    });

    const auto zones = ZoneSourceGeoService::parseActiveZones(data);

    REQUIRE(zones.size() == 3);

    REQUIRE(zones[0].getId() == "D-101");
    REQUIRE(zones[0].getZoneType() == ZoneType::Danger);

    REQUIRE(zones[1].getId() == "R-201");
    REQUIRE(zones[1].getZoneType() == ZoneType::Restricted);

    REQUIRE(zones[2].getId() == "T-301");
    REQUIRE(zones[2].getZoneType() == ZoneType::Temporary);

    // GeoJSON uses longitude, latitude.
    // Position uses latitude, longitude.
    REQUIRE(zones[0].containsPosition(Position{60.0, 5.1}));
    REQUIRE_FALSE(zones[0].containsPosition(Position{5.1, 60.0}));
}

TEST_CASE("Zone source accepts a collection without active zones")
{
    QJsonArray features;

    SECTION("The collection is empty")
    {
        features = QJsonArray{};
    }

    SECTION("All zones are inactive")
    {
        features = QJsonArray{
            makeZoneFeature("D-101", "DANGER", false),
            makeZoneFeature("R-201", "RESTRICTED", false)
        };
    }

    const auto zones = ZoneSourceGeoService::parseActiveZones(
        makeZoneCollection(features));

    REQUIRE(zones.empty());
}

TEST_CASE("Zone source rejects unusable zone data")
{
    QByteArray data;

    SECTION("Malformed JSON")
    {
        data = "{";
    }

    SECTION("Missing active flag")
    {
        auto feature = makeZoneFeature("D-101", "DANGER", true);
        auto properties = feature["properties"].toObject();
        properties.remove("active");
        feature["properties"] = properties;

        data = makeZoneCollection({feature});
    }

    SECTION("Unknown zone type")
    {
        data = makeZoneCollection({
            makeZoneFeature("D-101", "UNKNOWN", true)
        });
    }

    SECTION("Geometry is a point instead of a polygon")
    {
        auto feature = makeZoneFeature("D-101", "DANGER", true);
        feature["geometry"] = QJsonObject{
            {"type", "Point"},
            {"coordinates", QJsonArray{5.1, 60.0}}
        };

        data = makeZoneCollection({feature});
    }

    REQUIRE_THROWS_AS(
        ZoneSourceGeoService::parseActiveZones(data),
        std::runtime_error);
}

TEST_CASE("Zone source loads usable zones from a local file")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QString path = directory.filePath("zones.geojson");
    const QByteArray data = makeZoneCollection({
        makeZoneFeature("D-101", "DANGER", true),
        makeZoneFeature("T-302", "TEMPORARY", false)
    });

    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data) == data.size());
    file.close();

    const auto zones =
        ZoneSourceGeoService::loadFile(path.toStdString());

    REQUIRE(zones.size() == 1);
    REQUIRE(zones[0].getId() == "D-101");
    REQUIRE(zones[0].containsPosition(Position{60.0, 5.1}));
}

TEST_CASE("Zone source reports an unusable local file")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const QString path = directory.filePath("zones.geojson");

    SECTION("The file does not exist")
    {
        REQUIRE_FALSE(QFile::exists(path));
    }

    SECTION("The file contains invalid JSON")
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write("{") == 1);
        file.close();
    }

    REQUIRE_THROWS_AS(
        ZoneSourceGeoService::loadFile(path.toStdString()),
        std::runtime_error);
}