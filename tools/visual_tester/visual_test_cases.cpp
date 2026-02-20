#include "visual_test_cases.h"
#include "engine/component/auto_tile_component.h"
#include "engine/component/sprite_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/render_component.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/loader/level_loader.h"
#include "engine/render/image.h"
#include "engine/render/renderer.h"
#include "engine/render/camera.h"
#include "engine/render/text_renderer.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/system/auto_tile_system.h"
#include "engine/system/ysort_system.h"
#include "engine/audio/audio_player.h"
#include "engine/scene/scene.h"
#include "engine/utils/events.h"
#include "engine/utils/defs.h"
#include "engine/utils/math.h"
#include <entt/signal/dispatcher.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

using namespace entt::literals;

namespace tools::visual {

namespace {

[[nodiscard]] glm::vec2 screenToWorldWithRotation(const engine::render::Camera& camera, const glm::vec2& screen_pos) {
    const float zoom = camera.getZoom();
    if (zoom <= 0.0f) {
        return screen_pos;
    }

    const glm::vec2 logical = camera.getLogicalSize();
    const glm::vec2 centered = screen_pos - logical * 0.5f;
    const glm::vec2 unscaled = centered / zoom;

    const float rot = camera.getRotation();
    const float c = std::cos(rot);
    const float s = std::sin(rot);
    const glm::vec2 rotated{unscaled.x * c - unscaled.y * s, unscaled.x * s + unscaled.y * c};

    return rotated + camera.getPosition();
}

[[nodiscard]] engine::utils::Rect rectFromTileset(int tile_id, int columns, int tile_w, int tile_h) {
    const int x = (tile_id % columns) * tile_w;
    const int y = (tile_id / columns) * tile_h;
    return engine::utils::Rect{glm::vec2{static_cast<float>(x), static_cast<float>(y)},
                               glm::vec2{static_cast<float>(tile_w), static_cast<float>(tile_h)}};
}

void renderRegistrySprites(entt::registry& registry, engine::render::Renderer& renderer, const engine::render::Camera& camera) {
    (void)camera;

    registry.sort<engine::component::RenderComponent>([](const auto& lhs, const auto& rhs) { return lhs < rhs; });

    auto view = registry.view<engine::component::RenderComponent, engine::component::TransformComponent, engine::component::SpriteComponent>(
        entt::exclude<engine::component::InvisibleTag>);
    view.use<engine::component::RenderComponent>();

    engine::utils::ColorOptions color{};
    engine::utils::TransformOptions transform{};

    for (auto entity : view) {
        const auto& render_comp = view.get<engine::component::RenderComponent>(entity);
        const auto& transform_comp = view.get<engine::component::TransformComponent>(entity);
        const auto& sprite_comp = view.get<engine::component::SpriteComponent>(entity);

        const glm::vec2 size = sprite_comp.size_ * transform_comp.scale_;
        const glm::vec2 position = transform_comp.position_ - sprite_comp.pivot_ * size;

        color.use_gradient = false;
        color.start_color = render_comp.color_;
        color.end_color = render_comp.color_;

        transform.rotation_radians = transform_comp.rotation_;
        transform.pivot = sprite_comp.pivot_;
        transform.flip_horizontal = false;

        renderer.drawSprite(sprite_comp.sprite_, position, size, &color, &transform);
    }
}

} // namespace

TileAutoTileYSortVisualTest::~TileAutoTileYSortVisualTest() = default;

TileAutoTileYSortVisualTest::TileAutoTileYSortVisualTest()
    : VisualTestCase(
          "Tile / AutoTile / YSort",
          "Render",
          "验证 Tile 绘制、AutoTile(wangsets) 自动拼接更新、以及 y-sort 遮挡顺序。",
          "1) 左键点击图块：切换 tilled soil 图块的存在与否（会触发邻域 auto-tile 刷新）。\n"
          "2) 观察边缘/角落拼接是否正确（尤其是贴图边界与地图边界）。\n"
          "3) 调整 Y-Sort Demo 的 Actor Y 值，观察其与 Tree 的遮挡关系是否随 Y 改变。\n"
          "4) 相机控制同 Visual Tester 通用控制（WASD/滚轮/QE）。",
          "- AutoTile 拼接随增删图块即时刷新，边缘/角落无明显错块。\n"
          "- Atlas 边界示例（第一/最后 tile）无明显采样溢出/缝隙。\n"
          "- Actor Y 穿过 Tree Y 时，遮挡顺序发生反转（y-sort 生效）。",
          "- auto-tile 规则缺失：soil_tilled 不存在。\n"
          "- spatial index 未初始化：auto-tile 无法找到邻居。\n"
          "- 旋转相机时拾取不准：请检查 screen->world 是否考虑 rotation。\n"
          "- y-sort 不生效：RenderComponent.depth 未随 Transform.y 更新。") {}

void TileAutoTileYSortVisualTest::onEnter(engine::core::Context& context) {
    if (!saved_camera_) {
        auto& camera = context.getCamera();
        saved_camera_pos_ = camera.getPosition();
        saved_camera_zoom_ = camera.getZoom();
        saved_camera_rotation_ = camera.getRotation();
        saved_camera_ = true;
    }

    initialize(context);
}

void TileAutoTileYSortVisualTest::onExit(engine::core::Context& context) {
    auto_tile_system_.reset();
    level_loader_.reset();

    if (scene_) {
        context.getSpatialIndexManager().resetIfUsingRegistry(&scene_->getRegistry());
        scene_->getRegistry() = entt::registry{};
        scene_.reset();
    }

    if (saved_camera_) {
        auto& camera = context.getCamera();
        camera.setPosition(saved_camera_pos_);
        camera.setZoom(saved_camera_zoom_);
        camera.setRotation(saved_camera_rotation_);
        saved_camera_ = false;
    }

    initialized_ = false;
    tile_entities_.clear();
    hovered_tile_ = {-1, -1};
    hovered_world_ = {0.0f, 0.0f};
    actor_entity_ = entt::null;
    tree_entity_ = entt::null;
}

void TileAutoTileYSortVisualTest::initialize(engine::core::Context& context) {
    if (initialized_) {
        return;
    }

    scene_ = std::make_unique<engine::scene::Scene>("TileAutoTileYSortScene", context);
    level_loader_ = std::make_unique<engine::loader::LevelLoader>(*scene_);
    level_loader_->setUseAutoTile(true);
    level_loader_->setUseSpatialIndex(false);
    (void)level_loader_->preloadLevelData("assets/maps/home_exterior.tmj");

    const glm::vec2 map_pixel_size = glm::vec2(map_size_) * glm::vec2(tile_size_);
    context.getSpatialIndexManager().initialize(scene_->getRegistry(),
                                               map_size_,
                                               tile_size_,
                                               glm::vec2{0.0f, 0.0f},
                                               map_pixel_size,
                                               glm::vec2(tile_size_.x * 4.0f, tile_size_.y * 4.0f));

    auto& library = context.getAutoTileLibrary();
    constexpr entt::id_type kRuleSoilTilled = entt::hashed_string{"soil_tilled"}.value();
    constexpr std::string_view kTilledSoilTexturePath = "assets/farm-rpg/Farm/Tileset/Modular/Tilled Soil and wet soil.png";
    constexpr int kTilesetColumns = 24;
    constexpr int kDefaultTileId = 72; // tilled

    if (!library.hasRule(kRuleSoilTilled)) {
        spdlog::warn("[TileAutoTileYSort] AutoTile rule '{}' not found; preload may have failed.", "soil_tilled");
    }
    context.getResourceManager().loadTexture(entt::hashed_string{kTilledSoilTexturePath.data(), kTilledSoilTexturePath.size()},
                                             kTilledSoilTexturePath);

    auto_tile_system_ = std::make_unique<engine::system::AutoTileSystem>(
        scene_->getRegistry(),
        context.getDispatcher(),
        library,
        context.getSpatialIndexManager());

    tile_entities_.assign(static_cast<std::size_t>(map_size_.x * map_size_.y), entt::null);

    auto addTile = [&](glm::ivec2 coord, int layer, bool auto_tile) {
        if (coord.x < 0 || coord.x >= map_size_.x || coord.y < 0 || coord.y >= map_size_.y) {
            return;
        }
        const std::size_t idx = static_cast<std::size_t>(coord.y * map_size_.x + coord.x);
        if (tile_entities_[idx] != entt::null) {
            return;
        }

        auto& registry = scene_->getRegistry();
        const glm::vec2 world_pos{static_cast<float>(coord.x * tile_size_.x),
                                  static_cast<float>(coord.y * tile_size_.y)};

        const auto entity = registry.create();
        registry.emplace<engine::component::TransformComponent>(entity, world_pos, glm::vec2{1.0f, 1.0f}, 0.0f);

        auto sprite = engine::component::Sprite(
            kTilledSoilTexturePath,
            rectFromTileset(kDefaultTileId, kTilesetColumns, tile_size_.x, tile_size_.y),
            false);
        registry.emplace<engine::component::SpriteComponent>(entity, std::move(sprite), glm::vec2(tile_size_), glm::vec2{0.0f, 0.0f});

        registry.emplace<engine::component::RenderComponent>(entity, layer, 0.0f);

        if (auto_tile) {
            registry.emplace<engine::component::AutoTileComponent>(entity, kRuleSoilTilled, 0u);
            registry.emplace_or_replace<engine::component::AutoTileDirtyTag>(entity);
        }

        context.getSpatialIndexManager().addTileEntity(coord, entity);
        tile_entities_[idx] = entity;
    };

    // Cluster A (touches top-left boundary)
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool hole = (x == 3 && y == 3);
            if (!hole) {
                addTile({x, y}, 0, true);
            }
        }
    }

    // Cluster B (touches bottom-right boundary)
    const glm::ivec2 start_b{map_size_.x - 8, map_size_.y - 6};
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool hole = (x == 2 && y == 2);
            if (!hole) {
                addTile({start_b.x + x, start_b.y + y}, 0, true);
            }
        }
    }

    // Atlas edge samples (non-auto-tile, higher layer so it stays readable)
    addTile({0, map_size_.y - 1}, 1, false); // default tile (left edge)
    {
        // overwrite src rect for specific samples by creating dedicated entities
        auto& registry = scene_->getRegistry();
        auto makeSample = [&](glm::ivec2 coord, int tile_id, bool flip) {
            const glm::vec2 world_pos{static_cast<float>(coord.x * tile_size_.x), static_cast<float>(coord.y * tile_size_.y)};
            auto entity = registry.create();
            registry.emplace<engine::component::TransformComponent>(entity, world_pos, glm::vec2{1.0f, 1.0f}, 0.0f);
            auto sprite = engine::component::Sprite(
                kTilledSoilTexturePath,
                rectFromTileset(tile_id, kTilesetColumns, tile_size_.x, tile_size_.y),
                flip);
            registry.emplace<engine::component::SpriteComponent>(entity, std::move(sprite), glm::vec2(tile_size_), glm::vec2{0.0f, 0.0f});
            registry.emplace<engine::component::RenderComponent>(entity, 1, 0.0f);
            return entity;
        };

        (void)makeSample({1, map_size_.y - 1}, 0, false);                      // first tile (atlas origin)
        (void)makeSample({2, map_size_.y - 1}, kTilesetColumns - 1, false);    // row end
        (void)makeSample({3, map_size_.y - 1}, 191, true);                     // last tile + flip
    }

    // Y-sort demo entities (use circle texture with different sizes/colors)
    {
        auto& registry = scene_->getRegistry();

        constexpr std::string_view kCirclePath = "assets/textures/reserve/circle.png";
        const glm::vec2 circle_size = context.getResourceManager().getTextureSize(
            entt::hashed_string{kCirclePath.data(), kCirclePath.size()});
        const engine::utils::Rect circle_rect{glm::vec2{0.0f, 0.0f}, circle_size};

        const glm::vec2 tree_pos{128.0f, 112.0f};
        actor_base_pos_ = glm::vec2{128.0f, 112.0f};
        actor_y_ = actor_base_pos_.y;

        tree_entity_ = registry.create();
        registry.emplace<engine::component::TransformComponent>(tree_entity_, tree_pos, glm::vec2{1.0f, 1.0f}, 0.0f);
        registry.emplace<engine::component::SpriteComponent>(
            tree_entity_,
            engine::component::Sprite{kCirclePath, circle_rect},
            glm::vec2{80.0f, 120.0f},
            glm::vec2{0.5f, 1.0f});
        registry.emplace<engine::component::RenderComponent>(tree_entity_, 10, 0.0f, engine::utils::FColor{0.45f, 0.25f, 0.15f, 1.0f});

        actor_entity_ = registry.create();
        registry.emplace<engine::component::TransformComponent>(actor_entity_, actor_base_pos_, glm::vec2{1.0f, 1.0f}, 0.0f);
        registry.emplace<engine::component::SpriteComponent>(
            actor_entity_,
            engine::component::Sprite{kCirclePath, circle_rect},
            glm::vec2{48.0f, 48.0f},
            glm::vec2{0.5f, 1.0f});
        registry.emplace<engine::component::RenderComponent>(actor_entity_, 10, 0.0f, engine::utils::FColor{0.15f, 0.8f, 0.25f, 1.0f});
    }

    // Force one initial auto-tile resolve pass.
    auto& registry = scene_->getRegistry();
    auto dirty_view = registry.view<engine::component::AutoTileComponent>(entt::exclude<engine::component::AutoTileDirtyTag>);
    for (auto entity : dirty_view) {
        registry.emplace_or_replace<engine::component::AutoTileDirtyTag>(entity);
    }
    auto_tile_system_->update();

    // Focus camera roughly on center of map.
    {
        auto& camera = context.getCamera();
        camera.setPosition(glm::vec2(map_pixel_size) * 0.5f);
        camera.setZoom(2.0f);
        camera.setRotation(0.0f);
    }

    initialized_ = true;
}

