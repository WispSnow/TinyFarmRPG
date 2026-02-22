#include "game/runtime/system_scheduler.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::filesystem::path output_path = (argc > 1)
                                                  ? std::filesystem::path{argv[1]}
                                                  : std::filesystem::path{"post_gate_parallel_island.dot"};

    const std::string dot = game::runtime::dumpPostGateParallelIslandDot();
    if (dot.empty()) {
        std::cerr << "Failed to build post-gate parallel island DOT graph.\n";
        return 1;
    }

    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    out << dot;
    out.close();

    std::cout << "DOT exported to: " << output_path << '\n';
    return 0;
}
