
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <stdexcept>

#include "sources/TrackSourceOpenSky.hpp"

static constexpr char BASE_URL[] = "https://opensky-network.org/api";
static constexpr double MS_TO_KNOTS = 1.94384;
static constexpr double MS_TO_FPM = 196.850;
static constexpr double METRES_TO_FEET = 3.28084;

TrackSourceOpenSky::TrackSourceOpenSky() : ITrackSource()
{
    networkManager_ = new QNetworkAccessManager();
}

void TrackSourceOpenSky::setRegion(GridConfig config)
{
    setRegion(config.minLat, config.maxLat, config.minLon, config.maxLon);
}

void TrackSourceOpenSky::setRegion(double minLat, double maxLat, double minLon, double maxLon)
{
    minLat_ = minLat;
    maxLat_ = maxLat;
    minLon_ = minLon;
    maxLon_ = maxLon;
}

QByteArray TrackSourceOpenSky::blockingGet(const QString &url) const
{
    QEventLoop loop;
    QNetworkReply *reply = networkManager_->get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError)
    {
        const QString msg = reply->errorString();
        reply->deleteLater();
        throw std::runtime_error(msg.toStdString());
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

std::optional<Track> TrackSourceOpenSky::getTrack(const std::string &icao) const
{
    const QString url =
        QString("%1/states/all?icao24=%2").arg(BASE_URL).arg(QString::fromStdString(icao));

    const QJsonArray states =
        QJsonDocument::fromJson(blockingGet(url)).object().value("states").toArray();

    for (const QJsonValue &state : states)
    {
        QJsonArray stateArray = state.toArray();

        if (stateArray.at(5).isNull() || stateArray.at(6).isNull()) // no position fix
            continue;

        ApiResponse response = parseStates(stateArray);

        return Track(
        response.icao24,
        response.timestamp,
        Position{response.latitude, response.longitude},
        response.baroAltitude * METRES_TO_FEET,
        response.velocity,
        response.verticalRate,
        response.trueTrack, 
        response.trueTrack 
        );
    }

    return std::nullopt;
}

std::vector<Track> TrackSourceOpenSky::getAllTracks() const
{
    const QString url = QString("%1/states/all?lamin=%2&lamax=%3&lomin=%4&lomax=%5")
                            .arg(BASE_URL)
                            .arg(minLat_)
                            .arg(maxLat_)
                            .arg(minLon_)
                            .arg(maxLon_);

    const QJsonArray states =
        QJsonDocument::fromJson(blockingGet(url)).object().value("states").toArray();

    std::vector<Track> tracks;
    tracks.reserve(states.size());
    for (const QJsonValue &state : states)
    {
        QJsonArray stateArray = state.toArray();

        if (stateArray.at(5).isNull() || stateArray.at(6).isNull()) // no position fix
            continue;

        ApiResponse parsed = parseStates(stateArray);

        tracks.emplace_back(
        parsed.icao24,
        parsed.timestamp,
        Position{parsed.latitude, parsed.longitude},
        parsed.baroAltitude * METRES_TO_FEET,
        parsed.velocity,
        parsed.verticalRate,
        parsed.trueTrack,
        parsed.trueTrack 
        );
    }

    return tracks;
}

TrackSourceOpenSky::ApiResponse TrackSourceOpenSky::parseStates(QJsonArray &states)
{
    ApiResponse parsed;

    QJsonValue timePosition = states.at(3);
    QJsonValue lastContact = states.at(4);

    int timeValue = timePosition.isNull() ? lastContact.toInt() : timePosition.toInt();

    parsed.timestamp = QDateTime::fromSecsSinceEpoch(timeValue, QTimeZone::utc())
                           .toString(Qt::ISODate)
                           .toStdString();

    parsed.icao24 = states.at(0).toString().toStdString();
    parsed.callsign = states.at(1).toString().trimmed().toStdString();
    parsed.longitude = states.at(5).toDouble();
    parsed.latitude = states.at(6).toDouble();
    parsed.baroAltitude = states.at(7).toDouble();
    parsed.onGround = states.at(8).toBool();
    parsed.velocity = states.at(9).toDouble() * MS_TO_KNOTS;
    parsed.trueTrack = states.at(10).toDouble();
    parsed.verticalRate = states.at(11).toDouble() * MS_TO_FPM;

    return parsed;
}