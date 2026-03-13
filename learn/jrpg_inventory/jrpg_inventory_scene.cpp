#include "jrpg_inventory_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/hover_focus_sync_listener.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <string_view>

namespace {

[[nodiscard]] int parseIndexedElementId(const Rml::String& id, std::string_view prefix) {
    if (id.rfind(prefix.data(), 0) != 0) {
        return -1;
    }

    const char* number = id.c_str() + static_cast<std::ptrdiff_t>(prefix.size());
    if (*number == '\0') {
        return -1;
    }

    for (const char* p = number; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return -1;
        }
    }

    return std::atoi(number);
}

} // namespace

namespace learn::jrpg {

// ── Icon flag helpers ─────────────────────────────────────

void JrpgInventoryScene::ItemData::setIconFlags() {
    ic0 = (icon_id == 0); ic1 = (icon_id == 1);
    ic2 = (icon_id == 2); ic3 = (icon_id == 3);
    ic4 = (icon_id == 4); ic5 = (icon_id == 5);
    ic6 = (icon_id == 6); ic7 = (icon_id == 7);
}

void JrpgInventoryScene::EquipSlot::setIconFlags(int icon_id) {
    ic0 = (icon_id == 0); ic1 = (icon_id == 1);
    ic2 = (icon_id == 2); ic3 = (icon_id == 3);
    ic4 = (icon_id == 4); ic5 = (icon_id == 5);
    ic6 = (icon_id == 6); ic7 = (icon_id == 7);
}

void JrpgInventoryScene::PartyMember::refreshStatus() {
    if (isDown()) {
        status_text = "KO";
    } else if (poisoned) {
        status_text = "Poison";
    } else {
        status_text = "OK";
    }

    target_summary = std::format("{} [{}]  HP {}/{}  MP {}/{}",
                                 name, status_text, hp, max_hp, mp, max_mp);
}

// ── UIEventListener ───────────────────────────────────────

void JrpgInventoryScene::UIEventListener::ProcessEvent(Rml::Event& event) {
    const auto& type = event.GetType();

    if (type == "keydown") {
        const auto key = event.GetParameter<int>("key_identifier", 0);

        if (scene_.show_targets_) {
            if (key == static_cast<int>(Rml::Input::KI_ESCAPE)) {
                scene_.closeTargetPanel();
                event.StopPropagation();
                return;
            }
            if (key == static_cast<int>(Rml::Input::KI_UP)) {
                const int next_idx = scene_.findNextValidTargetIndex(scene_.selected_target_idx_, -1);
                if (next_idx >= 0) {
                    scene_.showTargetPreview(next_idx);
                    scene_.focusTargetElement(next_idx);
                }
                event.StopPropagation();
                return;
            }
            if (key == static_cast<int>(Rml::Input::KI_DOWN)) {
                const int next_idx = scene_.findNextValidTargetIndex(scene_.selected_target_idx_, 1);
                if (next_idx >= 0) {
                    scene_.showTargetPreview(next_idx);
                    scene_.focusTargetElement(next_idx);
                }
                event.StopPropagation();
                return;
            }
            if (key == static_cast<int>(Rml::Input::KI_RETURN) && scene_.selected_target_idx_ >= 0) {
                scene_.confirmTargetUse(scene_.selected_target_idx_);
                event.StopPropagation();
                return;
            }
        }

        if (key == static_cast<int>(Rml::Input::KI_ESCAPE)) {
            if (scene_.show_candidates_) {
                scene_.closeCandidateList();
                event.StopPropagation();
            }
            return;
        }
        return;
    }

    if (type == "click") {
        for (auto* element = event.GetTargetElement(); element != nullptr; element = element->GetParentNode()) {
            if (const int idx = parseIndexedElementId(element->GetId(), "target-"); idx >= 0) {
                scene_.confirmTargetUse(idx);
                event.StopPropagation();
                return;
            }
        }
        return;
    }

    if (type != "focus") {
        return;
    }

    auto* target = event.GetTargetElement();
    if (!target) {
        return;
    }

    const auto& id = target->GetId();
    if (const int idx = parseIndexedElementId(id, "item-"); idx >= 0) {
        scene_.showItemDetail(idx);
        return;
    }
    if (const int idx = parseIndexedElementId(id, "slot-"); idx >= 0) {
        scene_.showEquipDetail(idx);
        return;
    }
    if (const int idx = parseIndexedElementId(id, "cand-"); idx >= 0) {
        scene_.showCandidatePreview(idx);
        return;
    }
    if (const int idx = parseIndexedElementId(id, "target-"); idx >= 0) {
        scene_.showTargetPreview(idx);
    }
}

// ── Scene lifecycle ───────────────────────────────────────

bool JrpgInventoryScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    buildData();
    setupDataModel();

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_jrpg_inventory.rml");
    if (!doc_) {
        spdlog::error("JrpgInventoryScene: failed to load RML");
        return false;
    }

    listener_ = std::make_unique<UIEventListener>(*this);
    addListener(doc_, "keydown");
    addListener(doc_, "click", true);
    addListener(doc_, "focus", true);
    hover_focus_listener_ = std::make_unique<engine::ui::rmlui::HoverFocusSyncListener>(
        *context_.getGLRenderer().getRmlUILayer(),
        [](Rml::Element* element) {
            return element != nullptr && element->IsClassSet("target-item")
                && !element->IsClassSet("disabled");
        });
    doc_->AddEventListener("mouseover", hover_focus_listener_.get());
    hover_listener_registered_ = true;

