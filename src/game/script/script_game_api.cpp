#include "script_game_api.h"

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
#include "engine/script/script_host.h"
#include "game/component/actor_identity_component.h"
#include "game/component/chest_component.h"
#include "game/component/map_component.h"
#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/party_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/recruitable_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/script_trigger_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands_battle.h"
#include "game/defs/commands_inventory.h"
#include "game/defs/commands_interaction.h"
#include "game/defs/commands_quest.h"
#include "game/defs/commands_recruit.h"
#include "game/defs/commands_shop.h"
#include "game/defs/events_dialogue.h"
#include "game/defs/events_quest.h"
#include "game/defs/events_recruit.h"
#include "game/domain/quest_log_ops.h"
#include "game/system/system_helpers.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace game::script {

namespace {

[[nodiscard]] std::optional<game::defs::DialogueChannel> parseDialogueChannel(const int value) {
    switch (value) {
    case 0:
        return game::defs::DialogueChannel::Conversation;
    case 1:
        return game::defs::DialogueChannel::Notice;
    case 2:
        return game::defs::DialogueChannel::ItemNotice;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] entt::id_type hashId(const std::string_view value) {
    return value.empty() ? entt::id_type{entt::null} : entt::hashed_string{value.data(), value.size()}.value();
}

template <typename T>
[[nodiscard]] T* findRegistryContextPointer(entt::registry& registry) {
    auto** ptr = registry.ctx().find<T*>();
    return ptr ? *ptr : nullptr;
}

[[nodiscard]] bool containsString(const std::vector<std::string>& values, const std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] const game::data::QuestObjectiveData* findObjective(const game::data::QuestData& quest,
                                                                  const std::string_view objective_id) {
    const auto found = std::find_if(
        quest.objectives_.begin(),
        quest.objectives_.end(),
        [objective_id](const game::data::QuestObjectiveData& objective) {
            return objective.id_ == objective_id;
        });
    return found == quest.objectives_.end() ? nullptr : &*found;
}

[[nodiscard]] ScriptCommandResult commandOk() {
    return ScriptCommandResult{.ok = true, .reason = {}};
}

[[nodiscard]] ScriptCommandResult commandFailure(std::string reason) {
    return ScriptCommandResult{.ok = false, .reason = std::move(reason)};
}

template <typename Event>
void triggerFromScript(engine::script::ScriptHost& host, entt::dispatcher& dispatcher, Event event) {
    if (!host.isHandlingScriptCallback()) {
        dispatcher.trigger(std::move(event));
        return;
    }

    host.enqueueDeferredCommand([&dispatcher, event = std::move(event)]() mutable {
        dispatcher.trigger(std::move(event));
    });
}

template <typename Event>
void enqueueFromScript(engine::script::ScriptHost& host, entt::dispatcher& dispatcher, Event event) {
    if (!host.isHandlingScriptCallback()) {
        dispatcher.enqueue(std::move(event));
        return;
    }

    host.enqueueDeferredCommand([&dispatcher, event = std::move(event)]() mutable {
        dispatcher.enqueue(std::move(event));
    });
}

struct RecruitRequestResolution {
    ScriptCommandResult status{};
    entt::entity player{entt::null};
    entt::entity recruiter{entt::null};
    entt::id_type actor_id_hash{entt::null};
    std::string actor_id{};
};

[[nodiscard]] RecruitRequestResolution resolveRecruitRequest(
    engine::script::ScriptHost& host,
    entt::registry& registry,
    const std::string_view actor_id,
    const std::optional<engine::script::ScriptEntityHandle>& recruiter_handle,
    const std::string_view api_name) {
    if (actor_id.empty()) {
        return {.status = commandFailure("invalid_actor_id")};
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry);
    if (player == entt::null) {
        return {.status = commandFailure("no_player")};
    }

    const auto* rpg_catalog = findRegistryContextPointer<game::data::RpgCatalog>(registry);
    if (rpg_catalog && !rpg_catalog->findActor(actor_id)) {
        return {.status = commandFailure("unknown_actor")};
    }

    entt::entity recruiter = entt::null;
    if (recruiter_handle.has_value()) {
        const std::string validation_source = std::string{api_name} + ".recruiter";
        if (!host.validateHandle(recruiter_handle.value(), recruiter, validation_source)) {
            return {.status = commandFailure("invalid_recruiter")};
        }
    }

    return RecruitRequestResolution{
        .status = commandOk(),
        .player = player,
        .recruiter = recruiter,
        .actor_id_hash = hashId(actor_id),
        .actor_id = std::string{actor_id},
    };
}

struct QuestOfferResolution {
    ScriptCommandResult status{};
    entt::entity player{entt::null};
    entt::entity giver{entt::null};
    const game::data::QuestData* quest{nullptr};
};

[[nodiscard]] QuestOfferResolution resolveQuestOfferRequest(
    engine::script::ScriptHost& host,
    entt::registry& registry,
    const std::string_view quest_id,
    const std::optional<engine::script::ScriptEntityHandle>& giver_handle,
    const std::string_view api_name) {
    if (quest_id.empty()) {
        return {.status = commandFailure("invalid_quest_id")};
    }
    if (!giver_handle.has_value()) {
        return {.status = commandFailure("invalid_giver")};
    }

    const auto* quest_catalog = findRegistryContextPointer<game::data::QuestCatalog>(registry);
    if (!quest_catalog) {
        return {.status = commandFailure("catalog_unavailable")};
    }
    const auto* quest = quest_catalog->findQuest(quest_id);
    if (!quest) {
        return {.status = commandFailure("unknown_quest")};
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry);
    if (player == entt::null) {
        return {.status = commandFailure("no_player")};
    }
    const auto* quest_log = registry.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        return {.status = commandFailure("missing_quest_log")};
    }

    if (game::domain::quest_log_ops::isQuestCompleted(*quest_log, quest->id_hash_) ||
        game::domain::quest_log_ops::isQuestReadyToTurnIn(*quest_log, *quest) ||
        game::domain::quest_log_ops::isQuestActive(*quest_log, quest->id_hash_)) {
        return {.status = commandFailure("not_available")};
    }

    entt::entity giver = entt::null;
    const std::string validation_source = std::string{api_name} + ".giver";
    if (!host.validateHandle(giver_handle.value(), giver, validation_source)) {
        return {.status = commandFailure("invalid_giver")};
    }
    const auto* giver_component = registry.try_get<game::component::QuestGiverComponent>(giver);
    if (!giver_component || giver_component->quest_id_hash_ != quest->id_hash_) {
        return {.status = commandFailure("invalid_giver")};
    }

    return QuestOfferResolution{
        .status = commandOk(),
        .player = player,
        .giver = giver,
        .quest = quest,
    };
}

} // namespace

