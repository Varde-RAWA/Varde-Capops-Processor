#pragma once


#include "domain/types/Position.hpp"
#include "domain/Track.hpp"


class VardeEngine{
    private:
    public:
        VardeEngine() = default;
        Position predictPosition(const Track &track, double secondsAhead = 120.0) const;
};