    // Initial state
    filterItems();
    refreshCharStats();
    focus_nav_deferred_ = true;

    spdlog::info("JrpgInventoryScene initialized");
    return true;
}

void JrpgInventoryScene::update(float dt) {
    Scene::update(dt);

    // Deferred focus: nav bar
    if (focus_nav_deferred_) {
        if (auto* el = doc_->GetElementById("nav-items")) {
            el->Focus(true);
            focus_nav_deferred_ = false;
        }
    }

    // Deferred focus: item grid
    if (focus_grid_deferred_) {
        if (auto* grid = doc_->GetElementById("item-grid")) {
            if (grid->GetNumChildren() > 0) {
                grid->GetChild(0)->Focus(true);
                focus_grid_deferred_ = false;
            }
        }
    }

    // Deferred focus: equip slots
    if (focus_slots_deferred_) {
        if (auto* list = doc_->GetElementById("equip-slot-list")) {
            if (list->GetNumChildren() > 0) {
                list->GetChild(0)->Focus(true);
                focus_slots_deferred_ = false;
            }
        }
    }

    // Deferred focus: candidate list
    if (focus_candidate_deferred_) {
        if (auto* list = doc_->GetElementById("candidate-list")) {
            if (list->GetNumChildren() > 0) {
                list->GetChild(0)->Focus(true);
                focus_candidate_deferred_ = false;
            }
        }
    }

    // Deferred focus: target list
    if (focus_target_deferred_) {
        if (auto* list = doc_->GetElementById("target-list")) {
            if (list->GetNumChildren() > 0) {
                const int preferred_idx = getPreferredTargetIndex();
                if (preferred_idx >= 0) {
                    focusTargetElement(preferred_idx);
                    focus_target_deferred_ = false;
                }
            }
        }
    }

    if (show_targets_ && !focus_target_deferred_ && !hasFocusedTargetElement()) {
        const int preferred_idx = getPreferredTargetIndex();
        if (preferred_idx >= 0) {
            focusTargetElement(preferred_idx);
        }
    }
}

void JrpgInventoryScene::clean() {
    for (auto& [el, event, capture] : registrations_) {
        el->RemoveEventListener(event, listener_.get(), capture);
    }
    registrations_.clear();
    listener_.reset();
    if (doc_ && hover_listener_registered_ && hover_focus_listener_) {
        doc_->RemoveEventListener("mouseover", hover_focus_listener_.get());
    }
    hover_listener_registered_ = false;
    hover_focus_listener_.reset();

    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext()) {
        rml_ctx->RemoveDataModel("inventory");
    }

    Scene::clean();
}

// ── Data model setup ──────────────────────────────────────

