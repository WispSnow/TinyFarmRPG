#include "blueprint_manager.h"
#include "game/defs/crop_defs.h"
#include <string_view>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>

namespace game::factory {

namespace { // anonymous namespace

SpriteBlueprint parseSprite(const nlohmann::json& json) {
    auto path_str = json.value("path", "");
    auto path_id = entt::hashed_string(path_str.c_str());
    auto width = json.value("/src_size/width"_json_pointer, 0.0f);
    auto height = json.value("/src_size/height"_json_pointer, 0.0f);
    if (path_str.empty() || width == 0 || height == 0) {
        spdlog::error("Invalid sprite data: {}", json.dump());
        return SpriteBlueprint{};
    }
    // 可选部分：源矩形的起点默认值为 0,0，渲染目标大小默认值为 width,height
    auto src_x = json.value("/position/x"_json_pointer, 0.0f);
    auto src_y = json.value("/position/y"_json_pointer, 0.0f);
    auto size_x = json.value("/dst_size/width"_json_pointer, width);
    auto size_y = json.value("/dst_size/height"_json_pointer, height);
    auto pivot_x = json.value("/pivot/x"_json_pointer, 0.0f);
    auto pivot_y = json.value("/pivot/y"_json_pointer, 0.0f);
    // （如果指定，起点为 x,y，渲染目标大小为 size_x,size_y）
    return SpriteBlueprint{path_id,
                           path_str,
                           engine::utils::Rect{glm::vec2(src_x, src_y), glm::vec2(width, height)},
                           glm::vec2(size_x, size_y),
                           glm::vec2(pivot_x, pivot_y),
                           json.value("flip_horizontal", false)};
}

AnimationBlueprint parseOneAnimation(const nlohmann::json& json) {
    auto texture_path = json.value("path", "");
    entt::id_type texture_id = entt::hashed_string(texture_path.c_str());
    auto src_x = json.value("/position/x"_json_pointer, 0.0f);
    auto src_y = json.value("/position/y"_json_pointer, 0.0f);
    auto src_width = json.value("/src_size/width"_json_pointer, 0.0f);
    auto src_height = json.value("/src_size/height"_json_pointer, 0.0f);
    auto dst_width = json.value("/dst_size/width"_json_pointer, src_width);
    auto dst_height = json.value("/dst_size/height"_json_pointer, src_height);
    auto pivot_x = json.value("/pivot/x"_json_pointer, 0.0f);
    auto pivot_y = json.value("/pivot/y"_json_pointer, 0.0f);
    std::vector<int> frames = json.value("frames", std::vector<int>{0});
    auto duration = json.value("duration", 100.0f);
    auto row = json.value("row", 0);
    src_y += row * src_height;
    std::unordered_map<int, entt::id_type> events;
    if (json.contains("events")) {
        for (auto& [event_name, event_frame] : json["events"].items()) {
            events.emplace(event_frame.get<int>(), entt::hashed_string(event_name.c_str()));
        }
    }
    AnimationBlueprint animation{"default",
                                 texture_id,
                                 texture_path,
                                 duration,
                                 glm::vec2(src_x, src_y),
                                 glm::vec2(src_width, src_height),
                                 glm::vec2(dst_width, dst_height),
                                 glm::vec2(pivot_x, pivot_y),
                                 std::move(frames),
                                 std::move(events),
                                 json.value("flip_horizontal", false)};
    return animation;
}

std::unordered_map<entt::id_type, AnimationBlueprint> parseAnimationsMap(const nlohmann::json& json) {
    std::unordered_map<entt::id_type, AnimationBlueprint> animations; // 先准备好容器
    for (auto& [anim_name, anim_data] : json.items()) {
        // 先解析单个动画
        auto base_animation = parseOneAnimation(anim_data);
        // 如果存在“direction”字段，则需要创建多个动画, 键名ID规则是 "<动画名>_<方向>"
        if (anim_data.contains("direction") && anim_data["direction"].is_array()) {
            // 每个动画占一行，第一个动画起点不变，第二个动画开始依次下移一行。因此可以先让动画y坐标减一行
            base_animation.position_.y -= base_animation.src_size_.y;
            std::unordered_map<std::string, entt::id_type> inserted;
            std::size_t dir_index = 0;
            for (auto& direction : anim_data["direction"]) {
                std::string dir_name = direction.get<std::string>();
                auto anim_id = entt::hashed_string((anim_name + "_" + dir_name).c_str());
                auto animation = base_animation;
                animation.name_ = anim_name + "_" + dir_name;
                animation.position_.y += animation.src_size_.y * static_cast<float>(dir_index + 1); // 第一个动画y坐标还原并按行偏移
                animations.emplace(anim_id, animation);
                inserted.emplace(dir_name, anim_id);
                ++dir_index;
            }
            // 如果缺少左方向，自动使用右方向生成镜像动画
            if (!inserted.contains("left") && inserted.contains("right")) {
                auto right_id = inserted.at("right");
                if (auto it = animations.find(right_id); it != animations.end()) {
                    auto left_animation = it->second;
                    left_animation.name_ = anim_name + "_left";
                    left_animation.flip_horizontal_ = !left_animation.flip_horizontal_;
                    auto left_id = entt::hashed_string(left_animation.name_.c_str());
                    animations.emplace(left_id, std::move(left_animation));
                }
            }
            // 类似的，如果缺少右方向，自动使用左方向生成镜像动画
            if (!inserted.contains("right") && inserted.contains("left")) {
                auto left_id = inserted.at("left");
                if (auto it = animations.find(left_id); it != animations.end()) {
                    auto right_animation = it->second;
                    right_animation.name_ = anim_name + "_right";
                    right_animation.flip_horizontal_ = !right_animation.flip_horizontal_;
                    auto right_id = entt::hashed_string(right_animation.name_.c_str());
                    animations.emplace(right_id, std::move(right_animation));
                }
            }
        } else { // 如果不存在direction字段，则动画名就是键名ID，正常插入容器
            auto anim_name_id = entt::hashed_string(anim_name.c_str());
            base_animation.name_ = anim_name;
            animations.emplace(anim_name_id, std::move(base_animation));
        }
    }
    return animations;
}

SoundBlueprint parseSound(const nlohmann::json& json) {
    SoundBlueprint result;
    if (!json.is_object()) {
        return result;
    }

    for (const auto& [trigger_name, trigger_json] : json.items()) {
        if (!trigger_json.is_object()) {
            spdlog::warn("Invalid sound trigger '{}': {}", trigger_name, trigger_json.dump());
            continue;
        }

        const std::string sound_key = trigger_json.value("sound", "");
        if (sound_key.empty()) {
            spdlog::warn("Sound trigger '{}' missing 'sound' field", trigger_name);
            continue;
        }

        SoundTriggerBlueprint trigger{};
        trigger.sound_id_ = entt::hashed_string(sound_key.c_str());
        trigger.probability_ = trigger_json.value("probability", 1.0f);
        trigger.cooldown_seconds_ = trigger_json.value("cooldown", 0.0f);

        const entt::id_type trigger_id = entt::hashed_string(trigger_name.c_str());
        result.triggers_.emplace(trigger_id, trigger);
    }

    return result;
}

CropStageBlueprint parseCropStage(const nlohmann::json& json, game::defs::GrowthStage growth_stage) {
    int days_required = json.value("days_required", 0);
    auto sprite_json = json.value("sprite", nlohmann::json());
    return CropStageBlueprint{growth_stage, days_required, parseSprite(sprite_json)};
}

std::vector<CropStageBlueprint> parseCropStages(const nlohmann::json& json, const std::string& crop_type_name) {
    std::vector<CropStageBlueprint> stages;
    for (const auto& stage_obj : json) {
        std::string stage_str = stage_obj.value("stage", "");
        auto growth_stage = game::defs::growthStageFromString(stage_str);
        if (growth_stage == game::defs::GrowthStage::Unknown) {
            spdlog::warn("未知的生长阶段: {}，跳过", stage_str);
            continue;
        }

        if (!stage_obj.contains("sprite") || !stage_obj["sprite"].is_object()) {
            spdlog::warn("作物 {} 的阶段 {} 缺少 'sprite' 配置，跳过", crop_type_name, stage_str);
            continue;
        }

        stages.emplace_back(parseCropStage(stage_obj, growth_stage));
    }
    return stages;
}

struct MobBlueprintCommon {
    std::string name;
    std::string description;
    float speed{0.0f};
    SpriteBlueprint sprite;
    SoundBlueprint sounds;
    std::unordered_map<entt::id_type, AnimationBlueprint> animations;
    float wander_radius{0.0f};
    entt::id_type dialogue_id{entt::null};
    float interact_distance{80.0f};
};

MobBlueprintCommon parseMobBlueprintCommon(const nlohmann::json& json, float default_speed) {
    MobBlueprintCommon common{};
    common.name = json.value("name", "");
    common.description = json.value("description", "");
    common.speed = json.value("speed", default_speed);
    common.sprite = parseSprite(json.value("sprite", nlohmann::json()));
    common.sounds = parseSound(json.value("sounds", nlohmann::json()));
    common.animations = parseAnimationsMap(json.value("animations", nlohmann::json()));
    common.wander_radius = json.value("wander_radius", 0.0f);
    common.interact_distance = json.value("interact_distance", 80.0f);

    const std::string dialogue_str = json.value("dialogue_id", "");
    common.dialogue_id = dialogue_str.empty() ? entt::id_type{entt::null} : entt::hashed_string(dialogue_str.c_str()).value();
    return common;
}

[[nodiscard]] bool loadJsonObjectFile(std::string_view file_path, nlohmann::json& out_json, std::string_view label) {
    const auto path = std::filesystem::path(file_path);
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("无法打开{}配置文件: {}", label, file_path);
        return false;
    }

