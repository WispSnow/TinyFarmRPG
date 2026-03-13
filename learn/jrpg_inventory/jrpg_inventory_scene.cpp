#include "jrpg_inventory_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <format>

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

// ── UIEventListener ───────────────────────────────────────

void JrpgInventoryScene::UIEventListener::ProcessEvent(Rml::Event& event) {
    if (event.GetType() != "keydown") return;

    auto key = event.GetParameter<int>("key_identifier", 0);

    if (key == static_cast<int>(Rml::Input::KI_ESCAPE)) {
        // Close candidate list if open
        if (scene_.show_candidates_) {
            scene_.closeCandidateList();
            event.StopPropagation();
        }
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
}

void JrpgInventoryScene::clean() {
    for (auto& [el, event, capture] : registrations_) {
        el->RemoveEventListener(event, listener_.get(), capture);
    }
    registrations_.clear();
    listener_.reset();

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

    // Bind arrays
    ctor.Bind("filtered_items", &filtered_items_);
    ctor.Bind("equip_slots",    &equip_slots_);
    ctor.Bind("candidates",     &candidates_);
    ctor.Bind("stat_deltas",    &stat_deltas_);

    // Bind scalars
    ctor.Bind("view_mode",       &view_mode_);
    ctor.Bind("is_items_view",   &is_items_view_);
    ctor.Bind("is_equip_view",   &is_equip_view_);
    ctor.Bind("tab0_active",     &tab0_active_);
    ctor.Bind("tab1_active",     &tab1_active_);
    ctor.Bind("tab2_active",     &tab2_active_);
    ctor.Bind("can_use",         &can_use_);
    ctor.Bind("show_candidates", &show_candidates_);
    ctor.Bind("detail_name",     &detail_name_);
    ctor.Bind("detail_desc",     &detail_desc_);
    ctor.Bind("gold_text",       &gold_text_);
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

    ctor.BindEventCallback("on_candidate_select",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) confirmEquip(args[0].Get<int>(-1));
        });

    ctor.BindEventCallback("on_candidate_focus",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (!args.empty()) showCandidatePreview(args[0].Get<int>(-1));
        });

    model_handle_ = ctor.GetModelHandle();
}

// ── Build game data ───────────────────────────────────────

void JrpgInventoryScene::buildData() {
    // icon_id mapping:
    // 0=potion, 1=hi-potion, 2=ether, 3=antidote
    // 4=sword, 5=shield, 6=helmet, 7=armor/ring

    all_items_ = {
        // Consumables (category=0)
        {"Potion",       "Restores 50 HP.",              kCatConsumable, 5,  0, -1, 0,0,0, 50},
        {"Hi-Potion",    "Restores 120 HP.",             kCatConsumable, 2,  1, -1, 0,0,0, 120},
        {"Ether",        "Restores 30 MP.",              kCatConsumable, 3,  2, -1, 0,0,0, 0},
        {"Antidote",     "Cures poison.",                kCatConsumable, 4,  3, -1, 0,0,0, 0},
        {"Phoenix Down", "Revives a fallen ally.",       kCatConsumable, 1,  0, -1, 0,0,0, 0},
        // Equipment (category=1)
        {"Iron Sword",   "A sturdy iron sword.",         kCatEquipment, 1,  4, kSlotWeapon,    12, 0, 0, 0},
        {"Steel Sword",  "Forged from fine steel.",      kCatEquipment, 1,  4, kSlotWeapon,    18, 0, 2, 0},
        {"Bronze Shield","A basic bronze shield.",       kCatEquipment, 1,  5, kSlotShield,    0, 8,  0, 0},
        {"Iron Shield",  "Solid iron protection.",       kCatEquipment, 1,  5, kSlotShield,    0, 14, 0, 0},
        {"Leather Cap",  "Light leather headgear.",      kCatEquipment, 1,  6, kSlotHelmet,    0, 3,  1, 0},
        {"Iron Helm",    "Heavy iron helmet.",           kCatEquipment, 1,  6, kSlotHelmet,    0, 7,  -1, 0},
        {"Chain Mail",   "Interlocked chain armor.",     kCatEquipment, 1,  7, kSlotArmor,     0, 12, -2, 0},
        {"Plate Armor",  "Heavy plate protection.",      kCatEquipment, 1,  7, kSlotArmor,     2, 20, -4, 0},
        {"Speed Ring",   "Boosts agility.",              kCatEquipment, 1,  7, kSlotAccessory, 0, 0,  5, 0},
        {"Power Ring",   "Boosts attack power.",         kCatEquipment, 1,  7, kSlotAccessory, 5, 0,  0, 0},
        // Key Items (category=2)
        {"Old Map",      "A weathered treasure map.",    kCatKeyItem, 1, 3, -1, 0,0,0, 0},
        {"Royal Letter", "Sealed letter from the king.", kCatKeyItem, 1, 3, -1, 0,0,0, 0},
    };

    for (auto& item : all_items_) item.setIconFlags();

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
        } else {
            slot.setIconFlags(-1); // all false
        }
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
    clearDetail();
    closeCandidateList();

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
    tab0_active_ = (tab == 0);
    tab1_active_ = (tab == 1);
    tab2_active_ = (tab == 2);
    clearDetail();
    filterItems();

    model_handle_.DirtyVariable("tab0_active");
    model_handle_.DirtyVariable("tab1_active");
    model_handle_.DirtyVariable("tab2_active");

    focus_grid_deferred_ = true;
}