void JrpgInventoryScene::setupDataModel() {
    auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext();
    if (!rml_ctx) return;

    auto ctor = rml_ctx->CreateDataModel("inventory");
    if (!ctor) return;

    // ItemData struct
    if (auto sh = ctor.RegisterStruct<ItemData>()) {
        sh.RegisterMember("name",     &ItemData::name);
        sh.RegisterMember("desc",     &ItemData::desc);
        sh.RegisterMember("qty",      &ItemData::qty);
        sh.RegisterMember("ic0",      &ItemData::ic0);
        sh.RegisterMember("ic1",      &ItemData::ic1);
        sh.RegisterMember("ic2",      &ItemData::ic2);
        sh.RegisterMember("ic3",      &ItemData::ic3);
        sh.RegisterMember("ic4",      &ItemData::ic4);
        sh.RegisterMember("ic5",      &ItemData::ic5);
        sh.RegisterMember("ic6",      &ItemData::ic6);
        sh.RegisterMember("ic7",      &ItemData::ic7);
    }
    ctor.RegisterArray<std::vector<ItemData>>();

    // EquipSlot struct
    if (auto sh = ctor.RegisterStruct<EquipSlot>()) {
        sh.RegisterMember("slot_name",     &EquipSlot::slot_name);
        sh.RegisterMember("equipped_name", &EquipSlot::equipped_name);
        sh.RegisterMember("is_selected",   &EquipSlot::is_selected);
        sh.RegisterMember("ic0",           &EquipSlot::ic0);
        sh.RegisterMember("ic1",           &EquipSlot::ic1);
        sh.RegisterMember("ic2",           &EquipSlot::ic2);
        sh.RegisterMember("ic3",           &EquipSlot::ic3);
        sh.RegisterMember("ic4",           &EquipSlot::ic4);
        sh.RegisterMember("ic5",           &EquipSlot::ic5);
        sh.RegisterMember("ic6",           &EquipSlot::ic6);
        sh.RegisterMember("ic7",           &EquipSlot::ic7);
    }
    ctor.RegisterArray<std::vector<EquipSlot>>();

    // EquipCandidate struct
    if (auto sh = ctor.RegisterStruct<EquipCandidate>()) {
        sh.RegisterMember("name",      &EquipCandidate::name);
        sh.RegisterMember("atk_delta", &EquipCandidate::atk_delta);
        sh.RegisterMember("def_delta", &EquipCandidate::def_delta);
        sh.RegisterMember("spd_delta", &EquipCandidate::spd_delta);
    }
    ctor.RegisterArray<std::vector<EquipCandidate>>();

    // StatDelta struct
    if (auto sh = ctor.RegisterStruct<StatDelta>()) {
        sh.RegisterMember("label",       &StatDelta::label);
        sh.RegisterMember("display",     &StatDelta::display);
        sh.RegisterMember("is_positive", &StatDelta::is_positive);
        sh.RegisterMember("is_negative", &StatDelta::is_negative);
    }
    ctor.RegisterArray<std::vector<StatDelta>>();

    // PartyMember struct
    if (auto sh = ctor.RegisterStruct<PartyMember>()) {
        sh.RegisterMember("name",        &PartyMember::name);
        sh.RegisterMember("hp",          &PartyMember::hp);
        sh.RegisterMember("max_hp",      &PartyMember::max_hp);
        sh.RegisterMember("mp",          &PartyMember::mp);
        sh.RegisterMember("max_mp",      &PartyMember::max_mp);
        sh.RegisterMember("disabled",    &PartyMember::disabled);
        sh.RegisterMember("is_selected", &PartyMember::is_selected);
        sh.RegisterMember("status_text", &PartyMember::status_text);
        sh.RegisterMember("target_summary", &PartyMember::target_summary);
    }
    ctor.RegisterArray<std::vector<PartyMember>>();

    // Bind arrays
    ctor.Bind("filtered_items", &filtered_items_);
    ctor.Bind("equip_slots",    &equip_slots_);
    ctor.Bind("candidates",     &candidates_);
    ctor.Bind("stat_deltas",    &stat_deltas_);
    ctor.Bind("party",          &party_);

    // Bind scalars
    ctor.Bind("view_mode",       &view_mode_);
    ctor.Bind("is_items_view",   &is_items_view_);
    ctor.Bind("is_equip_view",   &is_equip_view_);
    ctor.Bind("tab_all_active",        &tab_all_active_);
    ctor.Bind("tab_consumable_active", &tab_consumable_active_);
    ctor.Bind("tab_equipment_active",  &tab_equipment_active_);
    ctor.Bind("tab_key_active",        &tab_key_active_);
    ctor.Bind("can_use",         &can_use_);
    ctor.Bind("show_use_button", &show_use_button_);
    ctor.Bind("show_candidates", &show_candidates_);
    ctor.Bind("show_targets",    &show_targets_);
    ctor.Bind("detail_name",     &detail_name_);
    ctor.Bind("detail_desc",     &detail_desc_);
    ctor.Bind("gold_text",       &gold_text_);
    ctor.Bind("target_panel_title", &target_panel_title_);
    ctor.Bind("char_atk_text",   &char_atk_text_);
    ctor.Bind("char_def_text",   &char_def_text_);
    ctor.Bind("char_spd_text",   &char_spd_text_);
    ctor.Bind("show_detail_icon", &show_detail_icon_);
    ctor.Bind("det_ic0", &det_ic0_); ctor.Bind("det_ic1", &det_ic1_);
    ctor.Bind("det_ic2", &det_ic2_); ctor.Bind("det_ic3", &det_ic3_);
    ctor.Bind("det_ic4", &det_ic4_); ctor.Bind("det_ic5", &det_ic5_);
    ctor.Bind("det_ic6", &det_ic6_); ctor.Bind("det_ic7", &det_ic7_);

    // Event callbacks
    ctor.BindEventCallback("on_nav",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) switchView(args[0].Get<int>(0));
        });

    ctor.BindEventCallback("on_tab",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) switchTab(args[0].Get<int>(0));
        });

    ctor.BindEventCallback("on_item_select",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) showItemDetail(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_use_item",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            onUseItem();
        });

    ctor.BindEventCallback("on_slot_select",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) openCandidateList(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_slot_focus",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) showEquipDetail(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_candidate_select",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) confirmEquip(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_candidate_focus",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) showCandidatePreview(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_target_focus",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) showTargetPreview(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_target_select",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) confirmTargetUse(args[0].Get<int>(-1));
        });

    model_handle_ = ctor.GetModelHandle();
}

// ── Build game data ───────────────────────────────────────

