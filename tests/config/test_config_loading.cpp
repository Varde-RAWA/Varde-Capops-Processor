#include "config/ConfigLoader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

namespace
{
const std::string bounds =
    "[grid]\nminLat=59\nmaxLat=61\nminLon=4\nmaxLon=6\ncellSizeDeg=0.1\n";

struct ConfigFile
{
    QTemporaryDir directory;
    std::string path;
    explicit ConfigFile(const std::string& text)
    {
        REQUIRE(directory.isValid());
        path = directory.filePath("configuration.cfg").toStdString();
        std::ofstream file(path);
        REQUIRE(file.is_open());
        file << text;
        file.close();
        REQUIRE(file.good());
    }
    ConfigurationValues load(const std::map<std::string, std::string>& env = {}) const
    {
        return loadConfigurationValues(path, [&env](const std::string& key) -> std::optional<std::string>
        {
            const auto found = env.find(key);
            if (found == env.end()) return std::nullopt;
            return found->second;
        });
    }
};
}

TEST_CASE("Config loader applies defaults and computes dimensions")
{
    ConfigFile file(bounds);
    const auto values = file.load();
    REQUIRE(values.grid.rows == 20);
    REQUIRE(values.grid.cols == 10);
    REQUIRE(values.protobufVersion == 3);
    REQUIRE(values.timestepSize == 0.5);
    REQUIRE(values.loopIntervalMs == 500);
    REQUIRE(values.redisChannel == "flightdata");
}

TEST_CASE("Environment overrides file values and absent optional keys")
{
    ConfigFile file(bounds + "[redis]\nredisChannel=file-channel\n");
    const auto values = file.load({{"REDIS_REDISCHANNEL", "env-channel"},
                                  {"EXECUTION_LOOPINTERVALMS", "750"}});
    REQUIRE(values.redisChannel == "env-channel");
    REQUIRE(values.loopIntervalMs == 750);
    REQUIRE_THROWS_AS(file.load({{"REDIS_REDISCHANNEL", ""}}), std::invalid_argument);
}

TEST_CASE("Loader rejects malformed entries before activation")
{
    std::string extra;
    SECTION("Unknown key") { extra = "[redis]\nredisChanel=typo\n"; }
    SECTION("Unknown section") { extra = "[redsi]\nredisChannel=typo\n"; }
    SECTION("Malformed line") { extra = "[redis]\nredisChannel\n"; }
    SECTION("Duplicate") { extra = "[redis]\nredisChannel=a\nredisChannel=b\n"; }
    SECTION("Trailing number text") { extra = "[execution]\nloopIntervalMs=500oops\n"; }
    SECTION("Integer overflow") { extra = "[execution]\nloopIntervalMs=9999999999999999\n"; }
    ConfigFile file(bounds + extra);
    REQUIRE_THROWS_AS(file.load(), std::invalid_argument);
}

TEST_CASE("Validation rejects unusable settings")
{
    ConfigFile file(bounds);
    std::map<std::string, std::string> env;
    SECTION("Missing bound") { env["GRID_MINLAT"] = "nan"; }
    SECTION("Reversed bounds") { env["GRID_MINLAT"] = "62"; }
    SECTION("Zero cell size") { env["GRID_CELLSIZEDEG"] = "0"; }
    SECTION("Grid overflow") { env["GRID_CELLSIZEDEG"] = "1e-300"; }
    SECTION("Negative loop delay") { env["EXECUTION_LOOPINTERVALMS"] = "-1"; }
    SECTION("Nonfinite simulation step") { env["SIMULATION_TIMESTEPSIZE"] = "inf"; }
    SECTION("Unknown source") { env["DATASOURCE_TYPE"] = "other"; }
    SECTION("Invalid Redis URL") { env["REDIS_REDISURL"] = "tcp://"; }
    SECTION("Invalid API URL") { env["ZONES_SOURCE"] = "api"; env["ZONES_APIURL"] = "broken"; }
    SECTION("Unsupported contract") { env["PROTOBUFVERSION_VERSION"] = "4"; }
    REQUIRE_THROWS_AS(file.load(env), std::invalid_argument);
}

TEST_CASE("Missing bounds fail and conversion errors identify their source")
{
    ConfigFile missing("[grid]\nminLat=59\n");
    REQUIRE_THROWS_WITH(missing.load(), Catch::Matchers::ContainsSubstring("all four finite bounds"));
    ConfigFile file(bounds + "[execution]\nloopIntervalMs=bad\n");
    REQUIRE_THROWS_WITH(file.load(), Catch::Matchers::ContainsSubstring("configuration.cfg:8 (execution.loopIntervalMs)"));
    REQUIRE_THROWS_WITH(file.load({{"EXECUTION_LOOPINTERVALMS", "bad"}}),
        Catch::Matchers::ContainsSubstring("EXECUTION_LOOPINTERVALMS"));
}

TEST_CASE("Existing inline comments remain supported")
{
    ConfigFile file(bounds + "[redis] # section comment\nredisChannel=test#channel # comment\n");
    REQUIRE(file.load().redisChannel == "test#channel");
}

TEST_CASE("Reload is transactional and recalculates dimensions")
{
    ConfigFile first(bounds + "[redis]\nredisChannel=first\n");
    Configuration config(first.path);
    ConfigFile invalid(bounds + "[execution]\nloopIntervalMs=-1\n");
    REQUIRE_THROWS_AS(config.load(invalid.path), std::invalid_argument);
    REQUIRE(config.getRedisChannel() == "first");
    REQUIRE(config.grid().rows == 20);

    ConfigFile second("[grid]\nminLat=59\nmaxLat=60\nminLon=4\nmaxLon=6\ncellSizeDeg=0.1\n");
    config.load(second.path);
    REQUIRE(config.grid().rows == 10);
    REQUIRE(config.getRedisChannel() == "flightdata");
}
