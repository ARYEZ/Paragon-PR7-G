#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// CIPHER's controls on the rack. The rack's display is the selected slot's
// ENVELOPE EDITOR (a PR7 screen you draw on with the mouse: double-click to
// add a point, drag one to move it, double-click it to remove it, drag the
// small dot between two points up or down to bend that line), with the
// envelope's playhead and the main knob as the envelope moves it. On the
// face: the ten slot keys (each showing its effect) and MASTER, then for
// a slot its TYPE, steepness (or Pan's mode), ENV MOD STYLE, SYNC and
// SPEED, SHAPE, and its knobs; for MASTER the drive, compressor, CLEAN BASS
// and output.
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

    // 0-9 a slot, 10 the master section.
    void showSlot (int slot);
    int getShownSlot() const noexcept { return shown; }
    static constexpr int masterPage = 10;

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
    void bind();   // attaches the controls to the shown slot's (or the master's) parameters
    void layoutRow (juce::Rectangle<int> row, std::initializer_list<Knob*>);
    float raw (const juce::String& id) const;
    juce::String slotId (const char* part) const { return ParamIDs::cipherSlot (juce::jmin (shown, 9) + 1, part); }
    effects::Cipher::Type shownType() const;
    void setChoice (const juce::String& id, int index);

    ParagonX1AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    std::unique_ptr<Editor> editor;
    int shown = 0;

    Key onKey;                                       // CIPHER on
    std::array<juce::TextButton, 11> slotKeys;       // 1-10 and MASTER
    // A slot's controls.
    Key slotOn, syncKey;
    Choice typeList, speedList, shapeList;
    std::array<juce::TextButton, 4> optionKeys;      // 6 12 18 24, or PAN BALANCE ROTATE
    std::array<juce::TextButton, 3> styleKeys;       // BOTH UP DOWN
    Knob mainKnob, p1Knob, p2Knob, envKnob, rateKnob, glideKnob, stereoKnob, gainKnob, mixKnob;
    // The master's.
    Key driveOn, driveAfter, compOn, bassOn, bassListen;
    Choice driveList, compList, lawList;
    Knob driveKnob, toneKnob, compressKnob, bassKnob, outGainKnob, outMixKnob;

    juce::Rectangle<int> display, controls;
    juce::Rectangle<float> optionLabel, styleLabel, onLabel;
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::String shownState;
};
