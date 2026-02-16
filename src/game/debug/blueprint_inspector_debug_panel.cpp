#include "blueprint_inspector_debug_panel.h"

#include "game/factory/blueprint_manager.h"
#include "game/defs/crop_defs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <entt/core/hashed_string.hpp>
#include <imgui.h>

namespace game::debug {

namespace {
using game::defs::cropTypeToString;
using game::defs::growthStageToString;

[[nodiscard]] bool fileExists(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(std::filesystem::path(path), ec);
    return !ec && exists;
}
} // namespace

BlueprintInspectorDebugPanel::BlueprintInspectorDebugPanel(const game::factory::BlueprintManager& blueprint_manager,
                                                           std::string resource_mapping_path)
    : blueprint_manager_(blueprint_manager),
      resource_mapping_path_(std::move(resource_mapping_path)) {
}

std::string_view BlueprintInspectorDebugPanel::name() const {
    return "Blueprint Inspector";
}

void BlueprintInspectorDebugPanel::onShow() {
    reloadResourceMapping();
    rescan();
}

void BlueprintInspectorDebugPanel::reloadResourceMapping() {
    mapped_sound_ids_.clear();
    mapping_loaded_ = false;

    std::ifstream file{std::filesystem::path(resource_mapping_path_)};
    if (!file.is_open()) {
        spdlog::warn("BlueprintInspector: resource mapping not found: {}", resource_mapping_path_);
        return;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("BlueprintInspector: resource mapping invalid json: {} ({})", resource_mapping_path_, e.what());
        return;
    }
    if (!json.is_object()) {
        spdlog::warn("BlueprintInspector: resource mapping root must be an object: {}", resource_mapping_path_);
        return;
    }

    const auto it = json.find("sound");
    if (it == json.end() || !it->is_object()) {
        spdlog::warn("BlueprintInspector: resource mapping missing 'sound' object: {}", resource_mapping_path_);
        return;
    }

    for (const auto& [key, value] : it->items()) {
        if (!value.is_string()) {
            continue;
        }
        const entt::id_type id = entt::hashed_string{key.c_str(), key.size()}.value();
        mapped_sound_ids_.insert(id);
    }

    mapping_loaded_ = true;
}

void BlueprintInspectorDebugPanel::rescan() {
    missing_texture_paths_.clear();
    missing_sound_ids_.clear();

    std::unordered_set<std::string> missing_textures;
    std::unordered_set<entt::id_type> missing_sounds;

    const auto recordTexture = [&](const std::string& texture_path) {
        if (texture_path.empty()) {
            return;
        }
        if (!fileExists(texture_path)) {
            missing_textures.insert(texture_path);
        }
    };

    const auto recordSound = [&](entt::id_type sound_id) {
        if (!mapping_loaded_ || mapped_sound_ids_.empty() || sound_id == entt::null) {
            return;
        }
        if (mapped_sound_ids_.find(sound_id) == mapped_sound_ids_.end()) {
            missing_sounds.insert(sound_id);
        }
    };

    for (const auto& [id, actor] : blueprint_manager_.actorBlueprints()) {
        (void)id;
        recordTexture(actor.sprite_.path_);
        for (const auto& [anim_id, anim] : actor.animations_) {
            (void)anim_id;
            recordTexture(anim.texture_path_);
        }
        for (const auto& [trigger_id, trigger] : actor.sounds_.triggers_) {
            (void)trigger_id;
            recordSound(trigger.sound_id_);
        }
    }

    for (const auto& [id, animal] : blueprint_manager_.animalBlueprints()) {
        (void)id;
        recordTexture(animal.sprite_.path_);
        for (const auto& [anim_id, anim] : animal.animations_) {
            (void)anim_id;
            recordTexture(anim.texture_path_);
        }
        for (const auto& [trigger_id, trigger] : animal.sounds_.triggers_) {
            (void)trigger_id;
            recordSound(trigger.sound_id_);
        }
    }

    for (const auto& [id, crop] : blueprint_manager_.cropBlueprints()) {
        (void)id;
        for (const auto& stage : crop.stages_) {
            recordTexture(stage.sprite_.path_);
        }
    }

    missing_texture_paths_.assign(missing_textures.begin(), missing_textures.end());
    std::sort(missing_texture_paths_.begin(), missing_texture_paths_.end());

    missing_sound_ids_.assign(missing_sounds.begin(), missing_sounds.end());
    std::sort(missing_sound_ids_.begin(), missing_sound_ids_.end());
}

void BlueprintInspectorDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Blueprint Inspector", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("actors: %zu", blueprint_manager_.actorBlueprintCount());
    ImGui::Text("animals: %zu", blueprint_manager_.animalBlueprintCount());
    ImGui::Text("crops: %zu", blueprint_manager_.cropBlueprintCount());

