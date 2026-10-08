#pragma once

#include <vector>

#include "config/Config.hpp"

#include "domain/Track.hpp"
#include "domain/types/Position.hpp"

#include "sources/interfaces/ITrackSource.hpp"

class IngestService
{
  public:
    IngestService(GridConfig config, ITrackSource *trackSource);

    Track getTrack(const std::string &icao24) const;

    std::vector<Track> getAllTracks() const;

  private:
    GridConfig config_;
    ITrackSource *trackSource_;
};