void JrpgInventoryScene::filterItems() {
    filtered_items_.clear();
    for (auto& item : all_items_) {
        if (item.category == current_tab_ && item.qty > 0) {
            filtered_items_.push_back(item);
        }
    }
    model_handle_.DirtyVariable("filtered_items");
}

// ── Item detail ───────────────────────────────────────────

void JrpgInventoryScene::clearDetail() {
    selected_item_idx_ = -1;
    detail_name_.clear();
    detail_desc_.clear();
    can_use_ = false;
    show_detail_icon_ = false;
    stat_deltas_.clear();
    det_ic0_ = det_ic1_ = det_ic2_ = det_ic3_ = false;
    det_ic4_ = det_ic5_ = det_ic6_ = det_ic7_ = false;

    model_handle_.DirtyVariable("detail_name");
    model_handle_.DirtyVariable("detail_desc");
    model_handle_.DirtyVariable("can_use");
    model_handle_.DirtyVariable("show_detail_icon");
    model_handle_.DirtyVariable("stat_deltas");
    model_handle_.DirtyVariable("det_ic0"); model_handle_.DirtyVariable("det_ic1");
    model_handle_.DirtyVariable("det_ic2"); model_handle_.DirtyVariable("det_ic3");
    model_handle_.DirtyVariable("det_ic4"); model_handle_.DirtyVariable("det_ic5");
    model_handle_.DirtyVariable("det_ic6"); model_handle_.DirtyVariable("det_ic7");
}

void JrpgInventoryScene::showItemDetail(int filtered_idx) {
    if (filtered_idx < 0 || filtered_idx >= static_cast<int>(filtered_items_.size())) return;

    selected_item_idx_ = filtered_idx;
    auto& item = filtered_items_[filtered_idx];
    detail_name_ = item.name;
    detail_desc_ = item.desc;

    // Show use button only for consumables
    can_use_ = (item.category == kCatConsumable && item.qty > 0);

    // Detail icon
    show_detail_icon_ = true;
    det_ic0_ = item.ic0; det_ic1_ = item.ic1;
    det_ic2_ = item.ic2; det_ic3_ = item.ic3;
    det_ic4_ = item.ic4; det_ic5_ = item.ic5;
    det_ic6_ = item.ic6; det_ic7_ = item.ic7;

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
    }

    model_handle_.DirtyVariable("detail_name");
    model_handle_.DirtyVariable("detail_desc");
    model_handle_.DirtyVariable("can_use");
    model_handle_.DirtyVariable("show_detail_icon");
    model_handle_.DirtyVariable("stat_deltas");
    model_handle_.DirtyVariable("det_ic0"); model_handle_.DirtyVariable("det_ic1");
    model_handle_.DirtyVariable("det_ic2"); model_handle_.DirtyVariable("det_ic3");
    model_handle_.DirtyVariable("det_ic4"); model_handle_.DirtyVariable("det_ic5");
    model_handle_.DirtyVariable("det_ic6"); model_handle_.DirtyVariable("det_ic7");
}