void JrpgInventoryScene::buildData() {
    auto makeItem = [](
        std::string name,
        std::string desc,
        int category,
        int qty,
        int icon_id,
        int equip_slot = -1,
        int atk = 0,
        int def = 0,
        int spd = 0,
        int heal_hp = 0,
        int heal_mp = 0,
        int revive_hp = 0,
        bool cure_poison = false) {
        ItemData item{};
        item.name = std::move(name);
        item.desc = std::move(desc);
        item.category = category;
        item.qty = qty;
        item.icon_id = icon_id;
        item.equip_slot = equip_slot;
        item.atk = atk;
        item.def = def;
        item.spd = spd;
        item.heal_hp = heal_hp;
        item.heal_mp = heal_mp;
        item.revive_hp = revive_hp;
        item.cure_poison = cure_poison;
        return item;
    };
    auto makePartyMember = [](std::string name, int hp, int max_hp, int mp, int max_mp, bool poisoned = false) {
        PartyMember member{};
        member.name = std::move(name);
        member.hp = hp;
        member.max_hp = max_hp;
        member.mp = mp;
        member.max_mp = max_mp;
        member.poisoned = poisoned;
        return member;
    };

    // icon_id mapping:
    // 0=potion, 1=hi-potion, 2=ether, 3=antidote
    // 4=sword, 5=shield, 6=helmet, 7=armor/ring

    all_items_ = {
        // Consumables (category=0)
        makeItem("Potion",       "Restores 50 HP.",        kCatConsumable, 5, 0, -1, 0, 0, 0, 50),
        makeItem("Hi-Potion",    "Restores 120 HP.",       kCatConsumable, 2, 1, -1, 0, 0, 0, 120),
        makeItem("Ether",        "Restores 30 MP.",        kCatConsumable, 3, 2, -1, 0, 0, 0, 0, 30),
        makeItem("Antidote",     "Cures poison.",          kCatConsumable, 4, 3, -1, 0, 0, 0, 0, 0, 0, true),
        makeItem("Phoenix Down", "Revives a fallen ally.", kCatConsumable, 1, 0, -1, 0, 0, 0, 0, 0, 60),
        // Equipment (category=1)
        makeItem("Iron Sword",    "A sturdy iron sword.",     kCatEquipment, 1, 4, kSlotWeapon,    12, 0,  0),
        makeItem("Steel Sword",   "Forged from fine steel.",  kCatEquipment, 1, 4, kSlotWeapon,    18, 0,  2),
        makeItem("Bronze Shield", "A basic bronze shield.",   kCatEquipment, 1, 5, kSlotShield,     0, 8,  0),
        makeItem("Iron Shield",   "Solid iron protection.",   kCatEquipment, 1, 5, kSlotShield,     0, 14, 0),
        makeItem("Leather Cap",   "Light leather headgear.",  kCatEquipment, 1, 6, kSlotHelmet,     0, 3,  1),
        makeItem("Iron Helm",     "Heavy iron helmet.",       kCatEquipment, 1, 6, kSlotHelmet,     0, 7, -1),
        makeItem("Chain Mail",    "Interlocked chain armor.", kCatEquipment, 1, 7, kSlotArmor,      0, 12, -2),
        makeItem("Plate Armor",   "Heavy plate protection.",  kCatEquipment, 1, 7, kSlotArmor,      2, 20, -4),
        makeItem("Speed Ring",    "Boosts agility.",          kCatEquipment, 1, 7, kSlotAccessory,  0, 0,  5),
        makeItem("Power Ring",    "Boosts attack power.",     kCatEquipment, 1, 7, kSlotAccessory,  5, 0,  0),
        // Key Items (category=2)
        makeItem("Old Map",      "A weathered treasure map.",    kCatKeyItem, 1, 3),
        makeItem("Royal Letter", "Sealed letter from the king.", kCatKeyItem, 1, 3),
    };

    for (int i = 0; i < static_cast<int>(all_items_.size()); ++i) {
        all_items_[i].source_index = i;
        all_items_[i].setIconFlags();
    }

    // Equipment slots (start with some items equipped)
    equip_slots_ = {
        {"Weapon",    "Iron Sword",   kSlotWeapon,    false, 5},
        {"Shield",    "(empty)",      kSlotShield,    false, -1},
        {"Helmet",    "Leather Cap",  kSlotHelmet,    false, 9},
        {"Armor",     "Chain Mail",   kSlotArmor,     false, 11},
        {"Accessory", "(empty)",      kSlotAccessory, false, -1},
    };

    // Set icon flags for equipped items
    for (auto& slot : equip_slots_) {
        if (slot.equipped_idx >= 0) {
            slot.setIconFlags(all_items_[slot.equipped_idx].icon_id);
            all_items_[slot.equipped_idx].qty = std::max(0, all_items_[slot.equipped_idx].qty - 1);
        } else {
            slot.setIconFlags(-1); // all false
        }
    }

    party_ = {
        makePartyMember("Aelindra", 180, 220, 28, 50),
        makePartyMember("Bjorn",    260, 320, 10, 24),
        makePartyMember("Ciel",       0, 180, 45, 70),
        makePartyMember("Dusk",     120, 240, 16, 40, true),
    };
    for (auto& member : party_) {
        member.refreshStatus();
    }

    // Calculate initial stats from equipment
    char_atk_ = 10; char_def_ = 8; char_spd_ = 12; // base stats
    for (auto& slot : equip_slots_) {
        if (slot.equipped_idx >= 0) {
            auto& eq = all_items_[slot.equipped_idx];
            char_atk_ += eq.atk;
            char_def_ += eq.def;
            char_spd_ += eq.spd;
        }
    }

    gold_text_ = std::to_string(gold_) + "G";
}

// ── View / tab switching ──────────────────────────────────

void JrpgInventoryScene::switchView(int mode) {
    view_mode_ = mode;
    is_items_view_ = (mode == 0);
    is_equip_view_ = (mode == 1);
    closeTargetPanel(false, false);
    closeCandidateList(false);
    clearDetail();

    model_handle_.DirtyVariable("view_mode");
    model_handle_.DirtyVariable("is_items_view");
    model_handle_.DirtyVariable("is_equip_view");

    if (mode == 0) {
        filterItems();
        focus_grid_deferred_ = true;
    } else {
        focus_slots_deferred_ = true;
    }
}