ScriptGameApi::ScriptGameApi(engine::script::ScriptHost& host,
                             entt::registry& registry,
                             entt::dispatcher& dispatcher)
    : host_(host),
      registry_(registry),
      dispatcher_(dispatcher) {
}

std::uint32_t ScriptGameApi::day() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->day_ : 0U;
}

float ScriptGameApi::hour() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->hour_ : 0.0F;
}

float ScriptGameApi::minute() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    return game_time ? game_time->minute_ : 0.0F;
}

std::string ScriptGameApi::formattedTime() const {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        return "Day 0, 00:00";
    }
    return game_time->getFormattedTime();
}

bool ScriptGameApi::playerExists() const {
    return game::system::helpers::getPlayerEntity(registry_) != entt::null;
}

std::optional<engine::script::ScriptEntityHandle> ScriptGameApi::playerHandle() const {
    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return std::nullopt;
    }
    return host_.makeHandle(player);
}

std::tuple<float, float> ScriptGameApi::playerPosition() const {
    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return {0.0F, 0.0F};
    }

    const auto* transform = registry_.try_get<engine::component::TransformComponent>(player);
    if (!transform) {
        return {0.0F, 0.0F};
    }

    return {transform->position_.x, transform->position_.y};
}

std::optional<std::string> ScriptGameApi::entityActorId(
    const engine::script::ScriptEntityHandle& handle) const {
    entt::entity entity = entt::null;
    if (!host_.validateHandle(handle, entity, "tf.entity.actor_id")) {
        return std::nullopt;
    }

    const auto* identity = registry_.try_get<game::component::ActorIdentityComponent>(entity);
    if (!identity || identity->actor_id_.empty()) {
        return std::nullopt;
    }
    return identity->actor_id_;
}

