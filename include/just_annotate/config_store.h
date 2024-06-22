#pragma once

#include <deque>
#include <string>

struct ConfigState {
    std::deque<std::string> recent_files;
    std::deque<std::string> recent_videos;
    bool dark_mode = true;
    int window_width = 1280;
    int window_height = 720;
    int window_x = -1;
    int window_y = -1;

    bool operator==(const ConfigState& other) const;
    bool operator!=(const ConfigState& other) const;

    void addRecentFile(const std::string& path);
    void addRecentVideo(const std::string& path);
};

ConfigState getConfig();

bool saveConfig(const ConfigState& config);