void TileAutoTileYSortVisualTest::onUpdate(float /*delta_time*/, engine::core::Context& context) {
    if (!initialized_ || !scene_) {
        return;
    }

    auto& camera = context.getCamera();
    const auto& input = context.getInputManager();
    hovered_world_ = screenToWorldWithRotation(camera, input.getLogicalMousePosition());
    hovered_tile_ = {static_cast<int>(std::floor(hovered_world_.x / static_cast<float>(tile_size_.x))),
                     static_cast<int>(std::floor(hovered_world_.y / static_cast<float>(tile_size_.y)))};

    const bool hovered_in_bounds =
        hovered_tile_.x >= 0 && hovered_tile_.x < map_size_.x && hovered_tile_.y >= 0 && hovered_tile_.y < map_size_.y;

    if (show_ysort_demo_ && actor_entity_ != entt::null) {
        auto& registry = scene_->getRegistry();
        if (auto* transform = registry.try_get<engine::component::TransformComponent>(actor_entity_)) {
            transform->position_.x = actor_base_pos_.x;
            transform->position_.y = actor_y_;
        }
    }

    // Toggle tiles via mouse click, but respect ImGui capture.
    if (allow_tile_toggle_ && hovered_in_bounds && input.isActionPressed("mouse_left"_hs) && !ImGui::GetIO().WantCaptureMouse) {
        constexpr entt::id_type kRuleSoilTilled = entt::hashed_string{"soil_tilled"}.value();

        const std::size_t idx = static_cast<std::size_t>(hovered_tile_.y * map_size_.x + hovered_tile_.x);
        const bool has_tile = tile_entities_[idx] != entt::null;
        const glm::vec2 tile_world_pos{static_cast<float>(hovered_tile_.x * tile_size_.x),
                                       static_cast<float>(hovered_tile_.y * tile_size_.y)};

        if (has_tile) {
            auto& registry = scene_->getRegistry();
            const auto entity = tile_entities_[idx];
            context.getSpatialIndexManager().removeTileEntity(hovered_tile_, entity);
            registry.destroy(entity);
            tile_entities_[idx] = entt::null;

            context.getDispatcher().trigger<engine::utils::RemoveAutoTileEntityEvent>(
                engine::utils::RemoveAutoTileEntityEvent{kRuleSoilTilled, tile_world_pos});
        } else {
            auto& registry = scene_->getRegistry();
            constexpr std::string_view kTilledSoilTexturePath = "assets/farm-rpg/Farm/Tileset/Modular/Tilled Soil and wet soil.png";
            constexpr int kTilesetColumns = 24;
            constexpr int kDefaultTileId = 72;

            const auto entity = registry.create();
            registry.emplace<engine::component::TransformComponent>(entity, tile_world_pos, glm::vec2{1.0f, 1.0f}, 0.0f);
            auto sprite = engine::component::Sprite(
                kTilledSoilTexturePath,
                rectFromTileset(kDefaultTileId, kTilesetColumns, tile_size_.x, tile_size_.y),
                false);
            registry.emplace<engine::component::SpriteComponent>(entity, std::move(sprite), glm::vec2(tile_size_), glm::vec2{0.0f, 0.0f});
            registry.emplace<engine::component::RenderComponent>(entity, 0, 0.0f);
            registry.emplace<engine::component::AutoTileComponent>(entity, kRuleSoilTilled, 0u);
            registry.emplace_or_replace<engine::component::AutoTileDirtyTag>(entity);

            context.getSpatialIndexManager().addTileEntity(hovered_tile_, entity);
            tile_entities_[idx] = entity;

            context.getDispatcher().trigger<engine::utils::AddAutoTileEntityEvent>(
                engine::utils::AddAutoTileEntityEvent{kRuleSoilTilled, tile_world_pos});
        }
    }

    // Process auto-tile dirty updates.
    if (auto_tile_system_) {
        auto_tile_system_->update();
    }

    // Update render.depth_ from transform.y (ysort).
    engine::system::YSortSystem ysort{};
    ysort.render(scene_->getRegistry(), 1.0f);
}