std::optional<std::string> ScriptGameApi::entityName(
    const engine::script::ScriptEntityHandle& handle) const {
    entt::entity entity = entt::null;
    if (!host_.validateHandle(handle, entity, "tf.entity.name")) {
        return std::nullopt;
    }

    const auto* name = registry_.try_get<engine::component::NameComponent>(entity);
    if (!name || name->name_.empty()) {
        return std::nullopt;
    }
    return name->name_;
}

std::tuple<float, float> ScriptGameApi::entityPosition(
    const engine::script::ScriptEntityHandle& handle) const {
    entt::entity entity = entt::null;
    if (!host_.validateHandle(handle, entity, "tf.entity.position")) {
        return {0.0F, 0.0F};
    }

    const auto* transform = registry_.try_get<engine::component::TransformComponent>(entity);
    if (!transform) {
        return {0.0F, 0.0F};
    }

    return {transform->position_.x, transform->position_.y};
}

bool ScriptGameApi::entityHasComponent(const engine::script::ScriptEntityHandle& handle,
                                       const std::string_view kind) const {
    entt::entity entity = entt::null;
    if (!host_.validateHandle(handle, entity, "tf.entity.has_component")) {
        return false;
    }

    if (kind == "actor_identity") {
        return registry_.any_of<game::component::ActorIdentityComponent>(entity);
    }
    if (kind == "name") {
        return registry_.any_of<engine::component::NameComponent>(entity);
    }
    if (kind == "transform") {
        return registry_.any_of<engine::component::TransformComponent>(entity);
    }
    if (kind == "player") {
        return registry_.any_of<game::component::PlayerTag>(entity);
    }
    if (kind == "npc") {
        return registry_.any_of<game::component::NPCTag>(entity);
    }
    if (kind == "dialogue") {
        return registry_.any_of<game::component::DialogueComponent>(entity);
    }
    if (kind == "merchant") {
        return registry_.any_of<game::component::MerchantComponent>(entity);
    }
    if (kind == "quest_giver") {
        return registry_.any_of<game::component::QuestGiverComponent>(entity);
    }
    if (kind == "recruitable") {
        return registry_.any_of<game::component::RecruitableComponent>(entity);
    }
    if (kind == "chest") {
        return registry_.any_of<game::component::ChestComponent>(entity);
    }
    if (kind == "rest") {
        return registry_.any_of<game::component::RestArea>(entity);
    }
    if (kind == "closet") {
        return registry_.any_of<game::component::ClosetArea>(entity);
    }
    if (kind == "scripted_interaction") {
        return registry_.any_of<game::component::ScriptedInteractionComponent>(entity);
    }
    if (kind == "script_trigger") {
        return registry_.any_of<game::component::ScriptTriggerComponent>(entity);
    }
    if (kind == "map_id") {
        return registry_.any_of<game::component::MapId>(entity);
    }

    return false;
}

std::string ScriptGameApi::questStatus(const std::string_view quest_id) const {
    if (quest_id.empty()) {
        return "unknown";
    }

    const auto* quest_catalog = findRegistryContextPointer<game::data::QuestCatalog>(registry_);
    if (!quest_catalog) {
        return "unknown";
    }

    const auto* quest = quest_catalog->findQuest(quest_id);
    if (!quest) {
        return "unknown";
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return "unknown";
    }

    const auto* quest_log = registry_.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        return "unknown";
    }

    if (game::domain::quest_log_ops::isQuestCompleted(*quest_log, quest->id_hash_)) {
        return "completed";
    }
    if (game::domain::quest_log_ops::isQuestReadyToTurnIn(*quest_log, *quest)) {
        return "ready_to_turn_in";
    }
    if (game::domain::quest_log_ops::isQuestActive(*quest_log, quest->id_hash_)) {
        return "in_progress";
    }
    return "offerable";
}

