#include "jrpg_battle_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>

namespace learn::jrpg {

// ── PartyMember helpers ────────────────────────────────────

void JrpgBattleScene::PartyMember::refreshRatios() {
    hp_ratio = (max_hp > 0) ? static_cast<float>(hp) / static_cast<float>(max_hp) : 0.0f;
    mp_ratio = (max_mp > 0) ? static_cast<float>(mp) / static_cast<float>(max_mp) : 0.0f;
    hp_low   = (hp_ratio < 0.3f);
}

void JrpgBattleScene::PartyMember::setFaceFlags() {
    face_0 = (face_idx == 0);
    face_1 = (face_idx == 1);
    face_2 = (face_idx == 2);
    face_3 = (face_idx == 3);
    face_4 = (face_idx == 4);
    face_5 = (face_idx == 5);
}

// ── UIEventListener ────────────────────────────────────────

void JrpgBattleScene::UIEventListener::ProcessEvent(Rml::Event& event) {
    const auto& type = event.GetType();

    if (type == "keydown") {
        auto key = event.GetParameter<int>("key_identifier", 0);
        if (key == static_cast<int>(Rml::Input::KI_ESCAPE)) {
            if (scene_.sub_menu_type_ >= 0) {
                scene_.closeSubMenu();
                event.StopPropagation();
            }
        }
    }
    else if (type == "focus") {
        // Reserved for future sub-menu description updates
    }
}

// ── Scene lifecycle ────────────────────────────────────────

bool JrpgBattleScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    buildData();
    setupDataModel();

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_jrpg_battle.rml");
    if (!doc_) {
        spdlog::error("JrpgBattleScene: failed to load RML");
        return false;
    }

    battle_scene_el_ = doc_->GetElementById("battle-scene");
    skills_panel_el_ = doc_->GetElementById("skills-panel");
    items_panel_el_  = doc_->GetElementById("items-panel");

    listener_ = std::make_unique<UIEventListener>(*this);
    addListener(doc_, "keydown");

    beginPlayerTurn();

    spdlog::info("JrpgBattleScene initialized");
    return true;
}

void JrpgBattleScene::update(float dt) {
    Scene::update(dt);

    // Deferred focus for command menu
    if (focus_cmd_deferred_) {
        if (auto* el = doc_->GetElementById("cmd-0")) {
            el->Focus(true);
            focus_cmd_deferred_ = false;
        }
    }

    // Deferred focus for sub-menu
    if (focus_sub_deferred_) {
        const char* list_id = (sub_menu_type_ == 0) ? "skills-list" : "items-list";
        if (auto* list = doc_->GetElementById(list_id)) {
            if (list->GetNumChildren() > 0) {
                // Find first non-disabled item
                for (int i = 0; i < list->GetNumChildren(); ++i) {
                    auto* child = list->GetChild(i);
                    if (!child->IsClassSet("disabled")) {
                        child->Focus(true);
                        break;
                    }
                }
                focus_sub_deferred_ = false;
            }
        }
    }

    // Damage popup cleanup
    for (auto it = dmg_popups_.begin(); it != dmg_popups_.end();) {
        it->age += dt;
        if (it->age >= kDmgPopupDuration) {
            if (it->el && it->el->GetParentNode()) {
                it->el->GetParentNode()->RemoveChild(it->el);
            }
            it = dmg_popups_.erase(it);
        } else {
            ++it;
        }
    }

    // Enemy turn timer
    if (enemy_turn_pending_) {
        enemy_turn_timer_ -= dt;
        if (enemy_turn_timer_ <= 0.0f) {
            enemy_turn_pending_ = false;
            doEnemyTurn();
        }
    }
}

void JrpgBattleScene::clean() {
    for (auto& [el, event, capture] : registrations_) {
        el->RemoveEventListener(event, listener_.get(), capture);
    }
    registrations_.clear();
    listener_.reset();

    // Clear popup references before document unload
    dmg_popups_.clear();

    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext()) {
        rml_ctx->RemoveDataModel("battle");
    }

    Scene::clean();
}

