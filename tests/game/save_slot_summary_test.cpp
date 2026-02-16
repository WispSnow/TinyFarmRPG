#include <gtest/gtest.h>

#include "game/save/save_slot_summary.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace game::save {
namespace {

std::filesystem::path writeTempJsonFile(std::string_view contents) {
    const auto unique = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto path = std::filesystem::temp_directory_path() / ("save_slot_summary_test_" + std::to_string(unique) + ".json");
    std::ofstream out(path);
    out << contents;
    return path;
}

TEST(SaveSlotSummaryTest, ReadsDayAndTimestampFromFile) {
    const auto path = writeTempJsonFile(R"({"schema_version":2,"timestamp":"1700000000","game_time":{"day":3}})");

    std::string error;
    const auto summary = tryReadSlotSummary(path, error);

    std::error_code ec;
    std::filesystem::remove(path, ec);

    ASSERT_TRUE(summary.has_value()) << error;
    EXPECT_EQ(summary->day, 3u);
    EXPECT_EQ(summary->timestamp, "1700000000");
}

TEST(SaveSlotSummaryTest, FailsWhenGameTimeMissing) {
    const auto path = writeTempJsonFile(R"({"schema_version":2,"timestamp":"1700000000"})");

    std::string error;
    const auto summary = tryReadSlotSummary(path, error);

    std::error_code ec;
    std::filesystem::remove(path, ec);

    EXPECT_FALSE(summary.has_value());
    EXPECT_FALSE(error.empty());
}

} // namespace
} // namespace game::save