QuestProgressSnapshot ScriptGameApi::questProgress(const std::string_view quest_id,
                                                   const std::string_view objective_id) const {
    QuestProgressSnapshot snapshot{};
    if (quest_id.empty() || objective_id.empty()) {
        return snapshot;
    }

    const auto* quest_catalog = findRegistryContextPointer<game::data::QuestCatalog>(registry_);
    const auto* quest = quest_catalog ? quest_catalog->findQuest(quest_id) : nullptr;
    if (!quest) {
        return snapshot;
    }

    const auto* objective = findObjective(*quest, objective_id);
    if (!objective) {
        return snapshot;
    }
    snapshot.required = objective->required_count_;

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    const auto* quest_log =
        player == entt::null ? nullptr : registry_.try_get<game::component::QuestLogComponent>(player);
    if (!quest_log) {
        return snapshot;
    }

    const std::string key = game::data::makeQuestObjectiveProgressKey(quest->id_, objective->id_);
    if (const auto found = quest_log->objective_progress.find(key); found != quest_log->objective_progress.end()) {
        snapshot.current = found->second;
    }
    return snapshot;
}

bool ScriptGameApi::questIsAvailable(const std::string_view quest_id) const {
    return questStatus(quest_id) == "offerable";
}

ScriptCommandResult ScriptGameApi::questOffer(
    const std::string_view quest_id,
    const std::optional<engine::script::ScriptEntityHandle>& giver_handle) {
    const auto request = resolveQuestOfferRequest(
        host_,
        registry_,
        quest_id,
        giver_handle,
        "tf.quest.offer");
    if (!request.status.ok) {
        return request.status;
    }

    triggerFromScript(host_, dispatcher_, game::defs::QuestOfferRequestedEvent{
        .player = request.player,
        .giver = request.giver,
        .quest_id_hash = request.quest->id_hash_,
        .quest_id = request.quest->id_,
    });
    return commandOk();
}

ScriptCommandResult ScriptGameApi::questAccept(
    const std::string_view quest_id,
    const std::optional<engine::script::ScriptEntityHandle>& giver_handle) {
    const auto request = resolveQuestOfferRequest(
        host_,
        registry_,
        quest_id,
        giver_handle,
        "tf.quest.accept");
    if (!request.status.ok) {
        return request.status;
    }

    triggerFromScript(host_, dispatcher_, game::defs::AcceptQuestCommand{
        .player = request.player,
        .giver = request.giver,
        .quest_id_hash = request.quest->id_hash_,
        .quest_id = request.quest->id_,
    });
    return commandOk();
}

ScriptCommandResult ScriptGameApi::questTurnIn(
    const std::string_view quest_id,
    const std::optional<engine::script::ScriptEntityHandle>& giver_handle) {
    if (quest_id.empty()) {
        return commandFailure("invalid_quest_id");
    }
    if (!giver_handle.has_value()) {
        return commandFailure("invalid_giver");
    }

    const auto* quest_catalog = findRegistryContextPointer<game::data::QuestCatalog>(registry_);
    if (!quest_catalog) {
        return commandFailure("catalog_unavailable");
    }
    const auto* quest = quest_catalog->findQuest(quest_id);
    if (!quest) {
        return commandFailure("unknown_quest");
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return commandFailure("no_player");
    }
    if (!registry_.all_of<game::component::QuestLogComponent>(player)) {
        return commandFailure("missing_quest_log");
    }

    if (questStatus(quest_id) != "ready_to_turn_in") {
        return commandFailure("not_ready");
    }

    entt::entity giver = entt::null;
    if (!host_.validateHandle(giver_handle.value(), giver, "tf.quest.turn_in.giver")) {
        return commandFailure("invalid_giver");
    }
    const auto* giver_component = registry_.try_get<game::component::QuestGiverComponent>(giver);
    if (!giver_component || giver_component->quest_id_hash_ != quest->id_hash_) {
        return commandFailure("invalid_giver");
    }

    triggerFromScript(host_, dispatcher_, game::defs::TurnInQuestCommand{
        .player = player,
        .giver = giver,
        .quest_id_hash = quest->id_hash_,
        .quest_id = quest->id_,
    });
    return commandOk();
}

