#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Icons.h"

// The panels along the bottom of the PROCESSOR page, as in the mockup:
//
//   FACEPLATE (ui/Faceplate) | (the selected block's knobs) | EXPRESSION ASSIGN
//
// plus the SETTINGS page. (The RIG BROWSER and SELECTED RIG panels gave way
// to the FACEPLATE: they only repeated the PRESETS page and the unit's own
// display and SAVE buttons.)

// Dark rounded panel with a small grey title, shared by the dock panels.
namespace DockStyle
{
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area);
    void drawTitle (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);
}

//==============================================================================
// SETTINGS: tuner reference pitch and mute, the expression pedal's MIDI
// controller, the unit's finish, where rigs are kept, and what the plugin
// is built from.
class SettingsPage final : public juce::Component,
                           private juce::ChangeListener
{
public:
    explicit SettingsPage (ParagonX1AudioProcessor&);
    ~SettingsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    ParagonX1AudioProcessor& processor;
    juce::Slider reference { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton muteWhileTuning { "Mute the output while the tuner is on" };
    juce::AudioProcessorValueTreeState::ButtonAttachment muteAttachment;
    juce::ComboBox ccBox;
    juce::TextButton openFolder { "OPEN RIGS FOLDER" };
    juce::TextButton pearlWhite { "PEARL WHITE" }, pearlWhiteNeon { "WHITE + NEON" }, pearlBlack { "PEARL BLACK" };
    juce::Rectangle<int> tunerArea, expressionArea, finishArea, rigsArea, foldersArea, aboutArea;

    // AMP & CAB FOLDERS: where the NAM amps and cab IRs are, for each a
    // CHOOSE button and an x to forget it.
    struct FolderRow
    {
        juce::String label;
        juce::TextButton choose { "CHOOSE" }, forget { "x" };
        juce::Rectangle<int> labelArea, pathArea;
    };
    FolderRow folderRows[2];
    std::unique_ptr<juce::FileChooser> folderChooser;
    void chooseFolder (int row);

    void changeListenerCallback (juce::ChangeBroadcaster*) override { showFinish(); }
    void showFinish();
};
