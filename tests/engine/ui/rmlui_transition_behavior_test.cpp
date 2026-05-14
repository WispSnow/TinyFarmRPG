// NOLINTBEGIN
#include <gtest/gtest.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>

#include <memory>
#include <string>
#include <utility>

namespace engine::ui::rmlui {
namespace {

class TestSystemInterface final : public Rml::SystemInterface {
public:
    [[nodiscard]] double GetElapsedTime() override {
        return elapsed_time_;
    }

    bool LogMessage(Rml::Log::Type, const Rml::String&) override {
        return true;
    }

    void advance(double delta_seconds) {
        elapsed_time_ += delta_seconds;
    }

private:
    double elapsed_time_{0.0};
};

class NullRenderInterface final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return {};
    }

    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override {
        return {};
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override {
        return {};
    }

    void ReleaseTexture(Rml::TextureHandle) override {
    }

    void EnableScissorRegion(bool) override {
    }

    void SetScissorRegion(Rml::Rectanglei) override {
    }
};

class TransitionListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        ++count;
        last_property = event.GetParameter<Rml::String>("property", "");
    }

    int count{0};
    Rml::String last_property{};
};

class CountingEventListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event&) override {
        ++count;
    }

    int count{0};
};

class RmlUiTransitionBehaviorTest : public ::testing::Test {
protected:
    static constexpr const char* kDocument = R"(
<rml>
<head>
    <style>
        body, div { display: block; }
        body {
            width: 200px;
            height: 120px;
            margin: 0;
        }
        #fade-overlay {
            width: 200px;
            height: 120px;
            opacity: 0;
        }
        #fade-overlay.is-opaque {
            opacity: 1;
        }
    </style>
</head>
<body>
    <div id="fade-overlay"></div>
</body>
</rml>
)";

    void SetUp() override {
        Rml::SetSystemInterface(&system_interface_);
        Rml::SetRenderInterface(&render_interface_);
        ASSERT_TRUE(Rml::Initialise());

        context_ = Rml::CreateContext("transition-test", Rml::Vector2i{640, 360});
        ASSERT_NE(context_, nullptr);
    }

    void TearDown() override {
        if (context_ != nullptr) {
            const Rml::String name = context_->GetName();
            (void)Rml::RemoveContext(name);
            context_ = nullptr;
        }
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        Rml::SetSystemInterface(nullptr);
    }

    [[nodiscard]] Rml::ElementDocument* loadDocument() {
        auto* document = context_->LoadDocumentFromMemory(kDocument, ".");
        if (!document) {
            return nullptr;
        }
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        context_->Update();
        return document;
    }

    void advanceContext(double delta_seconds, int steps = 1) {
        const double step = delta_seconds / static_cast<double>(steps);
        for (int i = 0; i < steps; ++i) {
            system_interface_.advance(step);
            context_->Update();
        }
    }

    TestSystemInterface system_interface_{};
    NullRenderInterface render_interface_{};
    Rml::Context* context_{nullptr};
};

TEST_F(RmlUiTransitionBehaviorTest, InvalidLinearTweenKeywordPreventsTransitionend) {
    std::unique_ptr<Rml::ElementDocument, void (*)(Rml::ElementDocument*)> document(loadDocument(), [](Rml::ElementDocument* doc) {
        if (doc) {
            doc->Close();
        }
    });
    ASSERT_NE(document.get(), nullptr);

    auto* overlay = document->GetElementById("fade-overlay");
    ASSERT_NE(overlay, nullptr);

    TransitionListener listener{};
    overlay->AddEventListener("transitionend", &listener);

    overlay->SetProperty("transition", "opacity 0.200s linear");
    overlay->SetClass("is-opaque", true);
    advanceContext(0.3, 6);

    EXPECT_EQ(listener.count, 0);

    overlay->RemoveEventListener("transitionend", &listener);
}