void JrpgInventoryScene::onUseItem() {
    if (selected_item_idx_ < 0 || selected_item_idx_ >= static_cast<int>(filtered_items_.size()))
        return;

    auto& fitem = filtered_items_[selected_item_idx_];

    // Find in all_items_ and decrement
    for (auto& item : all_items_) {
        if (item.name == fitem.name && item.category == fitem.category) {
            item.qty--;
            spdlog::info("[Inventory] Used {} (remaining: {})", item.name, item.qty);
            break;
        }
    }

    // Refresh
    clearDetail();
    filterItems();
    focus_grid_deferred_ = true;
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
        auto& eq = all_items_[slot.equipped_idx];
        detail_name_ = eq.name;
        detail_desc_ = eq.desc;
        show_detail_icon_ = true;
        det_ic0_ = eq.ic0; det_ic1_ = eq.ic1;
        det_ic2_ = eq.ic2; det_ic3_ = eq.ic3;
        det_ic4_ = eq.ic4; det_ic5_ = eq.ic5;
        det_ic6_ = eq.ic6; det_ic7_ = eq.ic7;
    } else {
        detail_name_ = slot.slot_name;
        detail_desc_ = "(No equipment)";
        show_detail_icon_ = false;
    }

    stat_deltas_.clear();
    can_use_ = false;

    model_handle_.DirtyVariable("equip_slots");
    model_handle_.DirtyVariable("detail_name");
    model_handle_.DirtyVariable("detail_desc");
    model_handle_.DirtyVariable("can_use");
    model_handle_.DirtyVariable("show_detail_icon");
    model_handle_.DirtyVariable("stat_deltas");
    model_handle_.DirtyVariable("det_ic0"); model_handle_.DirtyVariable("det_ic1");
    model_handle_.DirtyVariable("det_ic2"); model_handle_.DirtyVariable("det_ic3");
    model_handle_.DirtyVariable("det_ic4"); model_handle_.DirtyVariable("det_ic5");
    model_handle_.DirtyVariable("det_ic6"); model_handle_.DirtyVariable("det_ic7");
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

void JrpgInventoryScene::closeCandidateList() {
    show_candidates_ = false;
    candidates_.clear();
    stat_deltas_.clear();
    selected_cand_idx_ = -1;

    model_handle_.DirtyVariable("show_candidates");
    model_handle_.DirtyVariable("candidates");
    model_handle_.DirtyVariable("stat_deltas");

    if (view_mode_ == 1) {
        focus_slots_deferred_ = true;
    }
}

void JrpgInventoryScene::showCandidatePreview(int cand_idx) {
    if (cand_idx < 0 || cand_idx >= static_cast<int>(candidates_.size())) return;

    selected_cand_idx_ = cand_idx;
    auto& cand = candidates_[cand_idx];

    if (cand.item_index >= 0) {
        auto& item = all_items_[cand.item_index];
        detail_name_ = item.name;
        detail_desc_ = item.desc;
    } else {
        detail_name_ = "(Remove)";
        detail_desc_ = "Unequip current item.";
    }

    updateStatDeltas(cand.atk_delta, cand.def_delta, cand.spd_delta);

    model_handle_.DirtyVariable("detail_name");
    model_handle_.DirtyVariable("detail_desc");
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

    // Update slot
    if (cand.item_index >= 0) {
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
    model_handle_.DirtyVariable("equip_slots");
    closeCandidateList();
    showEquipDetail(selected_slot_idx_);
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

// ── Helpers ───────────────────────────────────────────────

void JrpgInventoryScene::addListener(Rml::Element* el, const Rml::String& event, bool capture) {
    if (!el) return;
    el->AddEventListener(event, listener_.get(), capture);
    registrations_.push_back({el, event, capture});
}

} // namespace learn::jrpg