std::vector<std::string> ScriptGameApi::partyMembers() const {
    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return {};
    }

    const auto* party = registry_.try_get<game::component::PartyComponent>(player);
    if (!party) {
        return {};
    }
    return party->recruited_actor_ids_;
}

bool ScriptGameApi::partyIsRecruited(const std::string_view actor_id) const {
    if (actor_id.empty()) {
        return false;
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return false;
    }

    const auto* party = registry_.try_get<game::component::PartyComponent>(player);
    return party && containsString(party->recruited_actor_ids_, actor_id);
}

ScriptCommandResult ScriptGameApi::partyOfferRecruit(
    const std::string_view actor_id,
    const std::optional<engine::script::ScriptEntityHandle>& recruiter_handle) {
    const auto request = resolveRecruitRequest(
        host_,
        registry_,
        actor_id,
        recruiter_handle,
        "tf.party.offer_recruit");
    if (!request.status.ok) {
        return request.status;
    }

    triggerFromScript(host_, dispatcher_, game::defs::RecruitOfferRequestedEvent{
        .player = request.player,
        .recruiter = request.recruiter,
        .actor_id_hash = request.actor_id_hash,
        .actor_id = request.actor_id,
    });
    return commandOk();
}

ScriptCommandResult ScriptGameApi::partyRequestRecruit(
    const std::string_view actor_id,
    const std::optional<engine::script::ScriptEntityHandle>& recruiter_handle) {
    const auto request = resolveRecruitRequest(
        host_,
        registry_,
        actor_id,
        recruiter_handle,
        "tf.party.request_recruit");
    if (!request.status.ok) {
        return request.status;
    }

    triggerFromScript(host_, dispatcher_, game::defs::RecruitPartyMemberCommand{
        .player = request.player,
        .recruiter = request.recruiter,
        .actor_id_hash = request.actor_id_hash,
        .actor_id = request.actor_id,
    });
    return commandOk();
}

int ScriptGameApi::partyLevel(const std::string_view actor_id) const {
    if (actor_id.empty()) {
        return 0;
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return 0;
    }

    const auto* party = registry_.try_get<game::component::PartyComponent>(player);
    if (!party || !containsString(party->recruited_actor_ids_, actor_id)) {
        return 0;
    }

    if (const auto* stats = registry_.try_get<game::component::PartyRuntimeStatsComponent>(player)) {
        if (const auto found = stats->states_by_actor_id_.find(std::string{actor_id});
            found != stats->states_by_actor_id_.end()) {
            return found->second.level;
        }
    }

    return partyInitialLevel(actor_id);
}

int ScriptGameApi::partyInitialLevel(const std::string_view actor_id) const {
    if (actor_id.empty()) {
        return 0;
    }

    const auto* rpg_catalog = findRegistryContextPointer<game::data::RpgCatalog>(registry_);
    const auto* actor = rpg_catalog ? rpg_catalog->findActor(actor_id) : nullptr;
    return actor ? actor->initial_level_ : 0;
}