    ImGui::Separator();
    ImGui::Text("resource_mapping: %s", resource_mapping_path_.c_str());
    ImGui::Text("mapped sounds: %s (%zu)", mapping_loaded_ ? "loaded" : "missing", mapped_sound_ids_.size());

    if (ImGui::Button("Reload mapping")) {
        reloadResourceMapping();
        rescan();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        rescan();
    }

    if (ImGui::CollapsingHeader("Missing References", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("missing textures: %zu", missing_texture_paths_.size());
        if (!missing_texture_paths_.empty() && ImGui::TreeNode("Missing texture paths")) {
            for (const auto& path : missing_texture_paths_) {
                ImGui::BulletText("%s", path.c_str());
            }
            ImGui::TreePop();
        }

        ImGui::Text("missing sounds (unmapped ids): %zu", missing_sound_ids_.size());
        if (!missing_sound_ids_.empty() && ImGui::TreeNode("Missing sound ids")) {
            for (const auto id : missing_sound_ids_) {
                ImGui::BulletText("%llu", static_cast<unsigned long long>(id));
            }
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Lookup", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("actor key", actor_key_.data(), actor_key_.size());
        if (actor_key_[0] != '\0') {
            const entt::id_type id = entt::hashed_string{actor_key_.data()}.value();
            if (blueprint_manager_.hasActorBlueprint(id)) {
                const auto& blueprint = blueprint_manager_.getActorBlueprint(id);
                ImGui::Text("name: %s", blueprint.name_.c_str());
                ImGui::Text("speed: %.2f", blueprint.speed_);
                ImGui::Text("wander_radius: %.2f", blueprint.wander_radius_);
                ImGui::Text("animations: %zu", blueprint.animations_.size());
                ImGui::Text("sound_triggers: %zu", blueprint.sounds_.triggers_.size());
                ImGui::Text("sprite: %s (%s)", blueprint.sprite_.path_.c_str(), fileExists(blueprint.sprite_.path_) ? "ok" : "missing");
            } else {
                ImGui::Text("actor: not found");
            }
        }

        ImGui::Separator();
        ImGui::InputText("animal key", animal_key_.data(), animal_key_.size());
        if (animal_key_[0] != '\0') {
            const entt::id_type id = entt::hashed_string{animal_key_.data()}.value();
            if (blueprint_manager_.hasAnimalBlueprint(id)) {
                const auto& blueprint = blueprint_manager_.getAnimalBlueprint(id);
                ImGui::Text("name: %s", blueprint.name_.c_str());
                ImGui::Text("speed: %.2f", blueprint.speed_);
                ImGui::Text("wander_radius: %.2f", blueprint.wander_radius_);
                ImGui::Text("sleep_at_night: %s", blueprint.sleep_at_night_ ? "true" : "false");
                ImGui::Text("animations: %zu", blueprint.animations_.size());
                ImGui::Text("sound_triggers: %zu", blueprint.sounds_.triggers_.size());
                ImGui::Text("sprite: %s (%s)", blueprint.sprite_.path_.c_str(), fileExists(blueprint.sprite_.path_) ? "ok" : "missing");
            } else {
                ImGui::Text("animal: not found");
            }
        }

        ImGui::Separator();
        ImGui::InputText("crop key", crop_key_.data(), crop_key_.size());
        if (crop_key_[0] != '\0') {
            const entt::id_type id = entt::hashed_string{crop_key_.data()}.value();
            if (blueprint_manager_.hasCropBlueprint(id)) {
                const auto& blueprint = blueprint_manager_.getCropBlueprint(id);
                ImGui::Text("type: %s", cropTypeToString(blueprint.type_));
                ImGui::Text("stages: %zu", blueprint.stages_.size());
                ImGui::Text("harvest_item_id: %llu", static_cast<unsigned long long>(blueprint.harvest_item_id_));
                if (!blueprint.stages_.empty() && ImGui::TreeNode("Stages")) {
                    std::size_t index = 0;
                    for (const auto& stage : blueprint.stages_) {
                        ImGui::BulletText("[%zu] %s days=%d sprite=%s (%s)",
                                          index,
                                          growthStageToString(stage.stage_),
                                          stage.days_required_,
                                          stage.sprite_.path_.c_str(),
                                          fileExists(stage.sprite_.path_) ? "ok" : "missing");
                        index += 1;
                    }
                    ImGui::TreePop();
                }
            } else {
                ImGui::Text("crop: not found");
            }
        }
    }

    ImGui::End();
}

} // namespace game::debug