// ── Data model ─────────────────────────────────────────────

void JrpgBattleScene::setupDataModel() {
    auto* rml_ctx = context_.getGLRenderer().getRmlUILayer()->getContext();
    if (!rml_ctx) return;

    auto ctor = rml_ctx->CreateDataModel("battle");
    if (!ctor) return;

    // PartyMember struct
    if (auto sh = ctor.RegisterStruct<PartyMember>()) {
        sh.RegisterMember("name",      &PartyMember::name);
        sh.RegisterMember("hp",        &PartyMember::hp);
        sh.RegisterMember("max_hp",    &PartyMember::max_hp);
        sh.RegisterMember("mp",        &PartyMember::mp);
        sh.RegisterMember("max_mp",    &PartyMember::max_mp);
        sh.RegisterMember("hp_ratio",  &PartyMember::hp_ratio);
        sh.RegisterMember("mp_ratio",  &PartyMember::mp_ratio);
        sh.RegisterMember("hp_low",    &PartyMember::hp_low);
        sh.RegisterMember("is_active", &PartyMember::is_active);
        sh.RegisterMember("face_0",    &PartyMember::face_0);
        sh.RegisterMember("face_1",    &PartyMember::face_1);
        sh.RegisterMember("face_2",    &PartyMember::face_2);
        sh.RegisterMember("face_3",    &PartyMember::face_3);
        sh.RegisterMember("face_4",    &PartyMember::face_4);
        sh.RegisterMember("face_5",    &PartyMember::face_5);
    }
    ctor.RegisterArray<std::vector<PartyMember>>();

    // SkillEntry struct
    if (auto sh = ctor.RegisterStruct<SkillEntry>()) {
        sh.RegisterMember("name",     &SkillEntry::name);
        sh.RegisterMember("cost",     &SkillEntry::cost);
        sh.RegisterMember("disabled", &SkillEntry::disabled);
    }
    ctor.RegisterArray<std::vector<SkillEntry>>();

    // ItemEntry struct
    if (auto sh = ctor.RegisterStruct<ItemEntry>()) {
        sh.RegisterMember("name",     &ItemEntry::name);
        sh.RegisterMember("qty",      &ItemEntry::qty);
        sh.RegisterMember("disabled", &ItemEntry::disabled);
    }
    ctor.RegisterArray<std::vector<ItemEntry>>();

    // Scalars
    ctor.Bind("party",      &party_);
    ctor.Bind("skills",     &skills_);
    ctor.Bind("items",      &items_);
    ctor.Bind("turn_text",  &turn_text_);
    ctor.Bind("log_text",   &log_text_);
    ctor.Bind("skill_desc", &skill_desc_);
    ctor.Bind("item_desc",  &item_desc_);

    // Command callback
    ctor.BindEventCallback("on_cmd",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (args.empty()) return;
            onCommand(args[0].Get<int>(-1));
        });

    // Skill callback
    ctor.BindEventCallback("on_skill",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (args.empty()) return;
            onSkillSelected(args[0].Get<int>(-1));
        });

    // Item callback
    ctor.BindEventCallback("on_item",
        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
            if (args.empty()) return;
            onItemSelected(args[0].Get<int>(-1));
        });

    model_handle_ = ctor.GetModelHandle();
}

// ── Build game data ────────────────────────────────────────