ScriptCommandResult ScriptGameApi::shopOpen(
    const std::string_view shop_id,
    const std::optional<engine::script::ScriptEntityHandle>& merchant_handle) {
    if (shop_id.empty()) {
        return commandFailure("invalid_shop_id");
    }

    const auto* shop_catalog = findRegistryContextPointer<game::data::ShopCatalog>(registry_);
    if (!shop_catalog) {
        return commandFailure("catalog_unavailable");
    }
    const auto* shop = shop_catalog->findShop(shop_id);
    if (!shop) {
        return commandFailure("unknown_shop");
    }

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return commandFailure("no_player");
    }

    entt::entity merchant = entt::null;
    if (merchant_handle.has_value() &&
        !host_.validateHandle(merchant_handle.value(), merchant, "tf.shop.open.merchant")) {
        return commandFailure("invalid_merchant");
    }

    triggerFromScript(host_, dispatcher_, game::defs::OpenShopCommand{
        .player = player,
        .merchant = merchant,
        .shop_id_hash = shop->id_hash_,
        .shop_id = shop->id_,
    });
    return commandOk();
}

ScriptCommandResult ScriptGameApi::battleStart(const std::string_view troop_id,
                                               std::vector<std::string> actor_ids,
                                               const std::string_view battle_background_id) {
    if (troop_id.empty()) {
        return commandFailure("invalid_troop_id");
    }

    const auto* rpg_catalog = findRegistryContextPointer<game::data::RpgCatalog>(registry_);
    if (!rpg_catalog) {
        return commandFailure("catalog_unavailable");
    }
    if (!rpg_catalog->findTroop(troop_id)) {
        return commandFailure("unknown_troop");
    }

    triggerFromScript(host_, dispatcher_, game::defs::EnterBattleCommand{
        .actor_ids = std::move(actor_ids),
        .troop_id = std::string{troop_id},
        .battle_background_id = std::string{battle_background_id},
    });
    return commandOk();
}

std::string ScriptGameApi::currentMap() const {
    const auto* world_state = findRegistryContextPointer<game::world::WorldState>(registry_);
    if (!world_state) {
        return {};
    }

    const entt::id_type current_map = world_state->getCurrentMap();
    if (current_map == entt::null) {
        return {};
    }

    const auto* map_state = world_state->getMapState(current_map);
    return map_state ? map_state->info.name : std::string{};
}

bool ScriptGameApi::addItem(const std::string_view item_id,
                            const int count,
                            const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                            const int preferred_slot) {
    if (item_id.empty() || count <= 0) {
        spdlog::warn("ScriptHost: add_item 参数无效 item_id='{}', count={}", item_id, count);
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.add_item", target, true)) {
        return false;
    }

    triggerFromScript(host_, dispatcher_, game::defs::AddItemCommand{
        target,
        hashId(item_id),
        count,
        preferred_slot});
    return true;
}

bool ScriptGameApi::removeItem(const std::string_view item_id,
                               const int count,
                               const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                               const int slot_index) {
    if (item_id.empty() || count <= 0) {
        spdlog::warn("ScriptHost: remove_item 参数无效 item_id='{}', count={}", item_id, count);
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.remove_item", target, true)) {
        return false;
    }

    triggerFromScript(host_, dispatcher_, game::defs::RemoveItemCommand{
        target,
        hashId(item_id),
        count,
        slot_index});
    return true;
}

bool ScriptGameApi::inventorySync(const std::optional<engine::script::ScriptEntityHandle>& target_handle) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.inventory_sync", target, true)) {
        return false;
    }

    triggerFromScript(host_, dispatcher_, game::defs::InventorySyncCommand{target});
    return true;
}

bool ScriptGameApi::hotbarSync(const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                               const bool full_sync) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.command.hotbar_sync", target, true)) {
        return false;
    }

    triggerFromScript(host_, dispatcher_, game::defs::HotbarSyncCommand{target, full_sync});
    return true;
}

bool ScriptGameApi::interact(const engine::script::ScriptEntityHandle& target_handle,
                             const std::optional<engine::script::ScriptEntityHandle>& player_handle) {
    entt::entity target = entt::null;
    if (!host_.validateHandle(target_handle, target, "tf.command.interact.target")) {
        return false;
    }

    entt::entity player = entt::null;
    if (player_handle.has_value()) {
        if (!host_.validateHandle(player_handle.value(), player, "tf.command.interact.player")) {
            return false;
        }
    } else {
        player = game::system::helpers::getPlayerEntity(registry_);
        if (player == entt::null) {
            spdlog::warn("ScriptHost: tf.command.interact 失败，未找到默认玩家实体");
            return false;
        }
    }

    triggerFromScript(host_, dispatcher_, game::defs::InteractCommand{player, target});
    return true;
}

