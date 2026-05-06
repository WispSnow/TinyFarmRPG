#include "battle_tester_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_app.h"
#include "engine/utils/events.h"

#include <SDL3/SDL_main.h>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <charconv>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

struct ParseResult {
    tools::battle_tester::BattleTesterConfig config{};
    bool valid{true};
    bool help_requested{false};
};

void initializeEnvironment() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void printUsage() {
    std::puts("Usage:");
    std::puts("  battle_tester [--actors actor.player,actor.lyria,actor.tori] [--troop troop.goblin_pair] [--potion-count 5]");
    std::puts("");
    std::puts("Examples:");
    std::puts("  battle_tester");
    std::puts("  battle_tester --troop troop.slime");
    std::puts("  battle_tester --actors actor.player,actor.lyria,actor.tori --troop troop.gnome_pair");
}

[[nodiscard]] std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

[[nodiscard]] std::vector<std::string> splitCommaList(std::string_view value) {
    std::vector<std::string> result{};
    while (!value.empty()) {
        const std::size_t comma_pos = value.find(',');
        const std::string_view part = trim(value.substr(0, comma_pos));
        if (!part.empty()) {
            result.emplace_back(part);
        }
        if (comma_pos == std::string_view::npos) {
            break;
        }
        value.remove_prefix(comma_pos + 1);
    }
    return result;
}

[[nodiscard]] bool parseNonNegativeInt(std::string_view value, int& out_value) {
    int parsed_value = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed_value);
    if (ec != std::errc{} || ptr != end || parsed_value < 0) {
        return false;
    }
    out_value = parsed_value;
    return true;
}

[[nodiscard]] bool nextValue(int argc, char* argv[], int& index, std::string_view option, std::string_view& out_value) {
    if (index + 1 >= argc) {
        spdlog::error("BattleTester: missing value for '{}'.", option);
        return false;
    }
    ++index;
    out_value = argv[index];
    return true;
}

[[nodiscard]] ParseResult parseArgs(int argc, char* argv[]) {
    ParseResult result{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--help" || arg == "-h") {
            result.help_requested = true;
            return result;
        }

        std::string_view value{};
        if (arg == "--actors") {
            if (!nextValue(argc, argv, i, arg, value)) {
                result.valid = false;
                return result;
            }
            auto actors = splitCommaList(value);
            if (actors.empty()) {
                spdlog::error("BattleTester: --actors must contain at least one actor id.");
                result.valid = false;
                return result;
            }
            result.config.actor_ids = std::move(actors);
            continue;
        }

        if (arg == "--troop") {
            if (!nextValue(argc, argv, i, arg, value)) {
                result.valid = false;
                return result;
            }
            value = trim(value);
            if (value.empty()) {
                spdlog::error("BattleTester: --troop must not be empty.");
                result.valid = false;
                return result;
            }
            result.config.troop_id = std::string{value};
            continue;
        }

        if (arg == "--potion-count") {
            if (!nextValue(argc, argv, i, arg, value)) {
                result.valid = false;
                return result;
            }
            if (!parseNonNegativeInt(value, result.config.potion_count)) {
                spdlog::error("BattleTester: --potion-count must be a non-negative integer.");
                result.valid = false;
                return result;
            }
            continue;
        }

        spdlog::error("BattleTester: unknown argument '{}'.", arg);
        result.valid = false;
        return result;
    }

    return result;
}

} // namespace

int main(int argc, char* argv[]) {
    initializeEnvironment();
    spdlog::set_level(spdlog::level::info);

    ParseResult parsed = parseArgs(argc, argv);
    if (parsed.help_requested) {
        printUsage();
        return 0;
    }
    if (!parsed.valid) {
        printUsage();
        return 1;
    }

    engine::core::GameApp app;
    app.registerSceneSetup([config = std::move(parsed.config)](engine::core::Context& context) mutable {
        auto scene = std::make_unique<tools::battle_tester::BattleTesterScene>(
            "BattleTester",
            context,
            std::move(config));
        context.getDispatcher().trigger<engine::utils::PushSceneEvent>(engine::utils::PushSceneEvent{std::move(scene)});
    });
    app.run();
    return 0;
}