void JrpgInventoryScene::switchTab(int tab) {
    current_tab_ = tab;
    tab_all_active_ = (tab == kTabAll);
    tab_consumable_active_ = (tab == kCatConsumable);
    tab_equipment_active_ = (tab == kCatEquipment);
    tab_key_active_ = (tab == kCatKeyItem);
    closeTargetPanel(false, false);
    clearDetail();
    filterItems();

    model_handle_.DirtyVariable("tab_all_active");
    model_handle_.DirtyVariable("tab_consumable_active");
    model_handle_.DirtyVariable("tab_equipment_active");
    model_handle_.DirtyVariable("tab_key_active");

    focus_grid_deferred_ = true;
}

void JrpgInventoryScene::filterItems() {
    filtered_items_.clear();
    for (int i = 0; i < static_cast<int>(all_items_.size()); ++i) {
        const auto& item = all_items_[i];
        const bool tab_matches = (current_tab_ == kTabAll) || (item.category == current_tab_);
        if (tab_matches && item.qty > 0) {
            auto filtered = item;
            filtered.source_index = i;
            filtered_items_.push_back(std::move(filtered));
        }
    }
    model_handle_.DirtyVariable("filtered_items");
}

// ── Item detail ───────────────────────────────────────────

void JrpgInventoryScene::clearDetail() {
    selected_item_idx_ = -1;
    selected_item_master_idx_ = -1;
    detail_name_.clear();
    detail_desc_.clear();
    can_use_ = false;
    show_use_button_ = false;
    stat_deltas_.clear();
    setDetailIcon(-1);
    dirtyDetailBindings();
}

void JrpgInventoryScene::showItemDetail(int filtered_idx) {
    if (filtered_idx < 0 || filtered_idx >= static_cast<int>(filtered_items_.size())) return;

    selected_item_idx_ = filtered_idx;
    const auto& item = filtered_items_[filtered_idx];
    selected_item_master_idx_ = item.source_index;
    detail_name_ = item.name;
    detail_desc_ = item.desc;

    can_use_ = canUseItem(item);
    show_use_button_ = can_use_ && !show_targets_;

    setDetailIcon(item.icon_id);

    // Show stats if equipment
    stat_deltas_.clear();
    if (item.category == kCatEquipment) {
        auto addStat = [this](const std::string& label, int val) {
            if (val != 0) {
                stat_deltas_.push_back({
                    label,
                    (val > 0 ? "+" : "") + std::to_string(val),
                    val > 0, val < 0
                });
            }
        };
        addStat("ATK", item.atk);
        addStat("DEF", item.def);
        addStat("SPD", item.spd);
    } else if (item.heal_hp > 0) {
        stat_deltas_.push_back({"HP", "+" + std::to_string(item.heal_hp), true, false});
    } else if (item.heal_mp > 0) {
        stat_deltas_.push_back({"MP", "+" + std::to_string(item.heal_mp), true, false});
    } else if (item.revive_hp > 0) {
        stat_deltas_.push_back({"HP", "+" + std::to_string(item.revive_hp), true, false});
    } else if (item.cure_poison) {
        stat_deltas_.push_back({"Status", "Cleanse", true, false});
    }

    dirtyDetailBindings();
}

void JrpgInventoryScene::onUseItem() {
    if (selected_item_idx_ < 0 || selected_item_idx_ >= static_cast<int>(filtered_items_.size()))
        return;

    if (!can_use_ || selected_item_master_idx_ < 0
        || selected_item_master_idx_ >= static_cast<int>(all_items_.size())) {
        return;
    }

    openTargetPanel();
}

// ── Equipment ─────────────────────────────────────────────

void JrpgInventoryScene::showEquipDetail(int slot_idx) {
    if (slot_idx < 0 || slot_idx >= static_cast<int>(equip_slots_.size())) return;

    selected_slot_idx_ = slot_idx;
    auto& slot = equip_slots_[slot_idx];

    for (int i = 0; i < static_cast<int>(equip_slots_.size()); ++i) {
        equip_slots_[i].is_selected = (i == slot_idx);
    }

    if (slot.equipped_idx >= 0) {
        const auto& eq = all_items_[slot.equipped_idx];
        detail_name_ = eq.name;
        detail_desc_ = eq.desc;
        setDetailIcon(eq.icon_id);
    } else {
        detail_name_ = slot.slot_name;
        detail_desc_ = "(No equipment)";
        setDetailIcon(-1);
    }

    stat_deltas_.clear();
    can_use_ = false;
    show_use_button_ = false;

    model_handle_.DirtyVariable("equip_slots");
    dirtyDetailBindings();
}

