#include "config/ConfigLoader.hpp"

#include <QUrl>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
struct Entry
{
    std::string text;
    std::string source;
};
using Entries = std::map<std::string, Entry>;

std::string trim(std::string text)
{
    const auto space = [](unsigned char c) { return std::isspace(c); };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), space));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), space).base(), text.end());
    return text;
}

Entries readIni(const std::string& path, const std::set<std::string>& keys)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error(path + ": cannot open configuration file");
    std::set<std::string> sections;
    for (const auto& key : keys) sections.insert(key.substr(0, key.find('.')));
    Entries entries;
    std::string section, line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        const auto source = path + ":" + std::to_string(++lineNumber);
        // Inline comments start at # after whitespace; embedded # is a literal.
        for (std::size_t i = 0; i < line.size(); ++i)
            if (line[i] == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))))
            { line.resize(i); break; }
        line = trim(std::move(line));
        if (line.empty()) continue;
        if (line.front() == '[')
        {
            if (line.back() != ']') throw std::invalid_argument(source + ": malformed section");
            section = trim(line.substr(1, line.size() - 2));
            if (!sections.count(section)) throw std::invalid_argument(source + ": unknown section " + section);
            continue;
        }
        const auto equals = line.find('=');
        if (section.empty() || equals == std::string::npos)
            throw std::invalid_argument(source + ": expected key=value inside a section");
        const auto key = section + "." + trim(line.substr(0, equals));
        if (!keys.count(key)) throw std::invalid_argument(source + ": unknown key " + key);
        if (!entries.emplace(key, Entry{trim(line.substr(equals + 1)), source + " (" + key + ")"}).second)
            throw std::invalid_argument(source + ": duplicate key " + key);
    }
    if (file.bad()) throw std::runtime_error(path + ": failed while reading configuration");
    return entries;
}

int integer(const std::string& text)
{
    std::size_t used = 0;
    int value = 0;
    try { value = std::stoi(text, &used); }
    catch (const std::exception&) { throw std::invalid_argument("expected a representable integer"); }
    if (used != text.size()) throw std::invalid_argument("expected an integer with no trailing text");
    return value;
}

double number(const std::string& text)
{
    std::size_t used = 0;
    double value = 0.0;
    try { value = std::stod(text, &used); }
    catch (const std::exception&) { throw std::invalid_argument("expected a finite number"); }
    if (used != text.size() || !std::isfinite(value))
        throw std::invalid_argument("expected a finite number with no trailing text");
    return value;
}

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::invalid_argument(message);
}

void requireUrl(const std::string& text, const std::string& key, bool redis)
{
    const QUrl url(QString::fromStdString(text), QUrl::StrictMode);
    const bool scheme = redis ? url.scheme() == "tcp" :
        (url.scheme() == "http" || url.scheme() == "https");
    require(url.isValid() && scheme && !url.host().isEmpty() && !url.hasFragment() &&
        url.port(redis ? 6379 : 80) > 0 && url.port(redis ? 6379 : 80) <= 65535,
        key + (redis ? " must be a valid tcp Redis URL" : " must be a valid HTTP(S) URL"));
}
} // namespace

std::optional<std::string> configurationEnvironment(const std::string& name)
{
    if (const char* value = std::getenv(name.c_str())) return std::string(value);
    return std::nullopt;
}

