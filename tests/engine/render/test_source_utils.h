#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace test_source_utils {

[[nodiscard]] inline std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] inline bool containsTryCatch(std::string_view source) {
    if (source.find("try {") != std::string_view::npos) {
        return true;
    }
    if (source.find("try\n{") != std::string_view::npos) {
        return true;
    }
    if (source.find("catch (") != std::string_view::npos) {
        return true;
    }
    if (source.find("catch(") != std::string_view::npos) {
        return true;
    }
    return false;
}

[[nodiscard]] inline std::size_t countOccurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = text.find(needle);
    while (pos != std::string::npos) {
        ++count;
        pos = text.find(needle, pos + needle.size());
    }
    return count;
}

[[nodiscard]] inline std::string extractFunctionBlock(std::string_view content, std::string_view signature) {
    const std::size_t start = content.find(signature);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t open_brace = content.find('{', start);
    if (open_brace == std::string::npos) {
        return {};
    }

    int depth = 0;
    for (std::size_t i = open_brace; i < content.size(); ++i) {
        const char c = content[i];
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                return std::string(content.substr(start, i - start + 1));
            }
        }
    }
    return {};
}

} // namespace test_source_utils
