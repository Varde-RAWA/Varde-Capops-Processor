#include "publish/ProtoMapper.hpp"

#include "utils/time/IsoTimestamp.hpp"

#include <string>


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

FlightDataProto mapToProto(const ProcessingResult &result,
                           const Configuration &config)
{
    FlightDataProto proto;

    // Metadata
    proto.mutable_metadata()->set_version(config.getProtobufVersion());
    proto.mutable_metadata()->set_timestamp(createIsoTimestamp());

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