    try {
        file >> out_json;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("解析{}配置文件失败: {} ({})", label, file_path, e.what());
        return false;
    }

    if (!out_json.is_object()) {
        spdlog::error("{}配置文件格式错误: {}", label, file_path);
        return false;
    }

    return true;
}

} // anonymous namespace

BlueprintManager::BlueprintManager() = default;

BlueprintManager::~BlueprintManager() = default;

bool BlueprintManager::loadActorBlueprints(std::string_view file_path) {
    nlohmann::json json;
    if (!loadJsonObjectFile(file_path, json, "角色")) {
        return false;
    }

    actor_blueprints_.clear();
    for (auto& [actor_key, actor_obj] : json.items()) {
        const entt::id_type name_id = entt::hashed_string(actor_key.c_str());
        const auto common = parseMobBlueprintCommon(actor_obj, 100.0f);
        actor_blueprints_.emplace(name_id,
                                  ActorBlueprint{common.name,
                                                common.description,
                                                common.speed,
                                                common.sprite,
                                                common.sounds,
                                                common.animations,
                                                common.wander_radius,
                                                common.dialogue_id,
                                                common.interact_distance});
    }
    return true;
}

bool BlueprintManager::loadAnimalBlueprints(std::string_view file_path) {
    nlohmann::json json;
    if (!loadJsonObjectFile(file_path, json, "动物")) {
        return false;
    }

    animal_blueprints_.clear();
    for (auto& [animal_key, animal_obj] : json.items()) {
        const entt::id_type name_id = entt::hashed_string(animal_key.c_str());
        const auto common = parseMobBlueprintCommon(animal_obj, 80.0f);
        animal_blueprints_.emplace(name_id,
                                   AnimalBlueprint{common.name,
                                                   common.description,
                                                   common.speed,
                                                   common.sprite,
                                                   common.sounds,
                                                   common.animations,
                                                   common.wander_radius,
                                                   common.dialogue_id,
                                                   common.interact_distance,
                                                   animal_obj.value("sleep_at_night", true)});
    }

    return true;
}