void JrpgInventoryScene::openCandidateList(int slot_idx) {
    if (slot_idx < 0 || slot_idx >= static_cast<int>(equip_slots_.size())) return;

    showEquipDetail(slot_idx);

    auto& slot = equip_slots_[slot_idx];
    candidates_.clear();

    // "(Remove)" option
    if (slot.equipped_idx >= 0) {
        auto& cur = all_items_[slot.equipped_idx];
        candidates_.push_back({"(Remove)", -cur.atk, -cur.def, -cur.spd, -1});
    }

    // Find matching equipment in inventory
    for (int i = 0; i < static_cast<int>(all_items_.size()); ++i) {
        auto& item = all_items_[i];
        if (item.category != kCatEquipment) continue;
        if (item.equip_slot != slot.slot_type) continue;
        if (item.qty <= 0) continue;
        // Skip currently equipped item
        if (i == slot.equipped_idx) continue;

        int cur_atk = 0, cur_def = 0, cur_spd = 0;
        if (slot.equipped_idx >= 0) {
            auto& cur = all_items_[slot.equipped_idx];
            cur_atk = cur.atk; cur_def = cur.def; cur_spd = cur.spd;
        }
        candidates_.push_back({
            item.name,
            item.atk - cur_atk,
            item.def - cur_def,
            item.spd - cur_spd,
            i
        });
    }

    show_candidates_ = true;
    model_handle_.DirtyVariable("candidates");
    model_handle_.DirtyVariable("show_candidates");

    focus_candidate_deferred_ = true;
}

void JrpgInventoryScene::closeCandidateList(bool restore_focus) {
    show_candidates_ = false;
    candidates_.clear();
    stat_deltas_.clear();
    selected_cand_idx_ = -1;

    model_handle_.DirtyVariable("show_candidates");
    model_handle_.DirtyVariable("candidates");
    model_handle_.DirtyVariable("stat_deltas");

    if (restore_focus && view_mode_ == 1) {
        focus_slots_deferred_ = true;
    }
}

void JrpgInventoryScene::showCandidatePreview(int cand_idx) {
    if (cand_idx < 0 || cand_idx >= static_cast<int>(candidates_.size())) return;

    selected_cand_idx_ = cand_idx;
    auto& cand = candidates_[cand_idx];

    if (cand.item_index >= 0) {
        const auto& item = all_items_[cand.item_index];
        detail_name_ = item.name;
        detail_desc_ = item.desc;
        setDetailIcon(item.icon_id);
    } else {
        detail_name_ = "(Remove)";
        detail_desc_ = "Unequip current item.";
        setDetailIcon(-1);
    }

    updateStatDeltas(cand.atk_delta, cand.def_delta, cand.spd_delta);
    dirtyDetailBindings();
}

void JrpgInventoryScene::confirmEquip(int cand_idx) {
    if (cand_idx < 0 || cand_idx >= static_cast<int>(candidates_.size())) return;
    if (selected_slot_idx_ < 0) return;

    auto& cand = candidates_[cand_idx];
    auto& slot = equip_slots_[selected_slot_idx_];

    // Apply stat changes
    char_atk_ += cand.atk_delta;
    char_def_ += cand.def_delta;
    char_spd_ += cand.spd_delta;

    if (slot.equipped_idx >= 0) {
        all_items_[slot.equipped_idx].qty++;
    }

    // Update slot
    if (cand.item_index >= 0) {
        all_items_[cand.item_index].qty = std::max(0, all_items_[cand.item_index].qty - 1);
        slot.equipped_name = all_items_[cand.item_index].name;
        slot.equipped_idx  = cand.item_index;
        slot.setIconFlags(all_items_[cand.item_index].icon_id);
        spdlog::info("[Inventory] Equipped {} to {}", cand.name, slot.slot_name);
    } else {
        slot.equipped_name = "(empty)";
        slot.equipped_idx  = -1;
        slot.setIconFlags(-1);
        spdlog::info("[Inventory] Removed equipment from {}", slot.slot_name);
    }

    refreshCharStats();
    filterItems();
    model_handle_.DirtyVariable("equip_slots");
    closeCandidateList(false);
    showEquipDetail(selected_slot_idx_);
    focus_slots_deferred_ = true;
}

void JrpgInventoryScene::openTargetPanel() {
    if (selected_item_master_idx_ < 0 || selected_item_master_idx_ >= static_cast<int>(all_items_.size())) {
        return;
    }

    const auto& item = all_items_[selected_item_master_idx_];
    if (!canUseItem(item)) {
        return;
    }

    selected_target_idx_ = -1;
    target_panel_title_ = "Use " + item.name;
    show_targets_ = true;
    show_use_button_ = false;

    for (auto& member : party_) {
        member.is_selected = false;
        member.disabled = !isTargetValidForItem(item, member);
    }

    model_handle_.DirtyVariable("target_panel_title");
    model_handle_.DirtyVariable("show_targets");
    model_handle_.DirtyVariable("show_use_button");
    refreshPartyBindings();

    const int first_valid_idx = findNextValidTargetIndex(-1, 1);
    if (first_valid_idx >= 0) {
        showTargetPreview(first_valid_idx);
    }
    focus_target_deferred_ = true;
}