ConfigurationValues loadConfigurationValues(const std::string& path, EnvironmentLookup environment)
{
    ConfigurationValues values;
    // One binding per existing file key. All supported environment names derive from this table.
    const std::map<std::string, std::function<void(const std::string&)>> bindings{
        {"grid.minLat", [&](const auto& s) { values.grid.minLat = number(s); }},
        {"grid.maxLat", [&](const auto& s) { values.grid.maxLat = number(s); }},
        {"grid.minLon", [&](const auto& s) { values.grid.minLon = number(s); }},
        {"grid.maxLon", [&](const auto& s) { values.grid.maxLon = number(s); }},
        {"grid.cellSizeDeg", [&](const auto& s) { values.grid.cellSizeDeg = number(s); }},
        {"grid.coordinateSystem", [&](const auto& s) { values.coordinateSystem = s; }},
        {"protobufVersion.version", [&](const auto& s) { values.protobufVersion = integer(s); }},
        {"redis.redisUrl", [&](const auto& s) { values.redisUrl = s; }},
        {"redis.redisChannel", [&](const auto& s) { values.redisChannel = s; }},
        {"dataSource.type", [&](const auto& s)
            {
                require(s == "sim" || s == "api", "expected sim or api");
                values.sourceType = s == "sim" ? SourceType::Simulation : SourceType::Api;
            }},
        {"simulation.numFlights", [&](const auto& s) { values.numFlights = integer(s); }},
        {"simulation.timestepSize", [&](const auto& s) { values.timestepSize = number(s); }},
        {"execution.loopIntervalMs", [&](const auto& s) { values.loopIntervalMs = integer(s); }},
        {"zones.source", [&](const auto& s) { values.zoneSource = s; }},
        {"zones.apiUrl", [&](const auto& s) { values.zoneApiUrl = s; }},
        {"zones.file", [&](const auto& s) { values.zoneFile = s; }},
        {"zones.avoidanceApiUrl", [&](const auto& s) { values.avoidanceApiUrl = s; }},
        {"zones.avoidanceFile", [&](const auto& s) { values.avoidanceFile = s; }},
        {"zones.refreshSeconds", [&](const auto& s) { values.zoneRefreshSeconds = integer(s); }}
    };
    std::set<std::string> keys;
    for (const auto& binding : bindings) keys.insert(binding.first);
    auto entries = readIni(path, keys);
    for (const auto& [key, assign] : bindings)
    {
        auto envName = key;
        std::replace(envName.begin(), envName.end(), '.', '_');
        std::transform(envName.begin(), envName.end(), envName.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (environment)
            if (auto override = environment(envName)) entries[key] = {trim(*override), envName};
        const auto entry = entries.find(key);
        if (entry == entries.end()) continue;
        try { assign(entry->second.text); }
        catch (const std::invalid_argument& error)
        { throw std::invalid_argument(entry->second.source + ": " + error.what()); }
    }
    try { validateConfiguration(values); }
    catch (const std::invalid_argument& error)
    { throw std::invalid_argument(path + ": " + error.what()); }
    return values;
}

void validateConfiguration(ConfigurationValues& values)
{
    require(values.protobufVersion == 3, "protobufVersion.version must be 3 for this build");
    require(values.coordinateSystem == "WGS84", "grid.coordinateSystem must be WGS84 for this build");
    requireUrl(values.redisUrl, "redis.redisUrl", true);
    require(!values.redisChannel.empty(), "redis.redisChannel must not be empty");
    require(values.sourceType == SourceType::Simulation || values.sourceType == SourceType::Api,
        "dataSource.type must be sim or api");
    require(values.numFlights > 0, "simulation.numFlights must be greater than zero");
    require(std::isfinite(values.timestepSize) && values.timestepSize > 0.0,
        "simulation.timestepSize must be finite and greater than zero");
    require(values.loopIntervalMs > 0, "execution.loopIntervalMs must be greater than zero");
    require(values.zoneSource == "file" || values.zoneSource == "api", "zones.source must be file or api");
    require(!values.zoneFile.empty(), "zones.file must not be empty");
    require(!values.avoidanceFile.empty(), "zones.avoidanceFile must not be empty");
    require(values.zoneRefreshSeconds > 0, "zones.refreshSeconds must be greater than zero");
    if (values.zoneSource == "api")
    {
        requireUrl(values.zoneApiUrl, "zones.apiUrl", false);
        requireUrl(values.avoidanceApiUrl, "zones.avoidanceApiUrl", false);
    }
    values.grid.computeDimensions();
}