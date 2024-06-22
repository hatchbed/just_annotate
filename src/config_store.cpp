#include <just_annotate/config_store.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

void ConfigState::addRecentFile(const std::string& path) {
    auto it = std::find(recent_files.begin(), recent_files.end(), path);
    if (it != recent_files.end()) {
        recent_files.erase(it);
    }

    recent_files.push_front(path);
    while(recent_files.size() > 10) {
        recent_files.pop_back();
    }
}

void ConfigState::addRecentVideo(const std::string& path) {
    auto it = std::find(recent_videos.begin(), recent_videos.end(), path);
    if (it != recent_videos.end()) {
        recent_videos.erase(it);
    }

    recent_videos.push_front(path);
    while(recent_videos.size() > 10) {
        recent_videos.pop_back();
    }
}

bool ConfigState::operator==(const ConfigState& other) const {
    return std::equal(recent_files.begin(), recent_files.end(), other.recent_files.begin(), other.recent_files.end())
        && std::equal(recent_videos.begin(), recent_videos.end(), other.recent_videos.begin(), other.recent_videos.end())
        && dark_mode == other.dark_mode
        && window_width == other.window_width
        && window_height == other.window_height
        && window_x == other.window_x
        && window_y == other.window_y;
}

bool ConfigState::operator!=(const ConfigState& other) const {
    return !(*this == other);
}

void to_json(json& j, const ConfigState& c) {
    j = json{{"dark_mode", c.dark_mode}, {"window_width", c.window_width},
             {"window_height", c.window_height}, {"window_x", c.window_x},
             {"window_y", c.window_y}, {"recent_files", c.recent_files},
             {"recent_videos", c.recent_videos}};
}

void from_json(const json& j, ConfigState& c) {
    j.at("dark_mode").get_to(c.dark_mode);
    j.at("window_width").get_to(c.window_width);
    j.at("window_height").get_to(c.window_height);
    j.at("window_x").get_to(c.window_x);
    j.at("window_y").get_to(c.window_y);

    if (j.contains("recent_files")) {
        j.at("recent_files").get_to(c.recent_files);
    } else {
        c.recent_files.clear();
    }

    if (j.contains("recent_videos")) {
        j.at("recent_videos").get_to(c.recent_videos);
    } else {
        c.recent_videos.clear();
    }
}


std::string expandTilde(const std::string& path) {
    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home || (home = std::getenv("USERPROFILE"))) {  // USERPROFILE for Windows
            return std::string(home) + path.substr(1);
        } else {
            return {};
        }
    }
    return path;
}

std::string findConfigPath() {
    std::string config_dir = expandTilde("~/.config");
    if (!fs::exists(config_dir) || !fs::is_directory(config_dir)) {
        config_dir = expandTilde("~");
        if (!fs::exists(config_dir) || !fs::is_directory(config_dir)) {
            config_dir = "/tmp";
            if (!fs::exists(config_dir) || !fs::is_directory(config_dir)) {
              return ".just_annotate_config";
            }
        }
    }

    return config_dir + "/.just_annotate_config";
}

ConfigState getConfig() {
    std::string config_path = findConfigPath();
    if (!fs::exists(config_path)) {
      return {};
    }

    ConfigState config;
    std::ifstream infile(config_path);
    if (infile.is_open()) {
        json j;
        infile >> j;
        infile.close();

        try {
            j.at("config").get_to(config);

            std::deque<std::string> valid_files;
            for (const auto& recent_file: config.recent_files) {
                if (fs::exists(recent_file)) {
                    valid_files.push_back(recent_file);
                }
            }
            config.recent_files = valid_files;

            return config;
        }
        catch (json::exception& e) {
            spdlog::error("JSON parse error: {}", e.what());
            return {};
        }
    }

    spdlog::warn("Failed to open file: {}", config_path);
    return {};
}

bool saveConfig(const ConfigState& config) {
    std::string config_path = findConfigPath();

    json j;
    j["config"] = config;

    std::ofstream outfile(config_path);
    if (outfile.is_open()) {
        outfile << std::fixed << std::setprecision(6) << std::setw(4) << j << std::endl;
        outfile.close();
        return true;
    }
    return false;
}
