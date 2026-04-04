#include "jrpg_dialogue_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

namespace learn::jrpg {

namespace {

[[nodiscard]] Rml::Context* getRmlContext(const engine::core::Context& context) {
    auto* runtime = context.getRmlUi();
    return runtime ? runtime->getContext() : nullptr;
}

} // namespace

// ── UIEventListener ────────────────────────────────────────

void JrpgDialogueScene::UIEventListener::ProcessEvent(Rml::Event& event) {
    const auto& type = event.GetType();

    if (type == "keydown") {
        auto key = event.GetParameter<int>("key_identifier", 0);

        bool is_advance = (key == static_cast<int>(Rml::Input::KI_RETURN) ||
                           key == static_cast<int>(Rml::Input::KI_SPACE));
        if (is_advance && !scene_.show_choices_) {
            scene_.advanceDialogue();
        }
        // When choices visible, Enter/Space naturally triggers click on
        // the focused choice item via RmlUi's ProcessDefaultAction.
    }
    else if (type == "click") {
        // Click on dialogue window area → advance
        if (!scene_.show_choices_) {
            scene_.advanceDialogue();
        }
    }
}

// ── Scene lifecycle ────────────────────────────────────────

bool JrpgDialogueScene::init() {
    if (!Scene::init()) return false;

    context_.getGLRenderer().setDebugUIEnabled(true);

    setupDataModel();
    buildScript();

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_jrpg_dialogue.rml");
    if (!doc_) {
        spdlog::error("JrpgDialogueScene: failed to load RML");
        return false;
    }

    // Cache element refs
    portrait_el_        = doc_->GetElementById("portrait");
    dialogue_window_el_ = doc_->GetElementById("dialogue-window");

    // Event listener
    listener_ = std::make_unique<UIEventListener>(*this);
    addListener(doc_, "keydown");
    if (dialogue_window_el_) {
        addListener(dialogue_window_el_, "click");
    }

    // Start the conversation
    startLine(0);

    spdlog::info("JrpgDialogueScene initialized");
    return true;
}

void JrpgDialogueScene::update(float dt) {
    Scene::update(dt);

    // --- Typewriter ---
    if (!text_complete_) {
        char_timer_ += dt;
        bool changed = false;
        while (char_timer_ >= kCharDelay && displayed_chars_ < total_chars_) {
            ++displayed_chars_;
            char_timer_ -= kCharDelay;
            changed = true;
        }
        if (changed) {
            displayed_text_ = current_text_.substr(
                0, static_cast<size_t>(char_offsets_[displayed_chars_]));
            model_handle_.DirtyVariable("displayed_text");
        }
        if (displayed_chars_ >= total_chars_) {
            onTextComplete();
        }
    }

    // --- Deferred focus for choice items ---
    if (focus_first_choice_) {
        auto* list = doc_->GetElementById("choices-list");
        if (list && list->GetNumChildren() > 0) {
            list->GetChild(0)->Focus(true);
            focus_first_choice_ = false;
        }
    }
}

void JrpgDialogueScene::clean() {
    // Remove listeners before unloading documents
    for (auto& [el, event, capture] : registrations_) {
        el->RemoveEventListener(event, listener_.get(), capture);
    }
    registrations_.clear();
    listener_.reset();

    unloadAllRmlDocuments();
    doc_ = nullptr;

    if (auto* rml_ctx = getRmlContext(context_)) {
        rml_ctx->RemoveDataModel("dialogue");
    }

    Scene::clean();
}

// ── Data model ─────────────────────────────────────────────

void JrpgDialogueScene::setupDataModel() {
    auto* rml_ctx = getRmlContext(context_);
    if (!rml_ctx) return;

    auto ctor = rml_ctx->CreateDataModel("dialogue");
    if (!ctor) return;

    // Register Choice struct
    if (auto sh = ctor.RegisterStruct<Choice>()) {
        sh.RegisterMember("label", &Choice::label);
    }
    ctor.RegisterArray<std::vector<Choice>>();

    // Bind variables
    ctor.Bind("speaker_name",   &speaker_name_);
    ctor.Bind("displayed_text", &displayed_text_);
    ctor.Bind("hint_text",      &hint_text_);
    ctor.Bind("has_speaker",    &has_speaker_);
    ctor.Bind("show_choices",   &show_choices_);
    ctor.Bind("show_continue",  &show_continue_);
    ctor.Bind("choices",        &choices_);

    // Event callback: choice selected
    ctor.BindEventCallback("on_choice",
        [this](Rml::DataModelHandle /*model*/, Rml::Event& /*ev*/,
               const Rml::VariantList& args) {
            if (args.empty()) return;
            int idx = args[0].Get<int>(-1);
            onChoiceSelected(idx);
        });

    model_handle_ = ctor.GetModelHandle();
}

// ── Dialogue script ────────────────────────────────────────

void JrpgDialogueScene::buildScript() {
    script_ = {
        /* 0 */ {"Elysia", 0,
                 "Hello there, adventurer! Welcome to our village.",
                 {}, {}, 1},
        /* 1 */ {"Elysia", 1,
                 "My name is Elysia. I'm the village elder's daughter.",
                 {}, {}, 2},
        /* 2 */ {"Elysia", 7,
                 "Strange creatures have been appearing in the eastern forest...",
                 {}, {}, 3},
        /* 3 */ {"Elysia", 2,
                 "We really need help. Will you investigate the forest?",
                 {"Of course!", "Tell me more.", "I'm not interested."},
                 {5, 4, 6}, -1},
        /* 4 */ {"Elysia", 0,
                 "They come out at night with glowing red eyes... So, will you help?",
                 {"Count me in!", "Sorry, I can't."},
                 {5, 6}, -1},
        /* 5 */ {"Elysia", 4,
                 "Thank you so much! Meet me at the village gate at dawn. Good luck!",
                 {}, {}, -1},
        /* 6 */ {"Elysia", 10,
                 "I see... Please come back if you change your mind.",
                 {}, {}, -1},
    };
}

// ── Dialogue logic ─────────────────────────────────────────

void JrpgDialogueScene::startLine(int index) {
    if (index < 0 || index >= static_cast<int>(script_.size())) {
        showEndMessage();
        return;
    }

    auto& line      = script_[index];
    current_line_idx_ = index;
    current_text_     = line.text;

    // Speaker & portrait
    speaker_name_ = line.speaker;
    has_speaker_  = !line.speaker.empty();
    setPortrait(line.portrait_idx);

    // Reset typewriter
    prepareTypewriter(current_text_);
    displayed_text_.clear();
    text_complete_  = false;
    show_choices_   = false;
    show_continue_  = false;

    // Prepare choices data (rendered later when text completes)
    choices_.clear();
    for (auto& c : line.choices) {
        choices_.push_back({c});
    }

    hint_text_ = "Enter / Click: advance | Space: skip text";
    model_handle_.DirtyAllVariables();
}

void JrpgDialogueScene::advanceDialogue() {
    // During typewriter: skip to full text
    if (!text_complete_) {
        skipTypewriter();
        return;
    }

    // Choices visible: don't advance (choice click handles it)
    if (show_choices_) return;

    // End state: restart
    if (current_line_idx_ < 0) {
        startLine(0);
        return;
    }

    // Go to next line
    int next = script_[current_line_idx_].next;
    if (next >= 0) {
        startLine(next);
    } else {
        showEndMessage();
    }
}

void JrpgDialogueScene::skipTypewriter() {
    displayed_chars_ = total_chars_;
    displayed_text_  = current_text_;
    model_handle_.DirtyVariable("displayed_text");
    onTextComplete();
}

void JrpgDialogueScene::onTextComplete() {
    text_complete_ = true;

    auto& line = script_[current_line_idx_];

    if (!line.choices.empty()) {
        // Show choice window
        show_choices_  = true;
        show_continue_ = false;
        hint_text_     = "Arrows: choose | Enter: confirm";
        model_handle_.DirtyVariable("show_choices");
        model_handle_.DirtyVariable("show_continue");
        model_handle_.DirtyVariable("hint_text");

        // Focus the first choice on the next frame (elements need to be created)
        focus_first_choice_ = true;
    } else {
        show_continue_ = true;
        model_handle_.DirtyVariable("show_continue");
    }
}

void JrpgDialogueScene::onChoiceSelected(int choice_idx) {
    auto& line = script_[current_line_idx_];
    if (choice_idx < 0 || choice_idx >= static_cast<int>(line.choice_targets.size()))
        return;

    spdlog::info("Choice selected: {}", line.choices[choice_idx]);

    int target = line.choice_targets[choice_idx];
    show_choices_ = false;
    model_handle_.DirtyVariable("show_choices");

    if (target >= 0) {
        startLine(target);
    } else {
        showEndMessage();
    }
}

void JrpgDialogueScene::showEndMessage() {
    current_line_idx_ = -1;
    speaker_name_.clear();
    has_speaker_    = false;
    displayed_text_ = "[ End of dialogue -- Press Enter to restart ]";
    text_complete_  = true;
    show_choices_   = false;
    show_continue_  = true;
    hint_text_      = "Enter: restart dialogue";
    setPortrait(-1);
    model_handle_.DirtyAllVariables();
}

// ── Portrait ───────────────────────────────────────────────

void JrpgDialogueScene::setPortrait(int idx) {
    if (!portrait_el_) return;
    if (current_portrait_ >= 0) {
        portrait_el_->SetClass("face-" + std::to_string(current_portrait_), false);
    }
    current_portrait_ = idx;
    if (idx >= 0) {
        portrait_el_->SetClass("face-" + std::to_string(idx), true);
    }
}

// ── Typewriter helpers ─────────────────────────────────────

void JrpgDialogueScene::prepareTypewriter(const std::string& text) {
    char_offsets_.clear();
    char_offsets_.push_back(0);
    for (size_t i = 0; i < text.size();) {
        auto c = static_cast<unsigned char>(text[i]);
        int len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += static_cast<size_t>(len);
        char_offsets_.push_back(static_cast<int>(i));
    }
    total_chars_     = static_cast<int>(char_offsets_.size()) - 1;
    displayed_chars_ = 0;
    char_timer_      = 0.0f;
}

// ── Listener helpers ───────────────────────────────────────

void JrpgDialogueScene::addListener(Rml::Element* el, const Rml::String& event,
                                     bool capture) {
    if (!el) return;
    el->AddEventListener(event, listener_.get(), capture);
    registrations_.push_back({el, event, capture});
}

} // namespace learn::jrpg
