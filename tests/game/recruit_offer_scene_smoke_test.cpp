// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

TEST(RecruitOfferSceneSmokeTest, RecruitOfferSceneOwnsConfirmationUiAndRecruitCommand) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/recruit_offer_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/recruit_offer_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/recruit_offer.rml").lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/recruit_offer.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string rml = test_source_utils::readTextFile(rml_path);
    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(header.find("class RecruitOfferScene final"), std::string::npos);
    EXPECT_NE(header.find("RmlDocumentController document_controller_"), std::string::npos);
    EXPECT_NE(header.find("const game::data::ActorData& actor_"), std::string::npos);
    EXPECT_NE(header.find("void onAccept()"), std::string::npos);
    EXPECT_NE(header.find("void onDecline()"), std::string::npos);

    const std::string init_block = test_source_utils::extractFunctionBlock(source, "bool RecruitOfferScene::init()");
    const std::string init_ui_block =
        test_source_utils::extractFunctionBlock(source, "bool RecruitOfferScene::initUI()");
    const std::string clean_block = test_source_utils::extractFunctionBlock(source, "void RecruitOfferScene::clean()");
    const std::string accept_block =
        test_source_utils::extractFunctionBlock(source, "void RecruitOfferScene::onAccept()");
    ASSERT_FALSE(init_block.empty());
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(clean_block.empty());
    ASSERT_FALSE(accept_block.empty());

    EXPECT_NE(init_block.find("InputContextId::Dialogue"), std::string::npos);
    EXPECT_NE(init_block.find("setState(engine::core::State::Paused)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("data model"), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"accept\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"decline\""), std::string::npos);
    EXPECT_NE(source.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_NE(source.find("Focus(true)"), std::string::npos);
    EXPECT_NE(accept_block.find("RecruitPartyMemberCommand"), std::string::npos);
    EXPECT_NE(accept_block.find("requestPopScene()"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getInputManager().popContext();"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getGameState().setState(previous_state_);"), std::string::npos);

    EXPECT_NE(rml.find("data-model=\"recruit_offer\""), std::string::npos);
    EXPECT_NE(rml.find("{{ speaker_text }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ offer_text }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ actor_name }}"), std::string::npos);
    EXPECT_NE(rml.find("id=\"recruit-offer-accept-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"accept\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"recruit-offer-decline-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"decline\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-portrait-lyria"), std::string::npos);
    EXPECT_NE(rml.find("data-class-portrait-tori"), std::string::npos);

    EXPECT_NE(rcss.find("display: block;"), std::string::npos);
    EXPECT_NE(rcss.find("@spritesheet recruit-portrait-player"), std::string::npos);
    EXPECT_NE(rcss.find("#recruit-offer-panel"), std::string::npos);
    EXPECT_NE(rcss.find(".recruit-offer-action-button"), std::string::npos);
}

TEST(RecruitOfferSceneSmokeTest, GameScenePushesRecruitOfferSceneFromRequestEvent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("#include \"recruit_offer_scene.h\""), std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::RecruitOfferRequestedEvent>()"), std::string::npos);

    const std::string handler_block =
        test_source_utils::extractFunctionBlock(
            source,
            "void GameScene::onRecruitOfferRequested(const game::defs::RecruitOfferRequestedEvent& evt)");
    ASSERT_FALSE(handler_block.empty());
    EXPECT_NE(handler_block.find("RecruitOfferScene"), std::string::npos);
    EXPECT_NE(handler_block.find("services_->rpg_catalog->findActor"), std::string::npos);
    EXPECT_NE(handler_block.find("isPaused()"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
