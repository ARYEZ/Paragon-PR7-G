#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// CIPHER's controls on the rack. The rack's display is the selected slot's
// ENVELOPE EDITOR (a PR7 screen you draw on with the mouse: double-click to
// add a point, drag one to move it, double-click it to remove it, drag the
// small dot between two points up or down to bend that line), with the
// envelope's playhead and, with retriggering on, its START and loop markers
// to drag. ENV 1 / ENV 2 pick which envelope you draw. On the SEQ page the
// display is the sequencer's grid (click or drag to light steps). On the
// face: the ten slot keys (each showing its effect), MASTER and SEQ, then
// that page's controls.
//
// It covers the rack's display and its controls together (the faceplate
// draws only CIPHER's name above the display).
class CipherPanel final : public juce::Component,
                          private juce::Timer
{
public:
    explicit CipherPanel (ParagonX1AudioProcessor&);
    ~CipherPanel() override;

    void setAreas (juce::Rectangle<int> displayArea, juce::Rectangle<int> controlArea);

    // 0-9 a slot, 10 the master section, 11 the sequencer, 12 the macros.
    void showSlot (int slot);
    int getShownSlot() const noexcept { return shown; }
    static constexpr int masterPage = 10, seqPage = 11, macroPage = 12;
    // The MACRO page, for tests: pick a macro and an assignment (by its line).
    void selectMacro (int macro);
    void selectAssignment (int line);
    // Which envelope a slot page draws: ENV 1 (false) or ENV 2.
    void editEnvelope2 (bool second);

    // The envelope editor, for tests: its screen, and a point's place there (0-1 each way).
    juce::Rectangle<int> getEditorBounds() const;
    juce::Point<int> editorPointFor (juce::Point<float> position0to1) const;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    class Editor;
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String name, id;
        juce::Rectangle<float> label, screen;
    };
    struct Key
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
        juce::String name;
        juce::Rectangle<float> label;
    };
    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
        juce::String name;
        juce::Rectangle<float> label;
    };

    void timerCallback() override;
    void bind();   // attaches the controls to the shown page's parameters
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<Knob*>);
    float raw (const juce::String& id) const;
    juce::String slotId (const char* part) const { return ParamIDs::cipherSlot (juce::jmin (shown, 9) + 1, part); }
    effects::Cipher::Type shownType() const;
    void setChoice (const juce::String& id, int index);
    void setValue (const juce::String& id, float plain);
    bool isSlotPage() const noexcept { return shown < masterPage; }
    int editIndex() const noexcept { return juce::jmin (shown, 9) + (editSecond ? effects::Cipher::numSlots : 0); }
    void editRow (const std::function<juce::uint32 (juce::uint32)>& change);

    ParagonX1AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    std::unique_ptr<Editor> editor;
    int shown = 0, seqRow = 0;
    bool editSecond = false;

    Key onKey;                                       // CIPHER on
    std::array<juce::TextButton, 13> slotKeys;       // 1-10, MASTER, SEQ and MACRO
    // A slot's controls.
    Key slotOn, syncKey, retrigAudio, retrigMidi, randomStart, envLoop;
    Choice typeList, speedList, shapeList, modSourceList, modDestList;
    std::array<juce::TextButton, 6> optionKeys;      // 6 12 18 24, or the effect's modes (PAN BALANCE ROTATE...)
    std::array<juce::TextButton, 4> styleKeys;       // BOTH UP DOWN SNAP
    std::array<juce::TextButton, 2> envKeys;         // ENV 1, ENV 2
    Knob mainKnob, p1Knob, p2Knob, envKnob, rateKnob, glideKnob, stereoKnob, gainKnob, mixKnob, modKnob;
    // The master's.
    Key driveOn, driveAfter, compOn, bassOn, bassListen;
    Choice driveList, compList, lawList;
    Knob driveKnob, toneKnob, compressKnob, bassKnob, outGainKnob, outMixKnob;
    // The sequencer page's.
    Key seqOn, midiOn;
    Choice seqSpeedList, seqStartList, seqEndList, scaleList, rootList;
    juce::TextButton rotateLeft { "<" }, rotateRight { ">" }, clearKey { "CLEAR" }, randomKey { "RANDOM" };
    Knob trigLevelKnob, trigHpfKnob, trigHystKnob, trigHoldKnob, followAttackKnob, followHoldKnob, followDecayKnob, followGateKnob;

    // The macro page's.
    std::array<Knob, 8> macroKnobs;
    std::array<juce::TextButton, 8> macroKeys;
    juce::TextEditor nameEditor;
    Choice targetList;
    juce::TextButton addKey { "ADD" }, removeKey { "REMOVE" }, clearMacroKey { "CLEAR" };
    Knob depthKnob, curveKnob;   // the picked assignment's (not parameters)
    int editMacro = 0, editLine = -1;
    std::vector<ParagonX1AudioProcessor::CipherMacroAssignment> macroLines() const;   // the picked macro's assignments
    void changeAssignment (const std::function<void (ParagonX1AudioProcessor::CipherMacroAssignment&)>&);
    juce::Rectangle<float> nameLabel;

    juce::Rectangle<int> display, controls;
    juce::Rectangle<float> optionLabel, styleLabel, onLabel, retrigLabel, envLabel, rowLabel, trigLabel, followLabel;
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::String shownState;
    int laidOutType = -1;   // the type the keys were last laid out for
};
