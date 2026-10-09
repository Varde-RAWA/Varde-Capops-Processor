#pragma once

#include "config/Config.hpp"
#include "domain/Track.hpp"
#include "domain/types/ProcessingResult.hpp"
#include "proto/FlightData.pb.h"

varde::events::TrackProto mapToProto(const Track &track);

varde::events::FlightDataProto mapToProto(const ProcessingResult &result,
                           const Configuration &config);