#include "ingest/IngestService.hpp"
#include <optional>
#include <stdexcept>

IngestService::IngestService(GridConfig config, ITrackSource *trackSource)
    : config_(config), trackSource_(trackSource)
{
}

Track IngestService::getTrack(const std::string &icao24) const
{
    std::optional<Track> found = trackSource_->getTrack(icao24);

    if (!found.has_value())
    {
        throw std::runtime_error("Track with ICAO24 " + icao24 + " not found");
    }

    return found.value();
}

std::vector<Track> IngestService::getAllTracks() const
{
    return trackSource_->getAllTracks();
}