bool ScriptGameApi::showDialogue(const std::string_view text,
                                 const std::string_view speaker,
                                 const int channel,
                                 const std::optional<engine::script::ScriptEntityHandle>& target_handle,
                                 const std::string_view speaker_actor_id) {
    if (text.empty()) {
        return false;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.dialogue.show", target, false)) {
        return false;
    }

    const auto resolved_channel = parseDialogueChannel(channel);
    if (!resolved_channel.has_value()) {
        return false;
    }

    game::defs::DialogueShowEvent evt{};
    evt.target = target;
    evt.speaker = std::string{speaker};
    evt.text = std::string{text};
    evt.world_position = target == entt::null
                             ? glm::vec2{0.0F}
                             : game::system::helpers::computeHeadPosition(registry_, target);
    evt.channel = *resolved_channel;
    if (!speaker_actor_id.empty()) {
        evt.speaker_actor_id = std::string{speaker_actor_id};
        evt.speaker_actor_id_hash = hashId(speaker_actor_id);
    }

    triggerFromScript(host_, dispatcher_, std::move(evt));
    return true;
}

bool ScriptGameApi::hideDialogue(const int channel,
                                 const std::optional<engine::script::ScriptEntityHandle>& target_handle) {
    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.dialogue.hide", target, false)) {
        return false;
    }

    const auto resolved_channel = parseDialogueChannel(channel);
    if (!resolved_channel.has_value()) {
        return false;
    }

    game::defs::DialogueHideEvent evt{};
    evt.target = target;
    evt.channel = *resolved_channel;
    enqueueFromScript(host_, dispatcher_, std::move(evt));
    return true;
}

std::uint32_t ScriptGameApi::requestDialogueChoice(
    const std::string_view prompt,
    std::vector<ScriptDialogueChoiceOption> options,
    const std::optional<engine::script::ScriptEntityHandle>& target_handle,
    const std::string_view speaker,
    const std::string_view speaker_actor_id,
    const bool allow_cancel) {
    if (prompt.empty() || options.empty()) {
        return 0;
    }

    entt::entity target = entt::null;
    if (!resolveTargetEntity(target_handle, "tf.dialogue.choice", target, false)) {
        return 0;
    }

    const std::uint32_t request_id = next_dialogue_choice_request_id_++;
    if (next_dialogue_choice_request_id_ == 0) {
        next_dialogue_choice_request_id_ = 1;
    }

    game::defs::DialogueChoiceRequestedEvent event{};
    event.request_id = request_id;
    event.target = target;
    event.prompt = std::string{prompt};
    event.speaker = std::string{speaker};
    event.speaker_actor_id = std::string{speaker_actor_id};
    event.speaker_actor_id_hash = hashId(speaker_actor_id);
    event.allow_cancel = allow_cancel;
    event.options.reserve(options.size());
    for (auto& option : options) {
        event.options.push_back(game::defs::DialogueChoiceOption{
            .id = std::move(option.id),
            .label = std::move(option.label),
        });
    }

    triggerFromScript(host_, dispatcher_, std::move(event));
    return request_id;
}

bool ScriptGameApi::resolveTargetEntity(
    const std::optional<engine::script::ScriptEntityHandle>& raw_target,
    const std::string_view api_name,
    entt::entity& out_target,
    const bool require_default_player) const {
    if (raw_target.has_value()) {
        return host_.validateHandle(raw_target.value(), out_target, api_name);
    }

    out_target = game::system::helpers::getPlayerEntity(registry_);
    if (out_target == entt::null && require_default_player) {
        spdlog::warn("ScriptHost: {} 失败，未找到默认玩家实体", api_name);
        return false;
    }
    return true;
}

} // namespace game::script
