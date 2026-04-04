#pragma once

#include "engine/scene/scene.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/EventListener.h>

#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace learn::jrpg {

class JrpgInventoryScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void update(float dt) override;
    void clean() override;

private:
    // --- Bound data types ---

    static constexpr int kTabAll = -1;

    // Item categories
    static constexpr int kCatConsumable = 0;
    static constexpr int kCatEquipment  = 1;
    static constexpr int kCatKeyItem    = 2;

    // Equipment slot types
    static constexpr int kSlotWeapon    = 0;
    static constexpr int kSlotShield    = 1;
    static constexpr int kSlotHelmet    = 2;
    static constexpr int kSlotArmor     = 3;
    static constexpr int kSlotAccessory = 4;

    struct ItemData {
        std::string name;
        std::string desc;
        int  category    = 0;   // 0=consumable,1=equipment,2=key item
        int  qty         = 1;
        int  icon_id     = 0;
        int  source_index = -1; // index in all_items_
        int  equip_slot  = -1;  // which slot type this equips to (-1=N/A)
        int  atk = 0, def = 0, spd = 0;
        int  heal_hp = 0;       // HP restored (consumables)
        int  heal_mp = 0;       // MP restored (consumables)
        int  revive_hp = 0;     // HP restored when reviving a KO target
        bool cure_poison = false;

        // icon booleans for data-class binding
        bool ic0 = false, ic1 = false, ic2 = false, ic3 = false;
        bool ic4 = false, ic5 = false, ic6 = false, ic7 = false;
        void setIconFlags();
    };

    struct EquipSlot {
        std::string slot_name;
        std::string equipped_name;
        int  slot_type    = 0;
        bool is_selected  = false;
        int  equipped_idx = -1;  // index in all_items_, -1 = empty

        // icon booleans
        bool ic0 = false, ic1 = false, ic2 = false, ic3 = false;
        bool ic4 = false, ic5 = false, ic6 = false, ic7 = false;
        void setIconFlags(int icon_id);
    };

    struct EquipCandidate {
        std::string name;
        int  atk_delta = 0;
        int  def_delta = 0;
        int  spd_delta = 0;
        int  item_index = -1;  // -1 = "(Remove)"
    };

    struct StatDelta {
        std::string label;
        std::string display;
        bool is_positive = false;
        bool is_negative = false;
    };

    struct PartyMember {
        std::string name;
        int  hp = 0;
        int  max_hp = 0;
        int  mp = 0;
        int  max_mp = 0;
        bool poisoned = false;
        bool disabled = false;
        bool is_selected = false;
        std::string status_text;
        std::string target_summary;

        [[nodiscard]] bool isDown() const { return hp <= 0; }
        [[nodiscard]] bool isAlive() const { return hp > 0; }
        void refreshStatus();
    };

    // --- Logic ---

    void buildData();
    void setupDataModel();
    void filterItems();
    void switchView(int mode);
    void switchTab(int tab);
    void clearDetail();
    void showItemDetail(int filtered_idx);
    void onUseItem();
    void openCandidateList(int slot_idx);
    void closeCandidateList(bool restore_focus = true);
    void showEquipDetail(int slot_idx);
    void showCandidatePreview(int cand_idx);
    void confirmEquip(int cand_idx);
    void openTargetPanel();
    void closeTargetPanel(bool restore_item_detail = true, bool restore_focus = true);
    void showTargetPreview(int target_idx);
    void confirmTargetUse(int target_idx);
    void refreshCharStats();
    void updateStatDeltas(int atk_d, int def_d, int spd_d);
    void setDetailIcon(int icon_id);
    void dirtyDetailBindings();
    void refreshPartyBindings();
    [[nodiscard]] bool canUseItem(const ItemData& item) const;
    [[nodiscard]] bool isTargetValidForItem(const ItemData& item, const PartyMember& target) const;
    [[nodiscard]] std::string buildTargetPreviewText(const ItemData& item, const PartyMember& target) const;
    void updateTargetPreviewDeltas(const ItemData& item, const PartyMember& target);
    [[nodiscard]] int findNextValidTargetIndex(int start_idx, int direction) const;
    [[nodiscard]] int getPreferredTargetIndex() const;
    [[nodiscard]] bool hasFocusedTargetElement() const;
    void focusTargetElement(int target_idx);
    void addListener(Rml::Element* el, const Rml::String& event, bool capture = false);

    // --- Event listener ---

    class UIEventListener final : public Rml::EventListener {
    public:
        explicit UIEventListener(JrpgInventoryScene& s) : scene_(s) {}
        void ProcessEvent(Rml::Event& event) override;

    private:
        JrpgInventoryScene& scene_;
    };

    // DOM
    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle  model_handle_{};

    std::unique_ptr<UIEventListener> listener_;
    std::unique_ptr<Rml::EventListener> hover_focus_listener_{};
    struct ListenerReg { Rml::Element* el; Rml::String event; bool capture; };
    std::vector<ListenerReg> registrations_;
    bool hover_listener_registered_ = false;

    // All items (master list)
    std::vector<ItemData> all_items_;

    // Filtered view for current tab
    std::vector<ItemData> filtered_items_;

    // Equipment
    std::vector<EquipSlot>      equip_slots_;
    std::vector<EquipCandidate> candidates_;
    std::vector<StatDelta>      stat_deltas_;
    std::vector<PartyMember>    party_;

    // Bound scalars
    int view_mode_   = 0;  // 0=items, 1=equip
    int current_tab_ = kTabAll;
    bool is_items_view_ = true;
    bool is_equip_view_ = false;
    bool tab_all_active_ = true;
    bool tab_consumable_active_ = false;
    bool tab_equipment_active_ = false;
    bool tab_key_active_ = false;
    bool can_use_     = false;
    bool show_use_button_ = false;
    bool show_candidates_ = false;
    bool show_targets_ = false;

    std::string detail_name_;
    std::string detail_desc_;
    std::string gold_text_;
    std::string target_panel_title_;

    // Character stats
    int char_atk_ = 10, char_def_ = 8, char_spd_ = 12;
    std::string char_atk_text_, char_def_text_, char_spd_text_;

    // Detail icon booleans
    bool det_ic0_ = false, det_ic1_ = false, det_ic2_ = false, det_ic3_ = false;
    bool det_ic4_ = false, det_ic5_ = false, det_ic6_ = false, det_ic7_ = false;
    bool show_detail_icon_ = false;

    // State
    int  selected_item_idx_ = -1;   // index in filtered_items_
    int  selected_item_master_idx_ = -1;
    int  selected_slot_idx_ = -1;
    int  selected_cand_idx_ = -1;
    int  selected_target_idx_ = -1;
    int  gold_ = 1250;

    // Deferred focus
    bool focus_nav_deferred_       = false;
    bool focus_grid_deferred_      = false;
    bool focus_slots_deferred_     = false;
    bool focus_candidate_deferred_ = false;
    bool focus_target_deferred_    = false;
};

} // namespace learn::jrpg