void TileAutoTileYSortVisualTest::onRender(engine::core::Context& context) {
    if (!initialized_ || !scene_) {
        return;
    }

    auto& renderer = context.getRenderer();
    auto& camera = context.getCamera();
    renderRegistrySprites(scene_->getRegistry(), renderer, camera);

    // Grid overlay / hover highlight
    if (show_grid_) {
        auto grid_color = renderer.getDefaultWorldColorOptions();
        grid_color.use_gradient = false;
        grid_color.start_color = engine::utils::FColor{0.1f, 0.1f, 0.12f, 0.25f};
        grid_color.end_color = grid_color.start_color;

        const engine::utils::Rect border_rect{glm::vec2{0.0f, 0.0f}, glm::vec2(map_size_) * glm::vec2(tile_size_)};
        renderer.drawRect(border_rect.pos, border_rect.size, 2.0f, &grid_color);
    }

    if (show_hover_highlight_) {
        const bool hovered_in_bounds =
            hovered_tile_.x >= 0 && hovered_tile_.x < map_size_.x && hovered_tile_.y >= 0 && hovered_tile_.y < map_size_.y;
        if (hovered_in_bounds) {
            auto hl = renderer.getDefaultWorldColorOptions();
            hl.use_gradient = false;
            hl.start_color = engine::utils::FColor::yellow();
            hl.end_color = hl.start_color;
            const glm::vec2 pos{static_cast<float>(hovered_tile_.x * tile_size_.x), static_cast<float>(hovered_tile_.y * tile_size_.y)};
            renderer.drawRect(pos, glm::vec2(tile_size_), 2.0f, &hl);
        }
    }
}

void TileAutoTileYSortVisualTest::onImGui(engine::core::Context& context) {
    ImGui::Separator();

    if (ImGui::Button("Reset Defaults (Rebuild)")) {
        allow_tile_toggle_ = true;
        show_grid_ = true;
        show_hover_highlight_ = true;
        show_ysort_demo_ = true;
        hovered_tile_ = {-1, -1};
        hovered_world_ = {0.0f, 0.0f};
        actor_y_ = actor_base_pos_.y;

        auto_tile_system_.reset();
        level_loader_.reset();
        if (scene_) {
            context.getSpatialIndexManager().resetIfUsingRegistry(&scene_->getRegistry());
            scene_->getRegistry() = entt::registry{};
            scene_.reset();
        }
        initialized_ = false;
        tile_entities_.clear();
        actor_entity_ = entt::null;
        tree_entity_ = entt::null;

        initialize(context);
    }

    ImGui::TextUnformatted("Tile/AutoTile controls");
    ImGui::Checkbox("Allow tile toggle by left click", &allow_tile_toggle_);
    ImGui::Checkbox("Show grid border", &show_grid_);
    ImGui::Checkbox("Show hover highlight", &show_hover_highlight_);
    ImGui::Text("Hovered world: (%.1f, %.1f)", hovered_world_.x, hovered_world_.y);
    ImGui::Text("Hovered tile: (%d, %d)", hovered_tile_.x, hovered_tile_.y);

    if (ImGui::CollapsingHeader("Y-Sort Demo", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Y-Sort demo entities", &show_ysort_demo_);
        ImGui::SliderFloat("Actor Y", &actor_y_, 0.0f, static_cast<float>(map_size_.y * tile_size_.y));
        ImGui::TextUnformatted("Expected: Actor should go in front when ActorY > TreeY.");
    }
}

RenderPassCoverageVisualTest::RenderPassCoverageVisualTest()
    : VisualTestCase(
          "Render Pass Coverage",
          "Render",
          "以单一画面覆盖 scene/lighting/emissive/bloom/ui 五个通道的组合与叠加顺序。",
          "1) 观察：背景 sprite/primitive、光照范围、自发光与 bloom、UI 叠加。\n"
          "2) 在下方参数区切换各 pass 相关开关，确认只影响对应通道。",
          "- scene：sprite/primitive 正常。\n"
          "- lighting：光照叠加正常且不会污染 UI。\n"
          "- emissive+bloom：自发光产生可见 bloom（开启时）。\n"
          "- ui：UI 永远最上层且不被 lighting/bloom 影响。",
          "- bloom 关闭：自发光仍存在但无泛光。\n"
          "- UI 被光照/泛光影响：pass 叠加顺序或采样输入错误。") {}

void RenderPassCoverageVisualTest::applyDefaults(engine::core::Context& context) {
    auto& gl = context.getGLRenderer();
    gl.setPointLightsEnabled(true);
    gl.setSpotLightsEnabled(true);
    gl.setDirectionalLightsEnabled(true);
    gl.setEmissiveEnabled(true);
    gl.setBloomEnabled(true);
    gl.setPixelSnapEnabled(true);
    gl.setAmbient(glm::vec3{0.15f, 0.15f, 0.18f});
    gl.setBloomStrength(1.1f);
    gl.setBloomSigma(3.5f);
}

void RenderPassCoverageVisualTest::onEnter(engine::core::Context& context) {
    if (!saved_) {
        auto& gl = context.getGLRenderer();
        saved_point_lights_enabled_ = gl.isPointLightsEnabled();
        saved_spot_lights_enabled_ = gl.isSpotLightsEnabled();
        saved_directional_lights_enabled_ = gl.isDirectionalLightsEnabled();
        saved_bloom_enabled_ = gl.isBloomEnabled();
        saved_emissive_enabled_ = gl.isEmissiveEnabled();
        saved_pixel_snap_enabled_ = gl.isPixelSnapEnabled();
        saved_ambient_ = gl.getAmbient();
        saved_bloom_strength_ = gl.getBloomStrength();
        saved_bloom_sigma_ = gl.getBloomSigma();
        saved_ = true;
    }
    applyDefaults(context);
}

void RenderPassCoverageVisualTest::onExit(engine::core::Context& context) {
    if (!saved_) {
        return;
    }
    auto& gl = context.getGLRenderer();
    gl.setPointLightsEnabled(saved_point_lights_enabled_);
    gl.setSpotLightsEnabled(saved_spot_lights_enabled_);
    gl.setDirectionalLightsEnabled(saved_directional_lights_enabled_);
    gl.setBloomEnabled(saved_bloom_enabled_);
    gl.setEmissiveEnabled(saved_emissive_enabled_);
    gl.setPixelSnapEnabled(saved_pixel_snap_enabled_);
    gl.setAmbient(saved_ambient_);
    gl.setBloomStrength(saved_bloom_strength_);
    gl.setBloomSigma(saved_bloom_sigma_);
    saved_ = false;
}

void RenderPassCoverageVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();

    // Scene pass (world): primitive + sprite
    {
        auto bg = renderer.getDefaultWorldColorOptions();
        bg.use_gradient = true;
        bg.start_color = engine::utils::FColor{0.08f, 0.09f, 0.12f, 1.0f};
        bg.end_color = engine::utils::FColor{0.03f, 0.03f, 0.05f, 1.0f};
        bg.angle_radians = glm::radians(65.0f);
        renderer.drawFilledRect(engine::utils::Rect{-520.0f, -300.0f, 1040.0f, 600.0f}, &bg);

        engine::component::Sprite sprite{"assets/textures/Elements/Plants/spr_deco_mushroom_blue_01_strip4.png",
                                         engine::utils::Rect{0, 0, 64, 16}};
        renderer.drawSprite(sprite, glm::vec2{-220.0f, -40.0f}, glm::vec2{256.0f, 64.0f});
    }

    // Lighting pass
    renderer.addPointLight(glm::vec2{-80.0f, 40.0f}, 240.0f);
    {
        engine::utils::SpotLightOptions spot{};
        spot.color = engine::utils::FColor::yellow();
        renderer.addSpotLight(glm::vec2{-260.0f, 200.0f}, 420.0f, glm::vec2{1.0f, -0.4f}, &spot);
    }
    renderer.addDirectionalLight(glm::vec2{0.25f, -1.0f});

    // Emissive + Bloom
    {
        engine::utils::EmissiveParams params{};
        params.intensity = emissive_intensity_;
        params.color.start_color = engine::utils::FColor{0.9f, 0.2f, 0.9f, 1.0f};
        params.color.end_color = params.color.start_color;
        renderer.addEmissiveRect(engine::utils::Rect{60.0f, -20.0f, 120.0f, 80.0f}, &params);
    }
    {
        engine::utils::EmissiveParams params{};
        params.intensity = emissive_intensity_;
        params.color.start_color = engine::utils::FColor{0.2f, 0.8f, 1.0f, 1.0f};
        params.color.end_color = params.color.start_color;
        renderer.addEmissiveSprite(engine::component::Sprite{"assets/textures/reserve/circle.png",
                                                             engine::utils::Rect{0, 0, 960, 960}},
                                   glm::vec2{260.0f, 80.0f}, glm::vec2{96.0f, 96.0f}, &params);
    }

    // UI pass (should not be affected by lighting/bloom)
    {
        auto ui_color = renderer.getDefaultUIColorOptions();
        ui_color.use_gradient = false;
        ui_color.start_color = engine::utils::FColor{0.08f, 0.08f, 0.1f, 0.85f};
        ui_color.end_color = ui_color.start_color;
        renderer.drawUIFilledRect(engine::utils::Rect{-360.0f, -260.0f, 260.0f, 110.0f}, &ui_color);

        renderer.drawUIImage(engine::render::Image{"assets/textures/UI/bluebar_00.png", engine::utils::Rect{0, 0, 15, 7}},
                             glm::vec2{-350.0f, -250.0f}, glm::vec2{240.0f, 90.0f});
    }
}

