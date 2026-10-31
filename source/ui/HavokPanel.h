#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/PerformanceControls.h"

// HAVOK's controls on the rack: the TOUCH PAD itself in the rack's display
// (a PR7 screen you play with the mouse), beside it the display the
// original has, the program number in big digits, or the position you're
// touching as 1.1 to 9.9 (5.5 is the centre), with the program's name and
// what X and Y do; on the rest of the face FX ON, HOLD, REC (for programs
// 51-60), the six PROGRAM MAP keys, the PROGRAM knob and the program list.
//
// It covers the rack's display and its controls together (the faceplate
// draws only HAVOK's name above the display).
class HavokPanel final : public juce::Component,
                         private juce::Timer
{
public:
    explicit HavokPanel (ParagonX1AudioProcessor&);
    ~HavokPanel() override;

    // `displayArea` and `controlArea`: the rack's, in this panel's parent's coordinates.
    void setAreas (juce::Rectangle<int> displayArea, juce::Rectangle<int> controlArea);

    // The touch pad, for tests: 0 to 1 across and up.
    juce::Rectangle<int> getPadBounds() const;
    void touchPad (juce::Point<float> position0to1);
    void releasePad();
    // A PROGRAM MAP key (1-6): a press recalls it, holding it two seconds stores it.
    void recallMap (int key);
    void storeMap (int key);
    // What the display shows now ("51", "3.8"), for tests.
    juce::String getDisplayText() const;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    class Pad;
    void timerCallback() override;
    float raw (const juce::String& id) const;
    void setParameter (const juce::String& id, float plainValue);
    int program() const;

    ParagonX1AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    std::unique_ptr<Pad> pad;

    juce::ToggleButton onKey, holdKey;
    juce::AudioProcessorValueTreeState::ButtonAttachment onAttachment, holdAttachment;
    juce::TextButton recKey { "REC" };
    std::array<juce::TextButton, 6> mapKeys;
    std::array<std::unique_ptr<HoldDetector>, 6> mapHolds;
    juce::Slider programKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment programKnobAttachment;
    juce::ComboBox programList;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> programListAttachment;   // made once the list has its items
    juce::ComboBox soundList;   // the scratch programs' SOUND, where REC is for the sample programs
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundListAttachment;

    juce::Rectangle<int> display, controls;            // in this panel's own coordinates
    juce::Rectangle<float> screenArea, onLabel, holdLabel, mapLabel, knobLabel, infoArea, soundLabel;
    mutable std::vector<juce::Rectangle<float>> glasses;
    juce::String shownState;   // what the panel last drew (the timer repaints when it changes)
    int shownProgram = -1;
    juce::uint32 programShownSince = 0;   // a program with HOLD shows its number first, then the held position
    int blink = 0;
};
