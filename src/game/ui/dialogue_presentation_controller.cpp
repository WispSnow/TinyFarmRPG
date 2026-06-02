#include "dialogue_presentation_controller.h"

#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/name_component.h"
#include "game/component/actor_identity_component.h"
#include "game/component/recruitable_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/party_ids.h"
#include "game/ui/localized_text.h"
#include "game/ui/player_portrait_service.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <spdlog/fmt/fmt.h>

#include <utility>

namespace {

constexpr std::string_view NONE_DECORATOR = "none";
constexpr float NOTICE_SECONDS = 2.0F;

[[nodiscard]] std::string portraitDecoratorForActor(const game::data::ActorData& actor) {
    if (actor.portrait_.decorator_.empty()) {
        return std::string{NONE_DECORATOR};
    }
    return fmt::format("image({})", actor.portrait_.decorator_);
}

} // namespace

namespace game::ui {

DialoguePresentationController::DialoguePresentationController(entt::dispatcher& dispatcher,
                                                               entt::registry& registry,
                                                               DialogueBoxViewPort* conversation_box,
                                                               FloatingNoticeViewPort* notice_view,
                                                               FloatingNoticeViewPort* item_notice_view,
                                                               HotbarVisibilityPort* hotbar_ui,
                                                               const game::data::RpgCatalog* rpg_catalog,
                                                               glm::vec2 notice_offset,
                                                               glm::vec2 item_notice_offset,
                                                               const PlayerPortraitService* player_portrait_service,
                                                               const game::runtime::LocalizationService* localization)
    : dispatcher_(dispatcher),
      registry_(registry),
      conversation_box_(conversation_box),
      hotbar_ui_(hotbar_ui),
      notice_slot_{.view = notice_view, .screen_offset = notice_offset},
      item_notice_slot_{.view = item_notice_view, .screen_offset = item_notice_offset},
      rpg_catalog_(rpg_catalog),
      player_portrait_service_(player_portrait_service),
      localization_(localization) {
    buildActorCaches();
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialoguePresentationController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().connect<&DialoguePresentationController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialoguePresentationController::onHide>(this);
}

DialoguePresentationController::~DialoguePresentationController() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().disconnect<&DialoguePresentationController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().disconnect<&DialoguePresentationController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&DialoguePresentationController::onHide>(this);
}

void DialoguePresentationController::update(const float delta_time) {
    updateNoticeSlot(notice_slot_, delta_time);
    updateNoticeSlot(item_notice_slot_, delta_time);
}

void DialoguePresentationController::buildActorCaches() {
    portrait_by_actor_id_hash_.clear();
    portrait_by_map_actor_id_hash_.clear();
    display_name_key_by_map_actor_id_hash_.clear();
    if (!rpg_catalog_) {
        return;
    }

    for (const auto* actor : rpg_catalog_->listActors()) {
        if (!actor) {
            continue;
        }
        if (actor->map_actor_id_hash_ != entt::null && !actor->display_name_.empty()) {
            display_name_key_by_map_actor_id_hash_.insert_or_assign(actor->map_actor_id_hash_, actor->display_name_);
        }
        const std::string decorator = portraitDecoratorForActor(*actor);
        if (decorator == NONE_DECORATOR) {
            continue;
        }
        portrait_by_actor_id_hash_.insert_or_assign(actor->id_hash_, decorator);
        if (actor->map_actor_id_hash_ != entt::null) {
            portrait_by_map_actor_id_hash_.insert_or_assign(actor->map_actor_id_hash_, decorator);
        }
    }
}

void DialoguePresentationController::onShow(const game::defs::DialogueShowEvent& evt) {
    if (evt.channel == game::defs::DialogueChannel::Conversation) {
        if (!conversation_box_) {
            return;
        }
        hideHotbarForConversation();
        conversation_box_->setSpeaker(resolveSpeakerName(evt));
        conversation_box_->setText(evt.text);
        conversation_box_->setPortraitDecorator(resolvePortraitDecorator(evt));
        conversation_box_->setVisible(true);
        return;
    }

    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->target = evt.target;
    slot->remaining_seconds = NOTICE_SECONDS;
    slot->view->setWorldAnchor(evt.world_position, slot->screen_offset);
    slot->view->setText(formatNoticeText(resolveSpeakerName(evt), evt.text));
    slot->view->setVisible(true);
}

void DialoguePresentationController::onMove(const game::defs::DialogueMoveEvent& evt) {
    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->view->setWorldAnchor(evt.world_position, slot->screen_offset);
}

void DialoguePresentationController::onHide(const game::defs::DialogueHideEvent& evt) {
    if (evt.channel == game::defs::DialogueChannel::Conversation) {
        if (conversation_box_) {
            conversation_box_->setVisible(false);
        }
        restoreHotbarAfterConversation();
        return;
    }

    auto* slot = noticeSlot(evt.channel);
    if (!slot || !slot->view) {
        return;
    }
    slot->view->clearWorldAnchor();
    slot->view->setVisible(false);
    slot->target = entt::null;
    slot->remaining_seconds = 0.0F;
}

void DialoguePresentationController::hideHotbarForConversation() {
    if (conversation_active_) {
        return;
    }

    conversation_active_ = true;
    restore_hotbar_after_conversation_ = hotbar_ui_ && hotbar_ui_->isVisible();
    if (restore_hotbar_after_conversation_) {
        hotbar_ui_->hide();
    }
}

void DialoguePresentationController::restoreHotbarAfterConversation() {
    if (!conversation_active_) {
        return;
    }

    conversation_active_ = false;
    if (restore_hotbar_after_conversation_ && hotbar_ui_) {
        hotbar_ui_->show();
    }
    restore_hotbar_after_conversation_ = false;
}