void RenderPassCoverageVisualTest::onImGui(engine::core::Context& context) {
    auto& gl = context.getGLRenderer();

    ImGui::Separator();
    ImGui::TextUnformatted("Pass toggles");

    bool point = gl.isPointLightsEnabled();
    bool spot = gl.isSpotLightsEnabled();
    bool dir = gl.isDirectionalLightsEnabled();
    bool emissive = gl.isEmissiveEnabled();
    bool bloom = gl.isBloomEnabled();
    bool snap = gl.isPixelSnapEnabled();

    if (ImGui::Checkbox("Point Lights", &point)) gl.setPointLightsEnabled(point);
    if (ImGui::Checkbox("Spot Lights", &spot)) gl.setSpotLightsEnabled(spot);
    if (ImGui::Checkbox("Directional Lights", &dir)) gl.setDirectionalLightsEnabled(dir);
    if (ImGui::Checkbox("Emissive", &emissive)) gl.setEmissiveEnabled(emissive);
    if (ImGui::Checkbox("Bloom", &bloom)) gl.setBloomEnabled(bloom);
    if (ImGui::Checkbox("Pixel Snap", &snap)) gl.setPixelSnapEnabled(snap);

    glm::vec3 ambient = gl.getAmbient();
    if (ImGui::SliderFloat3("Ambient", &ambient.x, 0.0f, 1.0f)) {
        gl.setAmbient(ambient);
    }

    float bloom_strength = gl.getBloomStrength();
    if (ImGui::SliderFloat("Bloom Strength", &bloom_strength, 0.0f, 3.0f)) {
        gl.setBloomStrength(bloom_strength);
    }
    float bloom_sigma = gl.getBloomSigma();
    if (ImGui::SliderFloat("Bloom Sigma", &bloom_sigma, 0.1f, 10.0f)) {
        gl.setBloomSigma(bloom_sigma);
    }

    ImGui::SliderFloat("Emissive Intensity (this test)", &emissive_intensity_, 0.0f, 3.0f);

    if (ImGui::Button("Reset Defaults")) {
        applyDefaults(context);
        emissive_intensity_ = 0.8f;
    }

    if (ImGui::CollapsingHeader("Pass Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto show = [&](engine::render::opengl::GLRenderer::PassType pass, const char* name) {
            const auto& stats = gl.getPassStats(pass);
            ImGui::BulletText("%s: draw=%u sprites=%u vtx=%u idx=%u",
                              name, stats.draw_calls, stats.sprite_count, stats.vertex_count, stats.index_count);
        };
        show(engine::render::opengl::GLRenderer::PassType::Scene, "Scene");
        show(engine::render::opengl::GLRenderer::PassType::Lighting, "Lighting");
        show(engine::render::opengl::GLRenderer::PassType::Emissive, "Emissive");
        show(engine::render::opengl::GLRenderer::PassType::Bloom, "Bloom");
        show(engine::render::opengl::GLRenderer::PassType::UI, "UI");
    }
}

CameraViewportClippingVisualTest::CameraViewportClippingVisualTest()
    : VisualTestCase(
          "Camera + Viewport Clipping",
          "Render",
          "验证相机移动/缩放/旋转下的视口边界，以及 viewport clipping(culling) 开关行为。",
          "1) 观察黄色 View Outline（实际视口）与青色 Cull Rect（AABB 裁剪矩形）。\n"
          "2) 移动/缩放/旋转相机，确认边界与画面一致。\n"
          "3) 切换 Viewport Clipping Enabled，观察 Scene pass 的 sprite 计数变化。\n"
          "4) 当 rotation 超过 ~0.1 rad 时，Current ViewRect 为空（culling 自动失效，属预期）。",
          "- View Outline 与屏幕边缘一致，随 zoom/rotation 正确变化。\n"
          "- Cull Rect 在 rotation≈0 时与 View Outline 近似一致。\n"
          "- 打开/关闭 viewport clipping 不应导致可见元素误消失。\n"
          "- Scene pass sprite_count 会随 clipping/rotation 状态显著变化。",
          "- rotation>阈值时 view rect 为空：属预期（当前实现只在接近 0 时提供 AABB view rect）。\n"
          "- 若可见元素被误裁剪：检查 computeCameraViewRect / shouldCullRect 逻辑。") {
    rebuildMarkers();
}

void CameraViewportClippingVisualTest::rebuildMarkers() {
    marker_positions_.clear();
    const int extent = std::max(0, marker_half_extent_);
    marker_positions_.reserve(static_cast<std::size_t>((extent * 2 + 1) * (extent * 2 + 1)));

    for (int y = -extent; y <= extent; ++y) {
        for (int x = -extent; x <= extent; ++x) {
            marker_positions_.push_back(glm::vec2{static_cast<float>(x * marker_step_),
                                                 static_cast<float>(y * marker_step_)});
        }
    }
}