void JrpgInventoryScene::closeTargetPanel(bool restore_item_detail, bool restore_focus) {
    show_targets_ = false;
    selected_target_idx_ = -1;
    target_panel_title_.clear();

    for (auto& member : party_) {
        member.is_selected = false;
        member.disabled = false;
    }

    model_handle_.DirtyVariable("show_targets");
    model_handle_.DirtyVariable("target_panel_title");
    refreshPartyBindings();

    if (restore_item_detail && selected_item_idx_ >= 0
        && selected_item_idx_ < static_cast<int>(filtered_items_.size())) {
        showItemDetail(selected_item_idx_);
    }

    if (restore_focus && view_mode_ == 0) {
        focus_grid_deferred_ = true;
    }
}

void JrpgInventoryScene::showTargetPreview(int target_idx) {
    if (target_idx < 0 || target_idx >= static_cast<int>(party_.size())) return;
    if (selected_item_master_idx_ < 0 || selected_item_master_idx_ >= static_cast<int>(all_items_.size())) return;

    selected_target_idx_ = target_idx;
    auto& target = party_[target_idx];
    const auto& item = all_items_[selected_item_master_idx_];

    for (int i = 0; i < static_cast<int>(party_.size()); ++i) {
        party_[i].is_selected = (i == target_idx);
    }

    detail_name_ = std::format("{} -> {}", item.name, target.name);
    detail_desc_ = buildTargetPreviewText(item, target);
    setDetailIcon(item.icon_id);
    updateTargetPreviewDeltas(item, target);

    refreshPartyBindings();
    dirtyDetailBindings();
}

void JrpgInventoryScene::confirmTargetUse(int target_idx) {
    if (target_idx < 0 || target_idx >= static_cast<int>(party_.size())) return;
    if (selected_item_master_idx_ < 0 || selected_item_master_idx_ >= static_cast<int>(all_items_.size())) return;

    auto& item = all_items_[selected_item_master_idx_];
    auto& target = party_[target_idx];
    if (!isTargetValidForItem(item, target) || item.qty <= 0) {
        return;
    }

    const auto previous_hp = target.hp;
    const auto previous_mp = target.mp;
    const auto was_poisoned = target.poisoned;

    if (item.revive_hp > 0 && target.isDown()) {
        target.hp = std::min(target.max_hp, item.revive_hp);
    }
    if (item.heal_hp > 0 && target.isAlive()) {
        target.hp = std::min(target.max_hp, target.hp + item.heal_hp);
    }
    if (item.heal_mp > 0 && target.isAlive()) {
        target.mp = std::min(target.max_mp, target.mp + item.heal_mp);
    }
    if (item.cure_poison && target.isAlive()) {
        target.poisoned = false;
    }
    target.refreshStatus();

    item.qty = std::max(0, item.qty - 1);
    spdlog::info(
        "[Inventory] Used {} on {} (HP {}->{}, MP {}->{}, poison {}->{}, remaining: {})",
        item.name, target.name,
        previous_hp, target.hp,
        previous_mp, target.mp,
        was_poisoned, target.poisoned,
        item.qty);

    closeTargetPanel(false, false);
    refreshPartyBindings();
    filterItems();
    clearDetail();
    focus_grid_deferred_ = true;
}

void JrpgInventoryScene::refreshCharStats() {
    char_atk_text_ = std::to_string(char_atk_);
    char_def_text_ = std::to_string(char_def_);
    char_spd_text_ = std::to_string(char_spd_);

    model_handle_.DirtyVariable("char_atk_text");
    model_handle_.DirtyVariable("char_def_text");
    model_handle_.DirtyVariable("char_spd_text");
}

void JrpgInventoryScene::updateStatDeltas(int atk_d, int def_d, int spd_d) {
    stat_deltas_.clear();

    auto add = [this](const std::string& label, int d) {
        std::string disp;
        if (d > 0) disp = "+" + std::to_string(d);
        else if (d < 0) disp = std::to_string(d);
        else disp = "0";
        stat_deltas_.push_back({label, disp, d > 0, d < 0});
    };

    add("ATK", atk_d);
    add("DEF", def_d);
    add("SPD", spd_d);

    model_handle_.DirtyVariable("stat_deltas");
}

void JrpgInventoryScene::setDetailIcon(int icon_id) {
    det_ic0_ = det_ic1_ = det_ic2_ = det_ic3_ = false;
    det_ic4_ = det_ic5_ = det_ic6_ = det_ic7_ = false;
    show_detail_icon_ = (icon_id >= 0);

    switch (icon_id) {
        case 0: det_ic0_ = true; break;
        case 1: det_ic1_ = true; break;
        case 2: det_ic2_ = true; break;
        case 3: det_ic3_ = true; break;
        case 4: det_ic4_ = true; break;
        case 5: det_ic5_ = true; break;
        case 6: det_ic6_ = true; break;
        case 7: det_ic7_ = true; break;
        default: break;
    }
}

void JrpgInventoryScene::dirtyDetailBindings() {
    model_handle_.DirtyVariable("detail_name");
    model_handle_.DirtyVariable("detail_desc");
    model_handle_.DirtyVariable("can_use");
    model_handle_.DirtyVariable("show_use_button");
    model_handle_.DirtyVariable("show_detail_icon");
    model_handle_.DirtyVariable("stat_deltas");
    model_handle_.DirtyVariable("det_ic0"); model_handle_.DirtyVariable("det_ic1");
    model_handle_.DirtyVariable("det_ic2"); model_handle_.DirtyVariable("det_ic3");
    model_handle_.DirtyVariable("det_ic4"); model_handle_.DirtyVariable("det_ic5");
    model_handle_.DirtyVariable("det_ic6"); model_handle_.DirtyVariable("det_ic7");
}

