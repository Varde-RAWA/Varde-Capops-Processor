#include "publish/ProtoMapper.hpp"

#include "utils/time/IsoTimestamp.hpp"

#include <string>


SectorSummaryProto mapToProto(const SectorSummary &summary)
{
    SectorSummaryProto proto;

    proto.set_sectorid(summary.getSectorId());
    proto.set_row(summary.getRow());
    proto.set_column(summary.getColumn());
    proto.set_localaircraftcount(summary.getLocalAircraftCount());
    for (const auto &icao : summary.getIcao24List())
    {
        proto.add_icao24list(icao);
    }
    return proto;
}

TrackProto mapToProto(const Track &track)
{
    TrackProto proto;

    proto.set_icao24(track.getIcao());
    proto.set_timestamp(track.getTimestamp());
    proto.set_headingdegrees(track.getHeadingDegrees());
    proto.set_groundtrackdegrees(track.getGroundTrackDegrees());

    PositionProto *position = proto.mutable_position();
    position->set_latitudedegrees(track.getPosition().latDeg);
    position->set_longitudedegrees(track.getPosition().lonDeg);
    position->set_altitudefeet(track.getAltitudeFeet());

    VelocityProto *velocity = proto.mutable_velocity();
    velocity->set_groundspeedknots(track.getGroundSpeedKnots());
    velocity->set_verticalspeedfeetperminute(track.getVerticalSpeedFeetPerMinute());

    return proto;
}

FlightDataProto mapToProto(const ProcessingResult &result, const Configuration &config,
                           const GridConfig &grid)
{
    FlightDataProto proto;

    // Metadata
    proto.mutable_metadata()->set_version(config.getProtobufVersion());
    proto.mutable_metadata()->set_timestamp(createIsoTimestamp());

    // Sector summaries
    SectorSummaryDataProto *sectorSummaryData = proto.mutable_sectorsummarydata();
    sectorSummaryData->set_rowscount(grid.rows);
    sectorSummaryData->set_columnscount(grid.cols);
    sectorSummaryData->set_minlongitude(grid.minLon);
    sectorSummaryData->set_maxlongitude(grid.maxLon);
    sectorSummaryData->set_minlatitude(grid.minLat);
    sectorSummaryData->set_maxlatitude(grid.maxLat);

    for (const auto &summary : result.sectorSummaries)
    {
        *sectorSummaryData->add_sectorsummaries() = mapToProto(summary);
    }

    // Tracks
    TrackDataProto *trackData = proto.mutable_trackdata();
    trackData->set_totalaircraftcount(static_cast<int>(result.tracks.size()));
    trackData->set_coordinatesystem(config.getCoordinateSystem());

    for (const auto &track : result.tracks)
    {
        *trackData->add_tracks() = mapToProto(track);
    }

    return proto;
}