void CameraViewportClippingVisualTest::applyDefaults(engine::core::Context& context) {
    show_view_outline_ = true;
    show_culling_rect_ = true;
    show_axes_ = true;
    show_marker_grid_ = true;
    show_stats_ = true;

    marker_half_extent_ = 20;
    marker_step_ = 64;
    marker_size_ = glm::vec2{10.0f, 10.0f};
    rebuildMarkers();

    auto& camera = context.getCamera();
    camera.setPosition({0.0f, 0.0f});
    camera.setZoom(1.0f);
    camera.setRotation(0.0f);

    context.getRenderer().setViewportClippingEnabled(true);
}

void CameraViewportClippingVisualTest::onEnter(engine::core::Context& context) {
    if (!saved_) {
        auto& renderer = context.getRenderer();
        saved_viewport_clipping_enabled_ = renderer.isViewportClippingEnabled();

        auto& camera = context.getCamera();
        saved_camera_pos_ = camera.getPosition();
        saved_camera_zoom_ = camera.getZoom();
        saved_camera_rotation_ = camera.getRotation();
        saved_ = true;
    }

    applyDefaults(context);
}

void CameraViewportClippingVisualTest::onExit(engine::core::Context& context) {
    if (!saved_) {
        return;
    }

    auto& renderer = context.getRenderer();
    renderer.setViewportClippingEnabled(saved_viewport_clipping_enabled_);

    auto& camera = context.getCamera();
    camera.setPosition(saved_camera_pos_);
    camera.setZoom(saved_camera_zoom_);
    camera.setRotation(saved_camera_rotation_);

    saved_ = false;
}

void CameraViewportClippingVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();
    const auto& camera = context.getCamera();

    {
        auto bg = renderer.getDefaultWorldColorOptions();
        bg.use_gradient = false;
        bg.start_color = engine::utils::FColor{0.04f, 0.04f, 0.05f, 1.0f};
        bg.end_color = bg.start_color;
        renderer.drawFilledRect(engine::utils::Rect{-4096.0f, -4096.0f, 8192.0f, 8192.0f}, &bg);
    }

    const float world_extent = static_cast<float>(std::max(1, marker_half_extent_) * std::max(1, marker_step_));

    if (show_axes_) {
        auto axis = renderer.getDefaultWorldColorOptions();
        axis.use_gradient = false;
        axis.start_color = engine::utils::FColor{0.25f, 0.25f, 0.28f, 0.9f};
        axis.end_color = axis.start_color;
        renderer.drawLine(glm::vec2{-world_extent, 0.0f}, glm::vec2{world_extent, 0.0f}, 2.0f, &axis);
        renderer.drawLine(glm::vec2{0.0f, -world_extent}, glm::vec2{0.0f, world_extent}, 2.0f, &axis);
    }

    if (show_marker_grid_) {
        auto marker = renderer.getDefaultWorldColorOptions();
        marker.use_gradient = false;
        marker.start_color = engine::utils::FColor{0.12f, 0.65f, 0.9f, 0.35f};
        marker.end_color = marker.start_color;

        const glm::vec2 half = marker_size_ * 0.5f;
        for (const auto& p : marker_positions_) {
            renderer.drawFilledRect(engine::utils::Rect{p - half, marker_size_}, &marker);
        }
    }

    const float zoom = camera.getZoom();
    if (zoom > 0.0f) {
        const glm::vec2 half_view = (camera.getLogicalSize() / zoom) * 0.5f;
        const glm::vec2 center = camera.getPosition();

        if (show_view_outline_) {
            const float rot = camera.getRotation();
            const float c = std::cos(rot);
            const float s = std::sin(rot);
            auto rot2 = [&](const glm::vec2& v) {
                return glm::vec2{v.x * c - v.y * s, v.x * s + v.y * c};
            };

            const glm::vec2 tl = center + rot2(glm::vec2{-half_view.x, -half_view.y});
            const glm::vec2 tr = center + rot2(glm::vec2{half_view.x, -half_view.y});
            const glm::vec2 br = center + rot2(glm::vec2{half_view.x, half_view.y});
            const glm::vec2 bl = center + rot2(glm::vec2{-half_view.x, half_view.y});

            auto outline = renderer.getDefaultWorldColorOptions();
            outline.use_gradient = false;
            outline.start_color = engine::utils::FColor::yellow();
            outline.end_color = outline.start_color;

            renderer.drawLine(tl, tr, 2.0f, &outline);
            renderer.drawLine(tr, br, 2.0f, &outline);
            renderer.drawLine(br, bl, 2.0f, &outline);
            renderer.drawLine(bl, tl, 2.0f, &outline);
        }

        if (show_culling_rect_) {
            const glm::vec2 top_left = center - half_view;
            const engine::utils::Rect approx{top_left, half_view * 2.0f};

            auto cull = renderer.getDefaultWorldColorOptions();
            cull.use_gradient = false;
            cull.start_color = engine::utils::FColor{0.25f, 0.95f, 0.95f, 0.85f};
            cull.end_color = cull.start_color;
            renderer.drawRect(approx.pos, approx.size, 2.0f, &cull);
        }
    }
}

void CameraViewportClippingVisualTest::onImGui(engine::core::Context& context) {
    auto& renderer = context.getRenderer();
    auto& camera = context.getCamera();

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        applyDefaults(context);
    }

    bool clipping = renderer.isViewportClippingEnabled();
    if (ImGui::Checkbox("Viewport Clipping Enabled (culling)", &clipping)) {
        renderer.setViewportClippingEnabled(clipping);
    }

    ImGui::Checkbox("Show View Outline (yellow)", &show_view_outline_);
    ImGui::Checkbox("Show Cull Rect AABB (cyan)", &show_culling_rect_);
    ImGui::Checkbox("Show Axes", &show_axes_);
    ImGui::Checkbox("Show Marker Grid", &show_marker_grid_);

    int extent = marker_half_extent_;
    if (ImGui::SliderInt("Marker Half Extent", &extent, 4, 64)) {
        marker_half_extent_ = extent;
        rebuildMarkers();
    }
    int step = marker_step_;
    if (ImGui::SliderInt("Marker Step", &step, 16, 256)) {
        marker_step_ = step;
        rebuildMarkers();
    }
    ImGui::DragFloat2("Marker Size", &marker_size_.x, 1.0f, 1.0f, 64.0f, "%.1f");

    ImGui::Text("Camera: pos(%.1f, %.1f) zoom=%.2f rot=%.3f rad",
                camera.getPosition().x, camera.getPosition().y,
                camera.getZoom(), camera.getRotation());

    const auto view = renderer.getCurrentViewRect();
    if (view.has_value()) {
        ImGui::Text("Current ViewRect: (%.1f, %.1f, %.1f, %.1f)",
                    view->pos.x, view->pos.y, view->size.x, view->size.y);
    } else {
        ImGui::TextUnformatted("Current ViewRect: <none> (rotation>~0.1rad or clipping disabled)");
    }

    if (show_stats_ && ImGui::CollapsingHeader("Pass Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& gl = context.getGLRenderer();
        const auto& stats = gl.getPassStats(engine::render::opengl::GLRenderer::PassType::Scene);
        ImGui::BulletText("Scene: draw=%u sprites=%u vtx=%u idx=%u",
                          stats.draw_calls, stats.sprite_count, stats.vertex_count, stats.index_count);
    }
}

SpriteAndPrimitiveVisualTest::SpriteAndPrimitiveVisualTest()
    : VisualTestCase(
          "Sprites & Primitives",
          "Render",
          "验证基础 sprite / primitive 绘制链路是否正常。",
          "1) 观察绿色圆、渐变矩形、蘑菇 sprite。\n"
          "2) 使用 WASD/方向键移动相机；鼠标滚轮缩放。",
          "- primitive 填充颜色/渐变正确。\n"
          "- sprite UV/Rect 正确，无异常拉伸/闪烁。",
          "- 纹理路径缺失：sprite 不显示。\n"
          "- UV/Rect 错误：纹理错位或拉伸。\n"
          "- 混合/状态错误：出现叠加异常。") {
    applyDefaults();
}

