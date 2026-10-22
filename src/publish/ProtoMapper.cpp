#include "publish/ProtoMapper.hpp"

#include "utils/time/IsoTimestamp.hpp"

#include <string>
#include <stdexcept>


varde::events::TrackProto mapToProto(const Track &track)
{
    varde::events::TrackProto proto;

    proto.set_icao24(track.getIcao());
    proto.set_timestamp(track.getTimestamp());
    proto.set_headingdegrees(track.getHeadingDegrees());
    proto.set_groundtrackdegrees(track.getGroundTrackDegrees());

    varde::events::PositionProto *position = proto.mutable_position();
    position->set_latitudedegrees(track.getPosition().latDeg);
    position->set_longitudedegrees(track.getPosition().lonDeg);
    position->set_altitudefeet(track.getAltitudeFeet());

    varde::events::VelocityProto *velocity = proto.mutable_velocity();
    velocity->set_groundspeedknots(track.getGroundSpeedKnots());
    velocity->set_verticalspeedfeetperminute(track.getVerticalSpeedFeetPerMinute());

    return proto;
}

varde::events::FlightDataProto mapToProto(  const ProcessingResult &result,
                                            const Configuration &config)
{
    varde::events::FlightDataProto proto;

    // Metadata
    proto.mutable_metadata()->set_version(config.getProtobufVersion());
    proto.mutable_metadata()->set_timestamp(createIsoTimestamp());

    // Tracks
    varde::events::TrackDataProto *trackData = proto.mutable_trackdata();
    trackData->set_totalaircraftcount(static_cast<int>(result.tracks.size()));
    trackData->set_coordinatesystem(config.getCoordinateSystem());

    for (const auto &track : result.tracks)
    {
        *trackData->add_tracks() = mapToProto(track);
    }

    return proto;
}

varde::events::ZoneAlertEvent mapToProto(   const AlertEvent &event,
                                            const Configuration &config)
{
    varde::events::ZoneAlertEvent proto;

    proto.mutable_metadata()->set_version(config.getProtobufVersion());
    proto.mutable_metadata()->set_timestamp(createIsoTimestamp());

    proto.set_alertid(event.alertId);
    proto.set_icao24(event.icao);
    proto.set_zoneid(event.zoneId);

    switch (event.level)
    {
        case WarningLevel::NONE:
            proto.set_level(varde::events::WARNING_LEVEL_NONE);
            break;
        case WarningLevel::PREDICTED:
            proto.set_level(varde::events::WARNING_LEVEL_PREDICTED);
            break;
        case WarningLevel::IMMINENT:
            proto.set_level(varde::events::WARNING_LEVEL_IMMINENT);
            break;
        case WarningLevel::INFRINGEMENT:
            proto.set_level(varde::events::WARNING_LEVEL_INFRINGEMENT);
            break;
        default:
            throw std::invalid_argument("Unknown warning level.");
    }

    if (event.timeToEntrySeconds.has_value())
    {
        proto.set_timetoentryseconds(*event.timeToEntrySeconds);
    }

    switch (event.type)
    {
        case AlertEventType::OPENED:
            proto.set_eventtype(varde::events::ZONE_ALERT_EVENT_OPENED);
            break;
        case AlertEventType::LEVEL_CHANGED:
            proto.set_eventtype(
                varde::events::ZONE_ALERT_EVENT_LEVEL_CHANGED);
            break;
        case AlertEventType::CLOSED:
            proto.set_eventtype(varde::events::ZONE_ALERT_EVENT_CLOSED);
            break;
        default:
            throw std::invalid_argument("Unknown alert event type.");
    }

    return proto;
}

varde::events::VektorProposalEvent mapToProto(
    const VektorProposal &proposal, const Configuration &config)
{
    varde::events::VektorProposalEvent proto;
    proto.mutable_metadata()->set_version(config.getProtobufVersion());
    proto.mutable_metadata()->set_timestamp(createIsoTimestamp());
    proto.set_proposalid(proposal.proposalId);
    proto.set_alertid(proposal.alertId);
    proto.set_icao24(proposal.icao);
    proto.set_zoneid(proposal.zoneId);
    for (Position position : proposal.detour.waypoints)
    {
        auto *waypoint = proto.add_waypoints();
        waypoint->set_longitudedegrees(position.lonDeg);
        waypoint->set_latitudedegrees(position.latDeg);
    }
    proto.set_extradistancekm(proposal.detour.extraDistanceKm);
    proto.set_extratimeseconds(proposal.detour.extraTimeSeconds);
    return proto;
}