void JrpgBattleScene::buildData() {
    party_ = {
        {"Aelindra", 320, 320, 80, 80, 1.0f, 1.0f, false, false, 0},
        {"Bjorn",    450, 450, 30, 30, 1.0f, 1.0f, false, false, 2},
        {"Ciel",     200, 200, 120, 120, 1.0f, 1.0f, false, false, 4},
        {"Dusk",     280, 280, 60, 60, 1.0f, 1.0f, false, false, 1},
    };
    for (auto& m : party_) {
        m.refreshRatios();
        m.setFaceFlags();
    }

    skills_ = {
        {"Fire",      12, 45, false, "Deals fire damage to one enemy."},
        {"Ice",       12, 40, false, "Deals ice damage to one enemy."},
        {"Thunder",   18, 65, false, "Deals lightning damage to one enemy."},
        {"Heal",      8,  0,  false, "Restores 80 HP to one ally."},
        {"Meteor",    50, 120, false, "Massive damage to all enemies."},
    };

    items_ = {
        {"Potion",     5, 50, false, "Restores 50 HP."},
        {"Hi-Potion",  2, 120, false, "Restores 120 HP."},
        {"Ether",      3, 0,  false, "Restores 30 MP."},
        {"Phoenix Down",1, 0,  false, "Revives a fallen ally."},
        {"Bomb",       0, 80, true,  "Deals 80 fire damage. (out of stock)"},
    };
}

// ── Turn management ────────────────────────────────────────

void JrpgBattleScene::beginPlayerTurn() {
    // Mark active character
    for (int i = 0; i < static_cast<int>(party_.size()); ++i) {
        party_[i].is_active = (i == active_char_idx_);
    }

    // Update skill disabled state based on current character's MP
    auto& active = party_[active_char_idx_];
    for (auto& s : skills_) {
        s.disabled = (active.mp < s.cost);
    }

    turn_text_ = active.name + "'s Turn";
    log_text_  = "Choose a command.";
    closeSubMenu();
    updatePartyBinding();
    model_handle_.DirtyVariable("skills");
    model_handle_.DirtyVariable("turn_text");
    model_handle_.DirtyVariable("log_text");

    focus_cmd_deferred_ = true;
}

void JrpgBattleScene::advanceActiveChar() {
    active_char_idx_ = (active_char_idx_ + 1) % static_cast<int>(party_.size());

    // Trigger enemy turn every full round
    if (active_char_idx_ == 0) {
        enemy_turn_pending_ = true;
        enemy_turn_timer_   = kEnemyTurnDelay;
        turn_text_ = "Enemy Turn";
        model_handle_.DirtyVariable("turn_text");
    } else {
        beginPlayerTurn();
    }
}

// ── Commands ───────────────────────────────────────────────

void JrpgBattleScene::onCommand(int idx) {
    if (enemy_turn_pending_) return;

    switch (idx) {
        case 0: executeAttack(); break;
        case 1: openSubMenu(0); break;
        case 2: openSubMenu(1); break;
        case 3: executeDefend(); break;
        case 4: executeFlee(); break;
        default: break;
    }
}

void JrpgBattleScene::openSubMenu(int type) {
    sub_menu_type_ = type;
    if (skills_panel_el_) skills_panel_el_->SetClass("hidden", type != 0);
    if (items_panel_el_)  items_panel_el_->SetClass("hidden",  type != 1);

    skill_desc_.clear();
    item_desc_.clear();
    model_handle_.DirtyVariable("skill_desc");
    model_handle_.DirtyVariable("item_desc");

    focus_sub_deferred_ = true;
}

void JrpgBattleScene::closeSubMenu() {
    sub_menu_type_ = -1;
    if (skills_panel_el_) skills_panel_el_->SetClass("hidden", true);
    if (items_panel_el_)  items_panel_el_->SetClass("hidden", true);
}

// ── Action execution ───────────────────────────────────────

void JrpgBattleScene::executeAttack() {
    int dmg = 30 + std::rand() % 20;
    setLog(party_[active_char_idx_].name + " attacks! " + std::to_string(dmg) + " damage!");
    spawnDamagePopup(dmg, false);
    advanceActiveChar();
}

void JrpgBattleScene::onSkillSelected(int idx) {
    if (idx < 0 || idx >= static_cast<int>(skills_.size())) return;
    if (skills_[idx].disabled) return;
    executeSkill(idx);
}