void SpriteAndPrimitiveVisualTest::applyDefaults() {
    show_circle_ = true;
    show_gradient_rect_ = true;
    show_sprite_ = true;
    circle_radius_ = 100.0f;
    rect_pos_ = glm::vec2{300.0f, 300.0f};
    rect_size_ = glm::vec2{300.0f, 300.0f};
    rect_gradient_angle_deg_ = 65.0f;
    sprite_pos_ = glm::vec2{-100.0f, -100.0f};
    sprite_size_ = glm::vec2{128.0f, 32.0f};
}

void SpriteAndPrimitiveVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();
    engine::component::Sprite sprite{"assets/textures/Elements/Plants/spr_deco_mushroom_blue_01_strip4.png", engine::utils::Rect{0, 0, 64, 16}};

    if (show_circle_) {
        auto circle_color = renderer.getDefaultWorldColorOptions();
        circle_color.start_color = engine::utils::FColor::green();
        circle_color.end_color = engine::utils::FColor::green();
        circle_color.use_gradient = false;
        renderer.drawFilledCircle(glm::vec2{0.0f, 0.0f}, std::max(circle_radius_, 1.0f), &circle_color);
    }

    if (show_gradient_rect_) {
        auto rect_color = renderer.getDefaultWorldColorOptions();
        rect_color.use_gradient = true;
        rect_color.start_color = engine::utils::FColor::yellow();
        rect_color.end_color = engine::utils::FColor::red();
        rect_color.angle_radians = glm::radians(rect_gradient_angle_deg_);
        renderer.drawFilledRect(engine::utils::Rect{rect_pos_, glm::max(rect_size_, glm::vec2{1.0f, 1.0f})}, &rect_color);
    }

    if (show_sprite_) {
        renderer.drawSprite(sprite, sprite_pos_, glm::max(sprite_size_, glm::vec2{1.0f, 1.0f}));
    }
}

void SpriteAndPrimitiveVisualTest::onImGui(engine::core::Context& context) {
    (void)context;

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        applyDefaults();
    }

    ImGui::Checkbox("Show Circle", &show_circle_);
    ImGui::Checkbox("Show Gradient Rect", &show_gradient_rect_);
    ImGui::Checkbox("Show Sprite", &show_sprite_);
    ImGui::SliderFloat("Circle Radius", &circle_radius_, 4.0f, 400.0f, "%.1f");
    ImGui::DragFloat2("Rect Pos", &rect_pos_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
    ImGui::DragFloat2("Rect Size", &rect_size_.x, 1.0f, 1.0f, 5000.0f, "%.1f");
    ImGui::SliderFloat("Rect Gradient Angle (deg)", &rect_gradient_angle_deg_, 0.0f, 360.0f, "%.1f");
    ImGui::DragFloat2("Sprite Pos", &sprite_pos_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
    ImGui::DragFloat2("Sprite Size", &sprite_size_.x, 1.0f, 1.0f, 5000.0f, "%.1f");
}

LightingVisualTest::LightingVisualTest()
    : VisualTestCase(
          "Lighting",
          "Lighting",
          "验证点光/聚光/方向光的叠加与范围。",
          "1) 观察点光、聚光、方向光叠加效果。\n"
          "2) 移动相机，确认光照随世界位置/方向正确变化。",
          "- 光照范围与方向可感知。\n"
          "- 叠加不会导致全黑/全白等明显异常。",
          "- 光照 pass 未生效：画面与无光照一致。\n"
          "- 参数/方向错误：范围不对或方向反。\n"
          "- shader 编译失败：画面全黑。") {
    applyDefaults();
}

void LightingVisualTest::applyDefaults() {
    enable_point_ = true;
    enable_spot_ = true;
    enable_directional_ = true;

    point_pos_ = glm::vec2{300.0f, 300.0f};
    point_radius_ = 200.0f;
    point_intensity_ = 1.0f;

    spot_pos_ = glm::vec2{200.0f, 200.0f};
    spot_dir_ = glm::vec2{0.0f, -1.0f};
    spot_radius_ = 300.0f;
    spot_intensity_ = 1.0f;

    dir_light_dir_ = glm::vec2{0.0f, -1.0f};
    dir_light_intensity_ = 1.0f;
}

void LightingVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();

    // Base surface so light effects are easier to see.
    {
        auto base = renderer.getDefaultWorldColorOptions();
        base.use_gradient = false;
        base.start_color = engine::utils::FColor{0.18f, 0.18f, 0.2f, 1.0f};
        base.end_color = base.start_color;
        renderer.drawFilledRect(engine::utils::Rect{-520.0f, -320.0f, 1040.0f, 640.0f}, &base);
    }

    if (enable_point_) {
        engine::utils::PointLightOptions opts{};
        opts.intensity = point_intensity_;
        renderer.addPointLight(point_pos_, point_radius_, &opts);
    }

    if (enable_spot_) {
        engine::utils::SpotLightOptions opts{};
        opts.color = engine::utils::FColor::yellow();
        opts.intensity = spot_intensity_;

        glm::vec2 dir = spot_dir_;
        const float len = glm::length(dir);
        if (len > 1e-3f) {
            dir /= len;
        } else {
            dir = glm::vec2{0.0f, -1.0f};
        }
        renderer.addSpotLight(spot_pos_, spot_radius_, dir, &opts);
    }

    if (enable_directional_) {
        engine::utils::DirectionalLightOptions opts{};
        opts.intensity = dir_light_intensity_;

        glm::vec2 dir = dir_light_dir_;
        const float len = glm::length(dir);
        if (len > 1e-3f) {
            dir /= len;
        } else {
            dir = glm::vec2{0.0f, -1.0f};
        }
        renderer.addDirectionalLight(dir, &opts);
    }
}

void LightingVisualTest::onImGui(engine::core::Context& context) {
    (void)context;

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        applyDefaults();
    }

    ImGui::Checkbox("Enable Point Light", &enable_point_);
    ImGui::Checkbox("Enable Spot Light", &enable_spot_);
    ImGui::Checkbox("Enable Directional Light", &enable_directional_);

    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Pos", &point_pos_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
        ImGui::SliderFloat("Radius", &point_radius_, 10.0f, 1200.0f, "%.1f");
        ImGui::SliderFloat("Intensity", &point_intensity_, 0.0f, 3.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Pos##spot", &spot_pos_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
        ImGui::DragFloat2("Dir##spot", &spot_dir_.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Radius##spot", &spot_radius_, 10.0f, 1600.0f, "%.1f");
        ImGui::SliderFloat("Intensity##spot", &spot_intensity_, 0.0f, 3.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Dir##dir", &dir_light_dir_.x, 0.01f, -1.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Intensity##dir", &dir_light_intensity_, 0.0f, 3.0f, "%.2f");
    }
}

EmissiveVisualTest::EmissiveVisualTest()
    : VisualTestCase(
          "Emissive",
          "Lighting",
          "验证自发光绘制与（若启用）bloom 的组合效果。",
          "1) 观察自发光矩形与自发光 sprite 是否明显更亮。\n"
          "2) 若启用 bloom，观察泛光范围/强度是否符合预期。",
          "- emissive 对象明显更亮。\n"
          "- 不会把整屏照亮或完全无效果。",
          "- emissive pass 未合成：看不到发光。\n"
          "- 强度/阈值不合理：过曝或完全无效果。") {
    applyDefaults();
}

void EmissiveVisualTest::applyDefaults() {
    show_rect_ = true;
    show_sprite_ = true;
    show_cursor_ = true;
    rect_intensity_ = 0.3f;
    sprite_intensity_ = 1.0f;
    cursor_intensity_ = 1.0f;
}

void EmissiveVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();

    if (show_rect_) {
        engine::utils::EmissiveParams rect_emissive{};
        rect_emissive.intensity = rect_intensity_;
        rect_emissive.color.start_color = engine::utils::FColor::yellow();
        rect_emissive.color.end_color = rect_emissive.color.start_color;
        renderer.addEmissiveRect(engine::utils::Rect{0.0f, 0.0f, 100.0f, 100.0f}, &rect_emissive);
    }

    if (show_sprite_) {
        engine::utils::EmissiveParams sprite_emissive{};
        sprite_emissive.intensity = sprite_intensity_;
        sprite_emissive.color.start_color = engine::utils::FColor::purple();
        sprite_emissive.color.end_color = sprite_emissive.color.start_color;
        renderer.addEmissiveSprite(engine::component::Sprite{"assets/textures/reserve/circle.png", engine::utils::Rect{0, 0, 960, 960}},
                                   glm::vec2{100.0f, 100.0f}, glm::vec2{100.0f, 100.0f}, &sprite_emissive);
    }

    if (show_cursor_) {
        engine::utils::EmissiveParams cursor_emissive{};
        cursor_emissive.intensity = cursor_intensity_;
        cursor_emissive.color.start_color = engine::utils::FColor::white();
        cursor_emissive.color.end_color = cursor_emissive.color.start_color;
        renderer.addEmissiveSprite(engine::component::Sprite{"assets/textures/UI/cursor_01.png", engine::utils::Rect{0, 0, 11, 19}},
                                   glm::vec2{100.0f, -100.0f}, glm::vec2{55.0f, 95.0f}, &cursor_emissive);
    }
}

