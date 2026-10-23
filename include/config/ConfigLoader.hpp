#pragma once

#include "config/Config.hpp"
#include <functional>
#include <optional>
#include <string>

using EnvironmentLookup = std::function<std::optional<std::string>(const std::string&)>;

std::optional<std::string> configurationEnvironment(const std::string& name);

ConfigurationValues loadConfigurationValues(
    const std::string& path,
    EnvironmentLookup environment = configurationEnvironment);

void validateConfiguration(ConfigurationValues& values);