void JrpgBattleScene::executeSkill(int idx) {
    auto& skill  = skills_[idx];
    auto& active = party_[active_char_idx_];

    active.mp = std::max(0, active.mp - skill.cost);
    active.refreshRatios();

    if (skill.power > 0) {
        int dmg = skill.power + std::rand() % 15;
        setLog(active.name + " casts " + skill.name + "! " + std::to_string(dmg) + " damage!");
        spawnDamagePopup(dmg, false);
    } else {
        // Heal skill
        int heal_amt = 80;
        active.hp = std::min(active.max_hp, active.hp + heal_amt);
        active.refreshRatios();
        setLog(active.name + " casts " + skill.name + "! Recovered " + std::to_string(heal_amt) + " HP!");
        spawnDamagePopup(heal_amt, true);
    }

    closeSubMenu();
    updatePartyBinding();
    advanceActiveChar();
}

void JrpgBattleScene::onItemSelected(int idx) {
    if (idx < 0 || idx >= static_cast<int>(items_.size())) return;
    if (items_[idx].disabled) return;
    executeItem(idx);
}

void JrpgBattleScene::executeItem(int idx) {
    auto& item   = items_[idx];
    auto& active = party_[active_char_idx_];

    item.qty--;
    if (item.qty <= 0) item.disabled = true;

    if (item.heal > 0) {
        active.hp = std::min(active.max_hp, active.hp + item.heal);
        active.refreshRatios();
        setLog(active.name + " uses " + item.name + "! Recovered " + std::to_string(item.heal) + " HP!");
        spawnDamagePopup(item.heal, true);
    } else {
        setLog(active.name + " uses " + item.name + "!");
    }

    closeSubMenu();
    updatePartyBinding();
    model_handle_.DirtyVariable("items");
    advanceActiveChar();
}

void JrpgBattleScene::executeDefend() {
    setLog(party_[active_char_idx_].name + " defends.");
    advanceActiveChar();
}

void JrpgBattleScene::executeFlee() {
    if (std::rand() % 3 == 0) {
        setLog("Got away safely!");
    } else {
        setLog("Couldn't escape!");
        advanceActiveChar();
    }
}

// ── Enemy turn ─────────────────────────────────────────────

void JrpgBattleScene::doEnemyTurn() {
    // Random target
    int target = std::rand() % static_cast<int>(party_.size());
    int dmg = 20 + std::rand() % 30;

    party_[target].hp = std::max(0, party_[target].hp - dmg);
    party_[target].refreshRatios();

    setLog("Enemy attacks " + party_[target].name + "! " + std::to_string(dmg) + " damage!");
    spawnDamagePopup(dmg, false);
    updatePartyBinding();

    beginPlayerTurn();
}

// ── Damage popups ──────────────────────────────────────────

void JrpgBattleScene::spawnDamagePopup(int value, bool is_heal) {
    if (!battle_scene_el_) return;

    auto el_ptr = doc_->CreateElement("div");
    auto* el = el_ptr.get();
    el->SetClassNames(is_heal ? "dmg-popup heal dmg-animate" : "dmg-popup dmg-animate");
    el->SetInnerRML(std::to_string(value));

    // Random position within the battle scene
    int x = 80 + std::rand() % 200;
    int y = 30 + std::rand() % 60;
    el->SetProperty("left", std::to_string(x) + "dp");
    el->SetProperty("top",  std::to_string(y) + "dp");

    battle_scene_el_->AppendChild(std::move(el_ptr));
    dmg_popups_.push_back({el, 0.0f});
}

// ── Helpers ────────────────────────────────────────────────

void JrpgBattleScene::updatePartyBinding() {
    model_handle_.DirtyVariable("party");
}

void JrpgBattleScene::setLog(const std::string& text) {
    log_text_ = text;
    model_handle_.DirtyVariable("log_text");
    spdlog::info("[Battle] {}", text);
}

void JrpgBattleScene::addListener(Rml::Element* el, const Rml::String& event, bool capture) {
    if (!el) return;
    el->AddEventListener(event, listener_.get(), capture);
    registrations_.push_back({el, event, capture});
}

} // namespace learn::jrpg