void DialoguePresentationController::updateNoticeSlot(NoticeSlot& slot, const float delta_time) {
    if (!slot.view || slot.remaining_seconds <= 0.0F) {
        return;
    }

    slot.remaining_seconds -= delta_time;
    if (slot.target != entt::null && registry_.valid(slot.target)) {
        slot.view->setWorldAnchor(noticeAnchor(slot.target, glm::vec2{0.0F}), slot.screen_offset);
    }

    if (slot.remaining_seconds <= 0.0F) {
        slot.view->clearWorldAnchor();
        slot.view->setVisible(false);
        slot.target = entt::null;
        slot.remaining_seconds = 0.0F;
    }
}

glm::vec2 DialoguePresentationController::noticeAnchor(const entt::entity target,
                                                       const glm::vec2 fallback_position) const {
    if (target == entt::null || !registry_.valid(target)) {
        return fallback_position;
    }

    const auto* transform = registry_.try_get<engine::component::TransformComponent>(target);
    if (!transform) {
        return fallback_position;
    }

    glm::vec2 offset{0.0F, -16.0F};
    if (const auto* sprite = registry_.try_get<engine::component::SpriteComponent>(target)) {
        offset.y = -sprite->size_.y * sprite->pivot_.y;
    }
    return transform->position_ + offset;
}

std::string DialoguePresentationController::resolveActorNameKey(const std::string_view actor_id) const {
    if (!localization_ || actor_id.empty()) {
        return {};
    }

    const std::string key = std::string{actor_id} + ".name";
    if (localization_->hasText(key)) {
        return localization_->tr(key);
    }
    return {};
}

std::string DialoguePresentationController::resolveCatalogActorName(const entt::id_type actor_id_hash) const {
    if (!localization_ || !rpg_catalog_ || actor_id_hash == entt::null) {
        return {};
    }

    const auto* actor = rpg_catalog_->findActor(actor_id_hash);
    if (!actor || actor->display_name_.empty()) {
        return {};
    }
    return game::ui::tryLocalize(localization_, actor->display_name_);
}

std::string DialoguePresentationController::resolveSpeakerName(
    const game::defs::DialogueShowEvent& evt) const {
    if (std::string actor_name = resolveCatalogActorName(evt.speaker_actor_id_hash); !actor_name.empty()) {
        return actor_name;
    }
    if (std::string actor_name = resolveActorNameKey(evt.speaker_actor_id); !actor_name.empty()) {
        return actor_name;
    }

    if (const auto* identity = registry_.try_get<game::component::ActorIdentityComponent>(evt.target)) {
        if (std::string actor_name = resolveCatalogActorName(identity->actor_id_hash_); !actor_name.empty()) {
            return actor_name;
        }
        if (std::string actor_name = resolveActorNameKey(identity->actor_id_); !actor_name.empty()) {
            return actor_name;
        }
    }

    if (localization_) {
        if (const auto* name = registry_.try_get<engine::component::NameComponent>(evt.target)) {
            if (const auto it = display_name_key_by_map_actor_id_hash_.find(name->name_id_);
                it != display_name_key_by_map_actor_id_hash_.end()) {
                return game::ui::tryLocalize(localization_, it->second);
            }
        }
    }

    return game::ui::tryLocalize(localization_, evt.speaker);
}

std::string DialoguePresentationController::resolvePortraitDecorator(const game::defs::DialogueShowEvent& evt) const {
    const auto dynamic_player_portrait = [this]() -> std::string {
        if (player_portrait_service_ && player_portrait_service_->ready()) {
            return player_portrait_service_->decoratorString(game::ui::PortraitImageKind::Standard64);
        }
        return std::string{NONE_DECORATOR};
    };

    if (evt.speaker_actor_id_hash != entt::null) {
        if (evt.speaker_actor_id_hash == entt::hashed_string{game::defs::kDefaultPlayerActorId.data()}.value()) {
            const std::string decorator = dynamic_player_portrait();
            if (decorator != NONE_DECORATOR) {
                return decorator;
            }
        }
        if (const auto it = portrait_by_actor_id_hash_.find(evt.speaker_actor_id_hash);
            it != portrait_by_actor_id_hash_.end()) {
            return it->second;
        }
    }

    if (const auto* recruitable = registry_.try_get<game::component::RecruitableComponent>(evt.target)) {
        if (const auto it = portrait_by_actor_id_hash_.find(recruitable->actor_id_hash_);
            it != portrait_by_actor_id_hash_.end()) {
            return it->second;
        }
    }

    if (const auto* name = registry_.try_get<engine::component::NameComponent>(evt.target)) {
        if (const auto it = portrait_by_map_actor_id_hash_.find(name->name_id_);
            it != portrait_by_map_actor_id_hash_.end()) {
            return it->second;
        }
    }

    return std::string{NONE_DECORATOR};
}

const DialoguePresentationController::NoticeSlot* DialoguePresentationController::noticeSlot(
    game::defs::DialogueChannel channel) const {
    switch (channel) {
    case game::defs::DialogueChannel::Notice:
        return &notice_slot_;
    case game::defs::DialogueChannel::ItemNotice:
        return &item_notice_slot_;
    case game::defs::DialogueChannel::Conversation:
        return nullptr;
    }
    return nullptr;
}

DialoguePresentationController::NoticeSlot* DialoguePresentationController::noticeSlot(
    game::defs::DialogueChannel channel) {
    return const_cast<NoticeSlot*>(std::as_const(*this).noticeSlot(channel));
}

std::string DialoguePresentationController::formatNoticeText(std::string_view speaker, std::string_view text) {
    std::string output;
    output.reserve(speaker.size() + text.size() + 4);
    if (!speaker.empty()) {
        output.append(speaker);
        output.append(":\n");
    }
    output.append(text);
    return output;
}

} // namespace game::ui
