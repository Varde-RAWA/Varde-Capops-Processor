#pragma once

#include "domain/Zone.hpp"

#include <QByteArray>
#include <string>
#include <vector>

class ZoneSourceGeoService
{
public:
    std::vector<Zone> getActiveZones(const std::string &url) const;

    static std::vector<Zone> loadFile(const std::string &path);
    static std::vector<Zone> parseActiveZones(const QByteArray &data);
};






