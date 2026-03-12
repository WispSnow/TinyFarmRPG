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

class JrpgDialogueScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void update(float dt) override;
    void clean() override;

private:
    // --- Data types ---

    struct Choice {
        std::string label;
    };

    struct DialogueLine {
        std::string              speaker;
        int                      portrait_idx;
        std::string              text;
        std::vector<std::string> choices;
        std::vector<int>         choice_targets; // next line for each choice
        int                      next;           // -1 = end of conversation
    };

    // --- Logic ---

    void setupDataModel();
    void buildScript();
    void startLine(int index);
    void advanceDialogue();
    void skipTypewriter();
    void onTextComplete();
    void onChoiceSelected(int choice_idx);
    void showEndMessage();
    void setPortrait(int idx);
    void prepareTypewriter(const std::string& text);
    void addListener(Rml::Element* el, const Rml::String& event, bool capture = false);

    // --- Event listener ---

    class UIEventListener final : public Rml::EventListener {
    public:
        explicit UIEventListener(JrpgDialogueScene& scene) : scene_(scene) {}
        void ProcessEvent(Rml::Event& event) override;

    private:
        JrpgDialogueScene& scene_;
    };

    Rml::ElementDocument* doc_ = nullptr;
    Rml::DataModelHandle  model_handle_{};

    // Element refs
    Rml::Element* portrait_el_         = nullptr;
    Rml::Element* dialogue_window_el_  = nullptr;

    // Event listener
    std::unique_ptr<UIEventListener> listener_;
    struct ListenerReg {
        Rml::Element* el;
        Rml::String   event;
        bool          capture;
    };
    std::vector<ListenerReg> registrations_;

    // Dialogue script
    std::vector<DialogueLine> script_;
    int current_line_idx_ = -1;

    // Typewriter state
    std::string      current_text_;
    std::vector<int> char_offsets_;   // byte offset per UTF-8 character boundary
    int   total_chars_     = 0;
    int   displayed_chars_ = 0;
    float char_timer_      = 0.0f;
    static constexpr float kCharDelay = 0.035f; // seconds per character

    // Portrait state
    int current_portrait_ = -1;

    // Focus management
    bool focus_first_choice_ = false;

    // Data-model bound variables
    std::string speaker_name_;
    std::string displayed_text_;
    std::string hint_text_ = "Enter / Click: advance | Space: skip text";
    bool has_speaker_   = false;
    bool show_choices_  = false;
    bool show_continue_ = false;
    bool text_complete_  = false;
    std::vector<Choice> choices_;
};

} // namespace learn::jrpg