void EmissiveVisualTest::onImGui(engine::core::Context& context) {
    (void)context;

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        applyDefaults();
    }

    ImGui::Checkbox("Show Rect", &show_rect_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Sprite", &show_sprite_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Cursor", &show_cursor_);

    ImGui::SliderFloat("Rect Intensity", &rect_intensity_, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Sprite Intensity", &sprite_intensity_, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Cursor Intensity", &cursor_intensity_, 0.0f, 3.0f, "%.2f");
}

UiVisualTest::UiVisualTest()
    : VisualTestCase(
          "UI",
          "UI",
          "验证 UI 绘制路径（屏幕坐标、裁剪、颜色/透明度）。",
          "1) 观察 UI 图片与灰色矩形。\n"
          "2) 移动相机，确认 UI 不应随相机移动（若 UI 采用屏幕坐标）。",
          "- UI 元素固定在屏幕坐标。\n"
          "- 颜色/透明度正常。",
          "- UI 被相机影响：说明 UI/world 坐标混用。\n"
          "- 贴图/Rect 错误：UI 图像裁剪不对。") {
    applyDefaults();
}

void UiVisualTest::applyDefaults() {
    show_ui_image_ = true;
    show_ui_rect_ = true;
    ui_image_pos_ = glm::vec2{-200.0f, -300.0f};
    ui_image_size_ = glm::vec2{100.0f, 100.0f};
    ui_rect_pos_ = glm::vec2{-100.0f, -100.0f};
    ui_rect_size_ = glm::vec2{100.0f, 100.0f};
    ui_rect_color_ = glm::vec4{0.5f, 0.5f, 0.5f, 1.0f};
}

void UiVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();
    if (show_ui_image_) {
        renderer.drawUIImage(engine::render::Image{"assets/textures/UI/bluebar_00.png", engine::utils::Rect{0, 0, 15, 7}},
                             ui_image_pos_, glm::max(ui_image_size_, glm::vec2{1.0f, 1.0f}));
    }

    if (show_ui_rect_) {
        auto ui_rect_color = renderer.getDefaultUIColorOptions();
        ui_rect_color.use_gradient = false;
        ui_rect_color.start_color = engine::utils::FColor{ui_rect_color_.x, ui_rect_color_.y, ui_rect_color_.z, ui_rect_color_.w};
        ui_rect_color.end_color = ui_rect_color.start_color;
        renderer.drawUIFilledRect(engine::utils::Rect{ui_rect_pos_, glm::max(ui_rect_size_, glm::vec2{1.0f, 1.0f})}, &ui_rect_color);
    }
}

void UiVisualTest::onImGui(engine::core::Context& context) {
    (void)context;

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        applyDefaults();
    }

    ImGui::Checkbox("Show UIImage", &show_ui_image_);
    ImGui::SameLine();
    ImGui::Checkbox("Show UI Rect", &show_ui_rect_);

    ImGui::DragFloat2("Image Pos", &ui_image_pos_.x, 1.0f, -2000.0f, 2000.0f, "%.1f");
    ImGui::DragFloat2("Image Size", &ui_image_size_.x, 1.0f, 1.0f, 2000.0f, "%.1f");

    ImGui::DragFloat2("Rect Pos", &ui_rect_pos_.x, 1.0f, -2000.0f, 2000.0f, "%.1f");
    ImGui::DragFloat2("Rect Size", &ui_rect_size_.x, 1.0f, 1.0f, 2000.0f, "%.1f");
    ImGui::ColorEdit4("Rect Color", &ui_rect_color_.x);
}

TextRenderingVisualTest::TextRenderingVisualTest()
    : VisualTestCase(
          "Text Rendering",
          "Text",
          "验证中文/多行/对齐/渐变/缩放(glyph_scale) 等边界用例；并验证字体可重复加载/卸载。",
          "1) 观察世界文本与 UI 文本：中文/多行/混排/符号。\n"
          "2) 在面板中切换对齐、字号、glyph_scale、letter/line spacing、渐变。\n"
          "3) 旋转/缩放相机：世界文本随相机变化，UI 文本不随相机变化。\n"
          "4) 切换到其他用例再切回：确认字体可重复加载/卸载。",
          "- 中文可显示，多行换行正确。\n"
          "- 对齐与 bounds 框一致；渐变方向可控；glyph_scale 生效。\n"
          "- 来回切换用例不崩溃、不丢字。",
          "- 字体路径/加载失败：文本不显示。\n"
          "- atlas/layout 问题：字符缺失、重叠或抖动。\n"
          "- glyph_scale/spacing 极端：出现断行/重叠/抖动。") {
    font_id_ = entt::hashed_string::value(font_path_);
    applyDefaults();
}

void TextRenderingVisualTest::onEnter(engine::core::Context& context) {
    ensureFontLoaded(context);
}

void TextRenderingVisualTest::onExit(engine::core::Context& context) {
    if (!font_loaded_) {
        return;
    }

    auto& resources = context.getResourceManager();
    resources.unloadFont(font_id_, font_size_);
    font_loaded_ = false;
}

void TextRenderingVisualTest::onRender(engine::core::Context& context) {
    ensureFontLoaded(context);

    auto& text_renderer = context.getTextRenderer();
    auto& renderer = context.getRenderer();

    engine::utils::LayoutOptions layout{};
    layout.letter_spacing = letter_spacing_;
    layout.line_spacing_scale = line_spacing_scale_;
    layout.glyph_scale = glyph_scale_;

    const engine::utils::FColor start{gradient_start_.x, gradient_start_.y, gradient_start_.z, gradient_start_.w};
    const engine::utils::FColor end{gradient_end_.x, gradient_end_.y, gradient_end_.z, gradient_end_.w};

    constexpr std::string_view kWorldText =
        "中文/English 混排：你好，世界！\n"
        "多行/换行：第二行 - ABC abc 123 !@#$%()\n"
        "对齐/渐变/缩放：glyph_scale + spacing";

    constexpr std::string_view kUIText =
        "UI Text Block:\n"
        "Alignment / gradient / glyph_scale\n"
        "The quick brown fox jumps over the lazy dog. 0123456789";

    if (draw_world_) {
        auto world_params = text_renderer.getTextStyle(text_renderer.getDefaultWorldStyleKey());
        world_params.layout = layout;
        world_params.color.use_gradient = use_gradient_;
        world_params.color.start_color = use_gradient_ ? start : engine::utils::FColor::white();
        world_params.color.end_color = use_gradient_ ? end : world_params.color.start_color;
        world_params.color.angle_radians = glm::radians(gradient_angle_deg_);

        text_renderer.setTextStyle("world/visual_test_edge", world_params);

        const glm::vec2 size = text_renderer.getTextSize(kWorldText, font_id_, font_size_, &layout);
        glm::vec2 pos = world_anchor_;
        if (alignment_ == 1) {
            pos.x -= size.x * 0.5f;
        } else if (alignment_ == 2) {
            pos.x -= size.x;
        }

        if (show_bounds_) {
            auto bounds_color = renderer.getDefaultWorldColorOptions();
            bounds_color.use_gradient = false;
            bounds_color.start_color = engine::utils::FColor{0.05f, 0.05f, 0.05f, 0.45f};
            bounds_color.end_color = bounds_color.start_color;
            renderer.drawFilledRect(engine::utils::Rect{pos, size}, &bounds_color);
        }

        text_renderer.drawText(kWorldText, font_id_, font_size_, pos, "world/visual_test_edge");
    }

    if (draw_ui_) {
        auto ui_params = text_renderer.getTextStyle(text_renderer.getDefaultUIStyleKey());
        ui_params.layout = layout;
        ui_params.color.use_gradient = use_gradient_;
        ui_params.color.start_color = use_gradient_ ? engine::utils::FColor::white() : engine::utils::FColor::white();
        ui_params.color.end_color = use_gradient_ ? end : ui_params.color.start_color;
        ui_params.color.angle_radians = glm::radians(gradient_angle_deg_);

        text_renderer.setTextStyle("ui/visual_test_edge", ui_params);

        const glm::vec2 size = text_renderer.getTextSize(kUIText, font_id_, font_size_, &layout);
        glm::vec2 pos = ui_anchor_;
        if (alignment_ == 1) {
            pos.x -= size.x * 0.5f;
        } else if (alignment_ == 2) {
            pos.x -= size.x;
        }

        if (show_bounds_) {
            auto bounds_color = renderer.getDefaultUIColorOptions();
            bounds_color.use_gradient = false;
            bounds_color.start_color = engine::utils::FColor{0.05f, 0.05f, 0.05f, 0.45f};
            bounds_color.end_color = bounds_color.start_color;
            renderer.drawUIFilledRect(engine::utils::Rect{pos, size}, &bounds_color);
        }

        text_renderer.drawUIText(kUIText, font_id_, font_size_, pos, "ui/visual_test_edge");
    }
}