void JrpgInventoryScene::refreshPartyBindings() {
    model_handle_.DirtyVariable("party");
}

int JrpgInventoryScene::findNextValidTargetIndex(int start_idx, int direction) const {
    if (party_.empty()) {
        return -1;
    }

    const int count = static_cast<int>(party_.size());
    const int step = (direction >= 0) ? 1 : -1;
    int idx = start_idx;
    for (int i = 0; i < count; ++i) {
        idx += step;
        if (idx < 0) {
            idx = count - 1;
        } else if (idx >= count) {
            idx = 0;
        }

        if (!party_[idx].disabled) {
            return idx;
        }
    }

    return -1;
}

int JrpgInventoryScene::getPreferredTargetIndex() const {
    if (selected_target_idx_ >= 0
        && selected_target_idx_ < static_cast<int>(party_.size())
        && !party_[selected_target_idx_].disabled) {
        return selected_target_idx_;
    }

    return findNextValidTargetIndex(-1, 1);
}

bool JrpgInventoryScene::hasFocusedTargetElement() const {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        return false;
    }

    for (auto* element = layer->getFocusedElement(); element != nullptr; element = element->GetParentNode()) {
        if (parseIndexedElementId(element->GetId(), "target-") >= 0) {
            return true;
        }
    }

    return false;
}

void JrpgInventoryScene::focusTargetElement(int target_idx) {
    if (!doc_ || target_idx < 0) {
        return;
    }

    if (auto* element = doc_->GetElementById("target-" + std::to_string(target_idx))) {
        if (auto* layer = context_.getGLRenderer().getRmlUILayer()) {
            (void)layer->focusElement(element);
        }
    }
}

bool JrpgInventoryScene::canUseItem(const ItemData& item) const {
    if (item.category != kCatConsumable || item.qty <= 0) {
        return false;
    }

    return std::any_of(party_.begin(), party_.end(), [&item, this](const PartyMember& target) {
        return isTargetValidForItem(item, target);
    });
}

bool JrpgInventoryScene::isTargetValidForItem(const ItemData& item, const PartyMember& target) const {
    if (item.revive_hp > 0) {
        return target.isDown();
    }

    if (!target.isAlive()) {
        return false;
    }

    if (item.cure_poison && !target.poisoned && item.heal_hp == 0 && item.heal_mp == 0) {
        return false;
    }

    return (item.heal_hp > 0) || (item.heal_mp > 0) || item.cure_poison;
}

std::string JrpgInventoryScene::buildTargetPreviewText(const ItemData& item, const PartyMember& target) const {
    if (item.revive_hp > 0) {
        if (!target.isDown()) {
            return "This item only works on KO allies.";
        }
        const int revived_hp = std::min(target.max_hp, item.revive_hp);
        return std::format("Revive {} with {} HP.", target.name, revived_hp);
    }

    if (!target.isAlive()) {
        return "This item cannot target a KO ally.";
    }

    if (item.heal_hp > 0) {
        const int next_hp = std::min(target.max_hp, target.hp + item.heal_hp);
        return std::format("HP {} / {} -> {} / {}.", target.hp, target.max_hp, next_hp, target.max_hp);
    }

    if (item.heal_mp > 0) {
        const int next_mp = std::min(target.max_mp, target.mp + item.heal_mp);
        return std::format("MP {} / {} -> {} / {}.", target.mp, target.max_mp, next_mp, target.max_mp);
    }

    if (item.cure_poison) {
        return target.poisoned ? "Will cure poison." : "Target is not poisoned.";
    }

    return target.status_text;
}

void JrpgInventoryScene::updateTargetPreviewDeltas(const ItemData& item, const PartyMember& target) {
    stat_deltas_.clear();

    auto addDelta = [this](const std::string& label, const std::string& display, bool positive) {
        stat_deltas_.push_back({label, display, positive, false});
    };

    if (item.revive_hp > 0 && target.isDown()) {
        addDelta("HP", "+" + std::to_string(std::min(target.max_hp, item.revive_hp)), true);
    }
    if (item.heal_hp > 0 && target.isAlive()) {
        const int delta = std::max(0, std::min(target.max_hp, target.hp + item.heal_hp) - target.hp);
        if (delta > 0) {
            addDelta("HP", "+" + std::to_string(delta), true);
        }
    }
    if (item.heal_mp > 0 && target.isAlive()) {
        const int delta = std::max(0, std::min(target.max_mp, target.mp + item.heal_mp) - target.mp);
        if (delta > 0) {
            addDelta("MP", "+" + std::to_string(delta), true);
        }
    }
    if (item.cure_poison && target.isAlive() && target.poisoned) {
        addDelta("Status", "Cleanse", true);
    }
}

// ── Helpers ───────────────────────────────────────────────

void JrpgInventoryScene::addListener(Rml::Element* el, const Rml::String& event, bool capture) {
    if (!el) return;
    el->AddEventListener(event, listener_.get(), capture);
    registrations_.push_back({el, event, capture});
}

} // namespace learn::jrpg