TEST_F(RmlUiTransitionBehaviorTest, NoneTransitionPreventsTransitionend) {
    std::unique_ptr<Rml::ElementDocument, void (*)(Rml::ElementDocument*)> document(loadDocument(), [](Rml::ElementDocument* doc) {
        if (doc) {
            doc->Close();
        }
    });
    ASSERT_NE(document.get(), nullptr);

    auto* overlay = document->GetElementById("fade-overlay");
    ASSERT_NE(overlay, nullptr);

    TransitionListener listener{};
    overlay->AddEventListener("transitionend", &listener);

    overlay->SetProperty("transition", "none");
    overlay->SetClass("is-opaque", true);
    advanceContext(0.3, 6);

    EXPECT_EQ(listener.count, 0);

    overlay->RemoveEventListener("transitionend", &listener);
}

TEST_F(RmlUiTransitionBehaviorTest, ValidLinearInOutTweenKeywordFiresTransitionend) {
    std::unique_ptr<Rml::ElementDocument, void (*)(Rml::ElementDocument*)> document(loadDocument(), [](Rml::ElementDocument* doc) {
        if (doc) {
            doc->Close();
        }
    });
    ASSERT_NE(document.get(), nullptr);

    auto* overlay = document->GetElementById("fade-overlay");
    ASSERT_NE(overlay, nullptr);

    TransitionListener listener{};
    overlay->AddEventListener("transitionend", &listener);

    overlay->SetProperty("transition", "opacity 0.200s linear-in-out");
    overlay->SetClass("is-opaque", true);
    advanceContext(0.3, 6);

    EXPECT_EQ(listener.count, 1);
    EXPECT_EQ(listener.last_property, "opacity");

    overlay->RemoveEventListener("transitionend", &listener);
}

TEST_F(RmlUiTransitionBehaviorTest, ScrollableButtonListWithNarrowScrollbarReceivesClicks) {
    static constexpr const char* kScrollableDocument = R"(
<rml>
<head>
    <style>
        body, div, button { display: block; }
        body {
            position: absolute;
            left: 0px;
            top: 0px;
            width: 240px;
            height: 120px;
            margin: 0;
        }
        #candidate-panel {
            width: 218px;
            height: 48px;
            overflow-y: auto;
            overflow-x: hidden;
        }
        #candidate-panel scrollbarvertical {
            width: 4px;
            scrollbar-margin: 0px;
        }
        .candidate-entry {
            width: 204px;
            height: 20px;
            margin-bottom: 2px;
            background-color: #1f2335ff;
            border-width: 0px;
        }
    </style>
</head>
<body>
    <div id="candidate-panel">
        <button class="candidate-entry" id="candidate-0"></button>
        <button class="candidate-entry" id="candidate-1"></button>
        <button class="candidate-entry" id="candidate-2"></button>
        <button class="candidate-entry" id="candidate-3"></button>
    </div>
</body>
</rml>
)";

    std::unique_ptr<Rml::ElementDocument, void (*)(Rml::ElementDocument*)> document(
        context_->LoadDocumentFromMemory(kScrollableDocument, "."),
        [](Rml::ElementDocument* doc) {
            if (doc) {
                doc->Close();
            }
        });
    ASSERT_NE(document.get(), nullptr);
    document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    context_->Update();

    auto* panel = document->GetElementById("candidate-panel");
    auto* first_candidate = document->GetElementById("candidate-0");
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(first_candidate, nullptr);
    EXPECT_GT(panel->GetScrollHeight(), panel->GetClientHeight());
    EXPECT_LT(panel->GetClientWidth(), panel->GetOffsetWidth());

    CountingEventListener listener{};
    first_candidate->AddEventListener(Rml::EventId::Click, &listener);

    const int target_x =
        static_cast<int>(first_candidate->GetAbsoluteLeft() + first_candidate->GetOffsetWidth() * 0.5F);
    const int target_y =
        static_cast<int>(first_candidate->GetAbsoluteTop() + first_candidate->GetOffsetHeight() * 0.5F);
    (void)context_->ProcessMouseMove(target_x, target_y, 0);
    context_->Update();
    (void)context_->ProcessMouseButtonDown(0, 0);
    context_->Update();
    (void)context_->ProcessMouseButtonUp(0, 0);
    context_->Update();

    EXPECT_EQ(listener.count, 1);

    first_candidate->RemoveEventListener(Rml::EventId::Click, &listener);
}

} // namespace
} // namespace engine::ui::rmlui
// NOLINTEND
