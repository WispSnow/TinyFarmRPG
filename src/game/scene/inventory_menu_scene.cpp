#include "inventory_menu_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/data/item_catalog.h"
#include "game/scene/inventory_menu_character_panel.h"
#include "game/ui/inventory_tab_content.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace entt::literals;

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";
using SlotGridViewModels = std::vector<game::ui::SlotGridViewModel>;

class PlaceholderTabContent final : public game::ui::IMenuTabContent {
public:
    [[nodiscard]] bool bindModel(Rml::DataModelConstructor&) override {
        return true;
    }

    void onActivated() override {}
    void onDeactivated() override {}
    void update(float /*delta_time*/) override {}

    [[nodiscard]] bool onCancel() override {
        return false;
    }
};

[[nodiscard]] std::optional<game::ui::MenuTabId> toMenuTabId(int tab_index) {
    switch (tab_index) {
        case 0:
            return game::ui::MenuTabId::Inventory;
        case 1:
            return game::ui::MenuTabId::Equipment;
        case 2:
            return game::ui::MenuTabId::Quests;
        case 3:
            return game::ui::MenuTabId::Map;
        case 4:
            return game::ui::MenuTabId::Options;
        default:
            return std::nullopt;
    }
}

} // namespace

namespace game::scene {

InventoryMenuScene::InventoryMenuScene(std::string_view name,
                                       engine::core::Context& context,
                                       entt::registry& game_registry,
                                       entt::entity player,
                                       game::data::ItemCatalog* item_catalog)
    : engine::scene::Scene(name, context),
      game_registry_(game_registry),
      player_(player),
      item_catalog_(item_catalog),
      previous_state_(context.getGameState().getCurrentState()) {}

InventoryMenuScene::~InventoryMenuScene() {
    disconnectRuntimeListeners();
    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }
    tabs_.clear();
    shutdownUI();
}

bool InventoryMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs)
        .connect<&InventoryMenuScene::onMenuCancelPressed>(this);

    return Scene::init();
}

void InventoryMenuScene::update(float delta_time) {
    if (auto* tab = activeTab()) {
        tab->update(delta_time);
    }
    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }
    tabs_.clear();
    shutdownUI();
    disconnectRuntimeListeners();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool InventoryMenuScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("InventoryMenuScene: RmlUiRuntime 不可用。");
        return false;
    }

    active_tab_id_ = game::ui::MenuTabId::Inventory;

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("InventoryMenuScene: 创建 data model 失败。");
        return false;
    }

    if (!data_types_registered_) {
        if (!game::ui::registerSlotGridViewModelType(constructor)) {
            spdlog::error("InventoryMenuScene: RegisterStruct<SlotGridViewModel> 失败。");
            document_controller_.unload();
            return false;
        }

        if (!constructor.RegisterArray<SlotGridViewModels>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<SlotGridViewModels> 失败。");
            document_controller_.unload();
            return false;
        }

        data_types_registered_ = true;
    }

    constructor.Bind("char_name", &char_name_);
    constructor.Bind("char_title", &char_title_);
    constructor.Bind("gold_label", &gold_label_);
    constructor.Bind("farm_label", &farm_label_);

    if (!constructor.BindEventCallback(
            "switch_tab",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                switchTabFromTabsetIndex(game::ui::getSingleIntArgument(arguments));
            })) {
        spdlog::error("InventoryMenuScene: 绑定 switch_tab 回调失败。");
        document_controller_.unload();
        return false;
    }

    tabs_.clear();
    auto inventory_tab = std::make_unique<game::ui::InventoryTabContent>(
        context_, document_controller_, game_registry_, player_, item_catalog_, instanceId());
    if (!inventory_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: InventoryTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Inventory, std::move(inventory_tab));
    tabs_.emplace(game::ui::MenuTabId::Equipment, std::make_unique<PlaceholderTabContent>());
    tabs_.emplace(game::ui::MenuTabId::Quests, std::make_unique<PlaceholderTabContent>());
    tabs_.emplace(game::ui::MenuTabId::Map, std::make_unique<PlaceholderTabContent>());
    tabs_.emplace(game::ui::MenuTabId::Options, std::make_unique<PlaceholderTabContent>());

    if (!document_controller_.load(DOCUMENT_PATH)) {
        tabs_.clear();
        document_controller_.unload();
        spdlog::error("InventoryMenuScene: 加载 RML 文档失败。");
        return false;
    }

    syncCharacterPanel();
    if (auto* tab = activeTab()) {
        tab->onActivated();
    }
    document_controller_.markAllDirty();
    return true;
}

void InventoryMenuScene::shutdownUI() {
    document_controller_.unload();
}

void InventoryMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs)
        .disconnect<&InventoryMenuScene::onMenuCancelPressed>(this);
}

void InventoryMenuScene::syncCharacterPanel() {
    const InventoryMenuCharacterPanelData data =
        buildInventoryMenuCharacterPanelData(game_registry_, player_);
    char_name_ = data.char_name;
    char_title_ = data.char_title;
    gold_label_ = data.gold_label;
    farm_label_ = data.farm_label;

    document_controller_.markDirty("char_name");
    document_controller_.markDirty("char_title");
    document_controller_.markDirty("gold_label");
    document_controller_.markDirty("farm_label");
}

bool InventoryMenuScene::onMenuCancelPressed() {
    if (auto* tab = activeTab(); tab && tab->onCancel()) {
        return true;
    }

    requestPopScene();
    return true;
}

void InventoryMenuScene::switchTab(game::ui::MenuTabId new_tab) {
    const auto next_it = tabs_.find(new_tab);
    if (new_tab == active_tab_id_ || next_it == tabs_.end()) {
        return;
    }

    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }

    active_tab_id_ = new_tab;
    next_it->second->onActivated();
}

void InventoryMenuScene::switchTabFromTabsetIndex(int tab_index) {
    if (const auto tab_id = toMenuTabId(tab_index)) {
        switchTab(*tab_id);
    }
}

game::ui::IMenuTabContent* InventoryMenuScene::activeTab() {
    if (const auto it = tabs_.find(active_tab_id_); it != tabs_.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace game::scene
