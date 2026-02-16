#include "res_mgr_debug_panel.h"

#include <imgui.h>
#include "engine/resource/resource_manager.h"
#include "engine/resource/auto_tile_library.h"
#include <algorithm>
#include <cstdint>
#include <cfloat>
#include <cstdio>
#include <numeric>
#include <string>

namespace engine::debug {

namespace {
[[nodiscard]] double bytesToMB(std::size_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

const char* topologyToString(engine::resource::AutoTileTopology topology) {
    using engine::resource::AutoTileTopology;
    switch (topology) {
        case AutoTileTopology::CORNER:
            return "Corner";
        case AutoTileTopology::EDGE:
            return "Edge";
        case AutoTileTopology::MIXED:
            return "Mixed";
        case AutoTileTopology::UNKNOWN:
        default:
            return "Unknown";
    }
}
} // namespace

ResMgrDebugPanel::~ResMgrDebugPanel() = default;

ResMgrDebugPanel::ResMgrDebugPanel(engine::resource::ResourceManager& resource_manager)
    : resource_manager_(resource_manager) {}

std::string_view ResMgrDebugPanel::name() const {
    return "Resource";
}

void ResMgrDebugPanel::onShow() {
    refreshCaches();
}

void ResMgrDebugPanel::draw(bool& is_open) {
    if (!ImGui::Begin("Resource Manager", &is_open)) {
        ImGui::End();
        return;
    }

    refreshCaches();

    // std::accumulate 用法（<numeric>）：accumulate(begin, end, init, op)
    // 对 [begin,end) 从左到右做折叠：init = op(init, *it)。未传 op 时等价于 init + *it。
    // 此处用 op(sum, info) = sum + info.memory_bytes 或 sum + info.duration_seconds 做求和。
    // 结果：*_bytes 为各类资源内存占用，*_duration 为音效/音乐总时长（秒）。
    const std::size_t texture_bytes = std::accumulate(textures_.begin(), textures_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const std::size_t font_bytes = std::accumulate(fonts_.begin(), fonts_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const std::size_t sound_bytes = std::accumulate(sounds_.begin(), sounds_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const double sound_duration = std::accumulate(sounds_.begin(), sounds_.end(), 0.0,
        [](double sum, const auto& info) { return sum + info.duration_seconds; });
    const std::size_t music_bytes = std::accumulate(music_.begin(), music_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const double music_duration = std::accumulate(music_.begin(), music_.end(), 0.0,
        [](double sum, const auto& info) { return sum + info.duration_seconds; });

    const std::size_t audio_bytes = sound_bytes + music_bytes;
    const std::size_t total_bytes = texture_bytes + font_bytes + audio_bytes;

    ImGui::Text("Total: %.2f MB", bytesToMB(total_bytes));
    ImGui::Text("Textures: %.2f MB | Fonts: %.2f MB (atlas %.2f MB) | Audio: %.2f MB",
                bytesToMB(texture_bytes),
                bytesToMB(font_bytes),
                bytesToMB(cached_font_atlas_bytes_),
                bytesToMB(audio_bytes));
    ImGui::Text("Audio Duration: %.1fs (sounds %.1fs + music %.1fs)",
                sound_duration + music_duration,
                sound_duration,
                music_duration);
    ImGui::Separator();

    const std::string textures_header = "Textures (" + std::to_string(textures_.size()) + ")##TexturesSection";
    if (ImGui::CollapsingHeader(textures_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        drawTexturesSection();
    }

    const std::string auto_tiles_header = "Auto Tiles (" + std::to_string(auto_tiles_.size()) + ")##AutoTilesSection";
    if (ImGui::CollapsingHeader(auto_tiles_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        drawAutoTileSection();
    }

    const std::string fonts_header = "Fonts (" + std::to_string(fonts_.size()) + ")##FontsSection";
    if (ImGui::CollapsingHeader(fonts_header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        drawFontsSection();
    }

    drawAudioSection();

    ImGui::End();
}

void ResMgrDebugPanel::refreshCaches() {
    textures_ = resource_manager_.getTextureDebugInfo();
    fonts_ = resource_manager_.getFontDebugInfo();
    sounds_ = resource_manager_.getSoundDebugInfo();
    music_ = resource_manager_.getMusicDebugInfo();
    auto_tiles_ = resource_manager_.getAutoTileDebugInfo();

    cached_font_atlas_bytes_ = 0;
    for (const auto& info : fonts_) {
        for (const auto& page : info.atlas_pages) {
            cached_font_atlas_bytes_ += static_cast<std::size_t>(page.width) * static_cast<std::size_t>(page.height) * 4u;
        }
    }
}

void ResMgrDebugPanel::drawTexturesSection() {
    if (textures_.empty()) {
        ImGui::TextUnformatted("No textures loaded.");
        return;
    }

    std::size_t total_bytes = 0;
    std::int64_t total_pixels = 0;
    for (const auto& info : textures_) {
        total_bytes += info.memory_bytes;
        total_pixels += static_cast<std::int64_t>(info.width) * static_cast<std::int64_t>(info.height);
    }
    ImGui::Text("Total Memory: %.2f MB | Total Pixels: %lld",
                bytesToMB(total_bytes),
                static_cast<long long>(total_pixels));

    if (ImGui::BeginTable("TexturesTable", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, 200.0f))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("GL Handle");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Pixels");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Memory (MB)");
        ImGui::TableHeadersRow();

        for (const auto& info : textures_) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("0x%08X", static_cast<unsigned int>(info.id));

            ImGui::TableNextColumn();
            ImGui::Text("%u", info.texture);

            ImGui::TableNextColumn();
            ImGui::Text("%d x %d", info.width, info.height);

            ImGui::TableNextColumn();
            const int64_t pixels = static_cast<int64_t>(info.width) * static_cast<int64_t>(info.height);
            ImGui::Text("%lld", static_cast<long long>(pixels));

            ImGui::TableNextColumn();
            if (info.source.empty()) {
                ImGui::TextUnformatted("<unknown>");
            } else {
                ImGui::TextUnformatted(info.source.c_str());
            }

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", bytesToMB(info.memory_bytes));
        }

        ImGui::EndTable();
    }
}

void ResMgrDebugPanel::drawFontsSection() {
    if (fonts_.empty()) {
        ImGui::TextUnformatted("No fonts loaded.");
        return;
    }

    std::size_t total_bytes = 0;
    std::size_t total_glyphs = 0;
    std::size_t total_pages = 0;
    for (const auto& info : fonts_) {
        total_bytes += info.memory_bytes;
        total_glyphs += info.glyph_count;
        total_pages += info.atlas_pages.size();
    }
    ImGui::Text("Total: %zu fonts | %zu glyphs | %zu atlas pages", fonts_.size(), total_glyphs, total_pages);
    ImGui::Text("Memory: %.2f MB (atlas %.2f MB)", bytesToMB(total_bytes), bytesToMB(cached_font_atlas_bytes_));

    if (ImGui::BeginTable("FontsTable", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, 250.0f))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Size(px)");
        ImGui::TableSetupColumn("Glyphs");
        ImGui::TableSetupColumn("Atlas Pages");
        ImGui::TableSetupColumn("Metrics");
        ImGui::TableSetupColumn("Memory (MB)");
        ImGui::TableSetupColumn("Source");
        ImGui::TableHeadersRow();

        for (const auto& info : fonts_) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("0x%08X", static_cast<unsigned int>(info.id));

            ImGui::TableNextColumn();
            ImGui::Text("%d", info.pixel_size);

            ImGui::TableNextColumn();
            ImGui::Text("%zu", info.glyph_count);

            ImGui::TableNextColumn();
            ImGui::Text("%zu", info.atlas_pages.size());

            ImGui::TableNextColumn();
            ImGui::Text("Asc: %.1f  Desc: %.1f  Line: %.1f", info.ascender, info.descender, info.line_height);

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", bytesToMB(info.memory_bytes));

            ImGui::TableNextColumn();
            if (info.source.empty()) {
                ImGui::TextUnformatted("<unknown>");
            } else {
                ImGui::TextUnformatted(info.source.c_str());
            }
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Font Atlas Pages");

    for (std::size_t i = 0; i < fonts_.size(); ++i) {
        const auto& info = fonts_[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNodeEx("Font", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth,
                              "Font 0x%08X (%dpx) - %zu glyphs, %zu pages",
                              static_cast<unsigned int>(info.id),
                              info.pixel_size,
                              info.glyph_count,
                              info.atlas_pages.size())) {
            if (info.atlas_pages.empty()) {
                ImGui::TextUnformatted("No atlas pages.");
            } else if (ImGui::BeginTable("AtlasTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Handle");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Cursor");
                ImGui::TableSetupColumn("Row Height");
                ImGui::TableSetupColumn("Estimated Fill");
                ImGui::TableHeadersRow();

                for (const auto& page : info.atlas_pages) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", page.texture);

                    ImGui::TableNextColumn();
                    ImGui::Text("%d x %d", page.width, page.height);

                    ImGui::TableNextColumn();
                    ImGui::Text("%d, %d", page.cursor_x, page.cursor_y);

                    ImGui::TableNextColumn();
                    ImGui::Text("%d", page.current_row_height);

                    ImGui::TableNextColumn();
                    const int used_height = std::min(page.height, page.cursor_y + page.current_row_height);
                    const float fill_ratio = page.height > 0 ? static_cast<float>(used_height) / static_cast<float>(page.height) : 0.0f;
                    ImGui::ProgressBar(fill_ratio, ImVec2(-FLT_MIN, 0.0f));
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void ResMgrDebugPanel::drawAudioSection() {
    const std::size_t total = sounds_.size() + music_.size();
    const std::string header = "Audio (" + std::to_string(total) + ")##AudioSection";
    if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const std::size_t sound_bytes = std::accumulate(sounds_.begin(), sounds_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const double sound_duration = std::accumulate(sounds_.begin(), sounds_.end(), 0.0,
        [](double sum, const auto& info) { return sum + info.duration_seconds; });
    const std::size_t music_bytes = std::accumulate(music_.begin(), music_.end(), std::size_t{0},
        [](std::size_t sum, const auto& info) { return sum + info.memory_bytes; });
    const double music_duration = std::accumulate(music_.begin(), music_.end(), 0.0,
        [](double sum, const auto& info) { return sum + info.duration_seconds; });

    ImGui::Text("Sounds: %zu | %.2f MB | %.1fs", sounds_.size(), bytesToMB(sound_bytes), sound_duration);
    ImGui::Text("Music:  %zu | %.2f MB | %.1fs", music_.size(), bytesToMB(music_bytes), music_duration);
    ImGui::Separator();

    drawAudioTable("Sound Effects##AudioSounds", sounds_);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    drawAudioTable("Music Tracks##AudioMusic", music_);
}

void ResMgrDebugPanel::drawAudioTable(const char* label, const std::vector<engine::resource::AudioDebugInfo>& data) {
    ImGui::TextUnformatted(label);

    if (data.empty()) {
        ImGui::TextUnformatted("No entries.");
        return;
    }

    constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable(label, 8, TABLE_FLAGS, ImVec2(0.0f, 200.0f))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Channels");
        ImGui::TableSetupColumn("Sample Rate");
        ImGui::TableSetupColumn("Frames");
        ImGui::TableSetupColumn("Duration (s)");
        ImGui::TableSetupColumn("Samples");
        ImGui::TableSetupColumn("Memory (MB)");
        ImGui::TableHeadersRow();

        for (const auto& info : data) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("0x%08X", static_cast<unsigned int>(info.id));

            ImGui::TableNextColumn();
            if (info.source.empty()) {
                ImGui::TextUnformatted("<unknown>");
            } else {
                ImGui::TextUnformatted(info.source.c_str());
            }

            ImGui::TableNextColumn();
            ImGui::Text("%u", info.channels);

            ImGui::TableNextColumn();
            ImGui::Text("%u", info.sample_rate);

            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(info.frame_count));

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", info.duration_seconds);

            ImGui::TableNextColumn();
            ImGui::Text("%zu", static_cast<std::size_t>(info.sample_count));

            ImGui::TableNextColumn();
            ImGui::Text("%.2f", bytesToMB(info.memory_bytes));
        }

        ImGui::EndTable();
    }
}

void ResMgrDebugPanel::drawAutoTileSection() {
    if (auto_tiles_.empty()) {
        ImGui::TextUnformatted("No auto tile rules loaded.");
        return;
    }

    const std::size_t complete_rules = std::count_if(auto_tiles_.begin(), auto_tiles_.end(),
                                                     [](const engine::resource::AutoTileRuleDebugInfo& info) {
                                                         return info.missing_mask_count == 0;
                                                     });
    ImGui::Text("Rules: %zu | Complete: %zu | Incomplete: %zu",
                auto_tiles_.size(),
                complete_rules,
                auto_tiles_.size() - complete_rules);

    constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("AutoTileRulesTable", 7, TABLE_FLAGS, ImVec2(0.0f, 220.0f))) {
        ImGui::TableSetupColumn("Rule ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Topology");
        ImGui::TableSetupColumn("Coverage");
        ImGui::TableSetupColumn("Manual");
        ImGui::TableSetupColumn("Texture ID");
        ImGui::TableSetupColumn("Missing Masks");
        ImGui::TableHeadersRow();

        for (const auto& info : auto_tiles_) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("0x%08X", static_cast<unsigned int>(info.rule_id));

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(info.name.c_str());

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(topologyToString(info.topology));

            ImGui::TableNextColumn();
            const float ratio = static_cast<float>(info.defined_mask_count) / 256.0f;
            char overlay[32];
            std::snprintf(overlay, sizeof(overlay), "%zu/256", info.defined_mask_count);
            ImGui::ProgressBar(ratio, ImVec2(-FLT_MIN, 0.0f), overlay);
            
            ImGui::TableNextColumn();
            const float manual_ratio = static_cast<float>(info.manual_mask_count) / 256.0f;
            char manual_overlay[32];
            std::snprintf(manual_overlay, sizeof(manual_overlay), "%zu", info.manual_mask_count);
            ImGui::ProgressBar(manual_ratio, ImVec2(-FLT_MIN, 0.0f), manual_overlay);

            ImGui::TableNextColumn();
            ImGui::Text("0x%08X", static_cast<unsigned int>(info.texture_id));

            ImGui::TableNextColumn();
            if (info.missing_mask_count == 0) {
                ImGui::TextUnformatted("0");
            } else {
                const std::size_t preview_count = std::min<std::size_t>(info.missing_masks.size(), 6);
                std::string preview;
                preview.reserve(preview_count * 4);
                for (std::size_t i = 0; i < preview_count; ++i) {
                    if (i != 0) {
                        preview.append(", ");
                    }
                    preview += std::to_string(info.missing_masks[i]);
                }
                if (info.missing_mask_count > preview_count) {
                    preview.append(", ...");
                }
                ImGui::Text("%zu (%s)", info.missing_mask_count, preview.c_str());
            }
        }
        ImGui::EndTable();
    }
}

} // namespace engine::debug
