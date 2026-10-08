#pragma once

#include "compute/Grid.hpp"
#include "domain/SectorSummary.hpp"
#include "domain/Track.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "proto/FlightData.pb.h"

SectorSummaryProto mapToProto(const SectorSummary &summary);
TrackProto mapToProto(const Track &track);

FlightDataProto mapToProto(const ProcessingResult &result, const Configuration &config,
                           const GridConfig &grid);