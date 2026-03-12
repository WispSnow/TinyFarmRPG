#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace learn::jrpg {

class JrpgBattleScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void update(float dt) override;
    void clean() override;

private:
    // --- Bound data types ---

    struct PartyMember {
        std::string name;
        int   hp = 0, max_hp = 0;
        int   mp = 0, max_mp = 0;
        float hp_ratio = 1.0f;
        float mp_ratio = 1.0f;
        bool  hp_low   = false;
        bool  is_active = false;
        int   face_idx  = 0;
        // data-class booleans for portrait (only one true at a time)
        bool face_0 = false, face_1 = false, face_2 = false;
        bool face_3 = false, face_4 = false, face_5 = false;

        void refreshRatios();
        void setFaceFlags();
    };

    struct SkillEntry {
        std::string name;
        int  cost = 0;
        int  power = 0;
        bool disabled = false;
        std::string desc;
    };

    struct ItemEntry {
        std::string name;
        int  qty = 0;
        int  heal = 0;
        bool disabled = false;
        std::string desc;
    };

    // --- Logic ---

    void setupDataModel();
    void buildData();
    void beginPlayerTurn();
    void onCommand(int cmd_idx);
    void openSubMenu(int type); // 0=skills, 1=items
    void closeSubMenu();
    void onSkillSelected(int idx);
    void onItemSelected(int idx);
    void executeAttack();
    void executeSkill(int idx);
    void executeItem(int idx);
    void executeDefend();
    void executeFlee();
    void doEnemyTurn();
    void advanceActiveChar();
    void spawnDamagePopup(int value, bool is_heal);
    void updatePartyBinding();
    void setLog(const std::string& text);
    void addListener(Rml::Element* el, const Rml::String& event, bool capture = false);

    // --- Event listener ---

    class UIEventListener final : public Rml::EventListener {
    public:
        explicit UIEventListener(JrpgBattleScene& s) : scene_(s) {}
        void ProcessEvent(Rml::Event& event) override;

    private:
        JrpgBattleScene& scene_;
    };

    // DOM
    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle  model_handle_{};

    Rml::Element* battle_scene_el_ = nullptr;
    Rml::Element* skills_panel_el_ = nullptr;
    Rml::Element* items_panel_el_  = nullptr;

    std::unique_ptr<UIEventListener> listener_;
    struct ListenerReg { Rml::Element* el; Rml::String event; bool capture; };
    std::vector<ListenerReg> registrations_;

    // Game data
    std::vector<PartyMember> party_;
    std::vector<SkillEntry>  skills_;
    std::vector<ItemEntry>   items_;

    // Bound strings
    std::string turn_text_;
    std::string log_text_;
    std::string skill_desc_;
    std::string item_desc_;

    // State
    int  active_char_idx_ = 0;
    int  sub_menu_type_   = -1; // -1=none, 0=skills, 1=items
    bool enemy_turn_pending_ = false;
    float enemy_turn_timer_  = 0.0f;
    bool focus_cmd_deferred_ = false;
    bool focus_sub_deferred_ = false;

    // Damage popup tracking
    struct DmgPopup { Rml::Element* el; float age; };
    std::vector<DmgPopup> dmg_popups_;
    static constexpr float kDmgPopupDuration = 0.95f;
    static constexpr float kEnemyTurnDelay   = 0.8f;
};

} // namespace learn::jrpg
