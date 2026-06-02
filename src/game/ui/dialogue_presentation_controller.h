#pragma once

#include "game/defs/events.h"
#include "game/ui/hud_view_ports.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>
#include <glm/vec2.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::data {
class RpgCatalog;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::ui {

class PlayerPortraitService;

class DialoguePresentationController final {
public:
    DialoguePresentationController(entt::dispatcher& dispatcher,
                                   entt::registry& registry,
                                   DialogueBoxViewPort* conversation_box,
                                   FloatingNoticeViewPort* notice_view,
                                   FloatingNoticeViewPort* item_notice_view,
                                   HotbarVisibilityPort* hotbar_ui,
                                   const game::data::RpgCatalog* rpg_catalog,
                                   glm::vec2 notice_offset = {0.0F, -4.0F},
                                   glm::vec2 item_notice_offset = {0.0F, -56.0F},
                                   const PlayerPortraitService* player_portrait_service = nullptr,
                                   const game::runtime::LocalizationService* localization = nullptr);
    ~DialoguePresentationController();

    void update(float delta_time);

private:
    struct NoticeSlot {
        FloatingNoticeViewPort* view{nullptr};
        glm::vec2 screen_offset{0.0F, -4.0F};
        entt::entity target{entt::null};
        float remaining_seconds{0.0F};
    };

    entt::dispatcher& dispatcher_;
    entt::registry& registry_;
    DialogueBoxViewPort* conversation_box_{nullptr};
    HotbarVisibilityPort* hotbar_ui_{nullptr};
    NoticeSlot notice_slot_{};
    NoticeSlot item_notice_slot_{};
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    const PlayerPortraitService* player_portrait_service_{nullptr};
    const game::runtime::LocalizationService* localization_{nullptr};
    std::unordered_map<entt::id_type, std::string> portrait_by_actor_id_hash_{};
    std::unordered_map<entt::id_type, std::string> portrait_by_map_actor_id_hash_{};
    std::unordered_map<entt::id_type, std::string> display_name_key_by_map_actor_id_hash_{};
    bool conversation_active_{false};
    bool restore_hotbar_after_conversation_{false};

    void onShow(const game::defs::DialogueShowEvent& evt);
    void onMove(const game::defs::DialogueMoveEvent& evt);
    void onHide(const game::defs::DialogueHideEvent& evt);

    void hideHotbarForConversation();
    void restoreHotbarAfterConversation();
    void updateNoticeSlot(NoticeSlot& slot, float delta_time);
    void buildActorCaches();
    [[nodiscard]] glm::vec2 noticeAnchor(entt::entity target, glm::vec2 fallback_position) const;
    [[nodiscard]] std::string resolveSpeakerName(const game::defs::DialogueShowEvent& evt) const;
    [[nodiscard]] std::string resolveActorNameKey(std::string_view actor_id) const;
    [[nodiscard]] std::string resolveCatalogActorName(entt::id_type actor_id_hash) const;
    [[nodiscard]] std::string resolvePortraitDecorator(const game::defs::DialogueShowEvent& evt) const;
    [[nodiscard]] const NoticeSlot* noticeSlot(game::defs::DialogueChannel channel) const;
    [[nodiscard]] NoticeSlot* noticeSlot(game::defs::DialogueChannel channel);
    [[nodiscard]] static std::string formatNoticeText(std::string_view speaker, std::string_view text);
};

} // namespace game::ui