bool BlueprintManager::loadCropBlueprints(std::string_view file_path) {
    nlohmann::json json;
    if (!loadJsonObjectFile(file_path, json, "作物")) {
        return false;
    }

    if (!json.contains("crops") || !json["crops"].is_array()) {
        spdlog::error("作物配置文件格式错误：缺少 'crops' 数组");
        return false;
    }

    crop_blueprints_.clear();
    for (const auto& crop_obj : json["crops"]) {
        std::string type_str = crop_obj.value("type", "");
        if (type_str.empty()) {
            spdlog::warn("跳过缺少类型的作物配置");
            continue;
        }

        auto crop_type = game::defs::cropTypeFromString(type_str);
        if (crop_type == game::defs::CropType::Unknown) {
            spdlog::warn("未知的作物类型: {}，跳过", type_str);
            continue;
        }

        if (!crop_obj.contains("stages") || !crop_obj["stages"].is_array()) {
            spdlog::warn("作物 {} 缺少 'stages' 数组，跳过", type_str);
            continue;
        }

        auto stages = parseCropStages(crop_obj["stages"], type_str);
        if (stages.empty()) {
            spdlog::warn("作物 {} 没有有效的生长阶段，跳过", type_str);
            continue;
        }

        std::string harvest_item_str = crop_obj.value("harvest_item_id", "");
        entt::id_type harvest_item_id =
            harvest_item_str.empty() ? entt::id_type{entt::null} : entt::hashed_string(harvest_item_str.c_str()).value();

        entt::id_type crop_type_id = entt::hashed_string(type_str.c_str());
        crop_blueprints_.emplace(crop_type_id, CropBlueprint{crop_type, std::move(stages), harvest_item_id});
    }

    spdlog::info("成功加载 {} 个作物蓝图", crop_blueprints_.size());
    return true;
}

} // namespace game::factory