void TextRenderingVisualTest::applyDefaults() {
    font_size_ = 16;
    draw_world_ = true;
    draw_ui_ = true;
    show_bounds_ = true;
    alignment_ = 0;
    world_anchor_ = glm::vec2{-220.0f, -160.0f};
    ui_anchor_ = glm::vec2{-220.0f, 20.0f};
    use_gradient_ = true;
    gradient_angle_deg_ = 45.0f;
    gradient_start_ = glm::vec4{1.0f, 1.0f, 0.0f, 1.0f};
    gradient_end_ = glm::vec4{1.0f, 0.0f, 0.0f, 1.0f};
    letter_spacing_ = 0.0f;
    line_spacing_scale_ = 1.0f;
    glyph_scale_ = glm::vec2{1.0f, 1.0f};
}

void TextRenderingVisualTest::ensureFontLoaded(engine::core::Context& context) {
    if (font_loaded_) {
        return;
    }
    auto& resources = context.getResourceManager();
    font_loaded_ = resources.loadFont(font_id_, font_size_, font_path_) != nullptr;
}

void TextRenderingVisualTest::onImGui(engine::core::Context& context) {
    auto& resources = context.getResourceManager();
    auto& text_renderer = context.getTextRenderer();

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults")) {
        const int old_size = font_size_;
        applyDefaults();
        if (font_loaded_) {
            resources.unloadFont(font_id_, old_size);
            font_loaded_ = false;
        }
        text_renderer.clearLayoutCache();
        ensureFontLoaded(context);
    }

    ImGui::Checkbox("Draw World Text", &draw_world_);
    ImGui::SameLine();
    ImGui::Checkbox("Draw UI Text", &draw_ui_);
    ImGui::SameLine();
    ImGui::Checkbox("Show Bounds", &show_bounds_);

    const char* align_items[] = {"Left", "Center", "Right"};
    ImGui::Combo("Align", &alignment_, align_items, 3);

    ImGui::DragFloat2("World Anchor", &world_anchor_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");
    ImGui::DragFloat2("UI Anchor", &ui_anchor_.x, 1.0f, -5000.0f, 5000.0f, "%.1f");

    int new_size = font_size_;
    if (ImGui::SliderInt("Font Size", &new_size, 8, 64)) {
        const int old_size = font_size_;
        font_size_ = std::clamp(new_size, 1, 256);
        if (font_loaded_) {
            resources.unloadFont(font_id_, old_size);
            font_loaded_ = false;
        }
        text_renderer.clearLayoutCache();
        ensureFontLoaded(context);
    }

    ImGui::Checkbox("Use Gradient", &use_gradient_);
    ImGui::SliderFloat("Gradient Angle (deg)", &gradient_angle_deg_, 0.0f, 360.0f, "%.1f");
    ImGui::ColorEdit4("Gradient Start", &gradient_start_.x);
    ImGui::ColorEdit4("Gradient End", &gradient_end_.x);

    ImGui::DragFloat("Letter Spacing", &letter_spacing_, 0.1f, -10.0f, 50.0f, "%.1f");
    ImGui::DragFloat("Line Spacing Scale", &line_spacing_scale_, 0.01f, 0.1f, 4.0f, "%.2f");
    ImGui::DragFloat2("Glyph Scale", &glyph_scale_.x, 0.01f, 0.1f, 4.0f, "%.2f");

    if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Font: %s", font_path_);
        ImGui::Text("Layout revision: %llu", static_cast<unsigned long long>(text_renderer.getLayoutRevision()));
    }
}

AudioVisualTest::AudioVisualTest()
    : VisualTestCase(
          "Audio",
          "Audio",
          "验证音乐/音效资源加载、播放与退出时的清理。",
          "1) 进入该用例后应自动播放音乐（循环）。\n"
          "2) 通过面板按钮播放/停止音乐、播放音效。\n"
          "3) 切换到其他用例再切回，确认资源能正常 unload/reload。",
          "- 按钮触发后能听到音乐/音效（设备可用时）。\n"
          "- 切换用例不会崩溃或叠加重复播放。",
          "- 音频设备不可用：无声音但不应崩溃。\n"
          "- 资源路径缺失：播放失败日志。\n"
          "- 清理不彻底：切换后重复播放或泄漏。") {}

void AudioVisualTest::onEnter(engine::core::Context& context) {
    if (audio_loaded_) {
        return;
    }

    auto& resources = context.getResourceManager();
    resources.loadMusic(music_id_, music_path_);
    resources.loadSound(sound_id_, sound_path_);
    audio_loaded_ = true;
    auto& audio_player = context.getAudioPlayer();
    audio_player.playMusic(music_id_, true, 0);
}

void AudioVisualTest::onExit(engine::core::Context& context) {
    if (!audio_loaded_) {
        return;
    }

    auto& audio_player = context.getAudioPlayer();
    audio_player.stopMusic();

    auto& resources = context.getResourceManager();
    resources.unloadMusic(music_id_);
    resources.unloadSound(sound_id_);
    audio_loaded_ = false;
}

void AudioVisualTest::onRender(engine::core::Context& context) {
    auto& renderer = context.getRenderer();
    auto highlight = renderer.getDefaultWorldColorOptions();
    highlight.use_gradient = false;
    highlight.start_color = engine::utils::FColor::purple();
    highlight.end_color = highlight.start_color;
    renderer.drawFilledRect(engine::utils::Rect{-120.0f, -60.0f, 80.0f, 40.0f}, &highlight);
}

void AudioVisualTest::onImGui(engine::core::Context& context) {
    auto& audio_player = context.getAudioPlayer();

    if (ImGui::Button("Reset (Stop + Replay Music)")) {
        audio_player.stopMusic();
        audio_player.playMusic(music_id_, true, 0);
    }
    ImGui::Separator();

    ImGui::TextUnformatted("音频测试");
    if (ImGui::Button("播放音乐")) {
        audio_player.playMusic(music_id_, true, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("停止音乐")) {
        audio_player.stopMusic();
    }
    if (ImGui::Button("播放音效")) {
        [[maybe_unused]] const bool played =
            audio_player.playSound2D(sound_id_, glm::vec2{0.0f, 0.0f}, glm::vec2{200.0f, 200.0f});
    }
}

} // namespace tools::visual
