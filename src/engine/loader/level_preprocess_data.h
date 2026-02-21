#pragma once

#include <string>
#include <vector>

namespace engine::loader {

struct LevelPreprocessTileset {
    int first_gid{0};
    std::string path{};
};

struct LevelPreprocessData {
    std::string level_path{};
    std::vector<LevelPreprocessTileset> tilesets{};
    std::vector<std::string> tileset_paths{};
    std::vector<std::string> texture_paths{};
};

struct LevelPreprocessResult {
    bool success{false};
    LevelPreprocessData data{};
    std::string error{};
};

} // namespace engine::loader
