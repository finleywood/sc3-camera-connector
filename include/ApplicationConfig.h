#pragma once

#include <json/json.hpp>
#include <string>
#include <fstream>

using json = nlohmann::json;

static json get_app_config(const std::string& config_path)
{
    std::ifstream f(config_path);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    json data = json::parse(f);
    return data;
}