#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <thread>

namespace game::test {

inline void writeTextFile(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open()) << path;
    out << content;
}

inline void touchPng(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open()) << path;
}

inline std::filesystem::path createUniqueTempDir(std::string_view prefix) {
    static std::atomic<std::uint64_t> sequence{0};

    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto thread_hash = static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::random_device rng;
    const auto random_part = (static_cast<std::uint64_t>(rng()) << 32U) | static_cast<std::uint64_t>(rng());
    const auto seq = sequence.fetch_add(1, std::memory_order_relaxed);

    const std::string folder = std::string(prefix) + "_" + std::to_string(stamp) + "_" +
                               std::to_string(thread_hash) + "_" + std::to_string(random_part) + "_" +
                               std::to_string(seq);
    const auto temp_root = std::filesystem::temp_directory_path() / folder;
    std::filesystem::create_directories(temp_root);
    return temp_root;
}

} // namespace game::test
