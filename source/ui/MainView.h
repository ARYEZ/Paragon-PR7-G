#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/DockPanels.h"
#include "ui/EffectPanel.h"
#include "ui/Faceplate.h"
#include "ui/Finish.h"
#include "ui/GraphicEqPanel.h"
#include "ui/HavokPanel.h"
#include "ui/Footswitch.h"
#include "ui/LedDisplay.h"
#include "ui/LevelMeter.h"
#include "ui/ModelSlot.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/PedalBody.h"
#include "ui/PerformanceControls.h"
#include "ui/RigBrowser.h"
#include "ui/RigSlot.h"
#include "ui/SignalChain.h"

// A side strip from the mockup: a title, a gain knob with its value
// underneath, and a level meter.
class GainColumn final : public juce::Component
{
public:
    GainColumn (const juce::String& title, juce::AudioProcessorValueTreeState& parameters,
                const juce::String& parameterId, LevelMeterSource& meterSource);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Slider knob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    LevelMeter meter;
};

// A solid triangle printed on the unit beside the screen (the rig up and
// down arrows), in the finish's ink: like juce::ArrowButton, whose colour
// can't change after it's made.
class InkArrowButton final : public juce::Button
{
public:
    InkArrowButton (const juce::String& name, float direction);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Path path;
};

// Everything in the plugin window, laid out on a fixed 1600 x 900 canvas.
// That's the mockup image (1672 x 941) scaled by 0.957, so positions are
// written in the mockup's own pixel coordinates and scaled: what you see in
// the mockup at (x, y) is drawn at (x, y) * 0.957. The editor then scales
// the whole view to the window size, staying sharp (everything is vectors).
//
//   header:  logo | PROCESSOR PRESETS SETTINGS | < rig > | BPM TAP | undo redo save settings
//   middle:  INPUT | the pedal unit with its expression pedal | OUTPUT
//   dock:    RIG BROWSER | SELECTED RIG | TAGS + RIG SUMMARY (or a block's knobs) | EXPRESSION ASSIGN
//   bottom:  SIGNAL CHAIN
class MainView final : public juce::Component,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit MainView (ParagonX1AudioProcessor&);
    ~MainView() override;

    static constexpr int width = 1600;
    static constexpr int height = 900;

    // HIDE RACK takes the rack's strip out (264 mockup pixels, from the
    // rack's top to the SIGNAL CHAIN's) and the chain moves up under the
    // floor unit, so the canvas, and the window, get shorter.
    static constexpr int rackHeight = 253;
    static constexpr int heightFor (bool rackShown) { return rackShown ? height : height - rackHeight; }

    // Told after the rack is shown or hidden and the view has its new
    // height, so the editor can resize the window to match.
    std::function<void()> onHeightChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // The blocks in signal-chain order; footswitches, the chain strip and
    // the summary cards select these to show their knobs.
    enum Block { pitchBlock, compBlock, filterPitchBlock, driveBlock, ampBlock, cabBlock, eqBlock, glitchBlock, modBlock, mod2Block, delayBlock, reverbBlock, loFiBlock, havokBlock, numBlocks };

    enum class Page { processor, presets, settings };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutDock();    // the rack: the faceplate, the selected block's controls on it, EXPRESSION ASSIGN
    void applyFinish();   // colours the printed labels and redraws the unit in the current finish
    void applyRackView(); // shows or hides the rack and sets the view's height to match
    void timerCallback() override;

    void selectBlock (int block);
    void refreshPerformance();
    void toggleTuner();
    void tap();
    void showPage (Page);
    void refreshRig();
    void saveRig();
    juce::String valueText (const char* parameterId) const;
    float rawValue (const char* parameterId) const;

    ParagonX1AudioProcessor& audioProcessor;

    // Header
    IconButton processorTab { "Pedalboard and effects", Icons::Type::processor, "PROCESSOR" },
               presetsTab { "Browse, save and organise rigs", Icons::Type::presets, "PRESETS" },
               settingsTab { "Tuner, MIDI and other settings", Icons::Type::settings, "SETTINGS" };
    IconButton previousRig { "Previous rig", Icons::Type::chevronLeft }, nextRig { "Next rig", Icons::Type::chevronRight };
    juce::TextButton rigNameButton, tempoButton;
    IconButton tapButton { "Tap the tempo for the delay. Hold to open the tuner.", Icons::Type::tap, "TAP" },
               undoButton { "Undo (Ctrl+Z)", Icons::Type::undo }, redoButton { "Redo (Ctrl+Shift+Z)", Icons::Type::redo },
               saveButton { "Save the rig (as a new rig if it's a factory rig or not saved yet)", Icons::Type::save },
               settingsButton { "Settings", Icons::Type::settings };
    HoldDetector headerTapHold, pedalTapHold;

    GainColumn inputColumn, outputColumn;

    // The pedal unit and what sits on it
    PedalBody pedal;
    LedDisplay display;
    InkArrowButton rigUp { "Previous rig", 0.75f }, rigDown { "Next rig", 0.25f };
    juce::TextButton pedalSave { "SAVE" }, pedalSaveAs { "SAVE AS..." }, bypassButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    // The 2 x 3 button cluster: UNDO REDO BYPASS / TUNER MUTE TAP
    juce::TextButton undoPedal, redoPedal, tunerPedal, mutePedal, tapPedal;
    juce::AudioProcessorValueTreeState::ButtonAttachment tunerAttachment, muteAttachment;
    juce::Label undoLabel { {}, "UNDO" }, redoLabel { {}, "REDO" }, bypassLabel { {}, "BYPASS" },
                tunerLabel { {}, "TUNER" }, muteLabel { {}, "MUTE" }, tapLabel { {}, "TAP" }, holdLabel { {}, "HOLD TO TUNER" };

    juce::Slider rigLevelKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox },
                 presenceKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment rigLevelAttachment, presenceAttachment;
    juce::Label rigLevelLabel { {}, "RIG LEVEL" }, presenceLabel { {}, "PRESENCE" };
    ModelSlot ampSlot, cabSlot;
    RigSlot rigSlot;
    std::array<std::unique_ptr<Footswitch>, 4> footswitches;

    // Expression pedal and tuner
    ExpressionPedal expressionPedal;
    TunerReader tunerReader;
    LedDisplay::Contents::Tuner tunerState;
    int ticks = 0;

    // Dock and signal chain
    Faceplate faceplate;   // the selected block as hardware, beside its knobs
    ExpressionPanel expressionPanel;
    std::array<std::unique_ptr<EffectPanel>, numBlocks> panels;   // empty at eqBlock and havokBlock: they have their own panels
    GraphicEqPanel eqPanel;
    HavokPanel havokPanel;   // the touch pad, in the rack's display, and its keys
    juce::TextButton closePanel { "GRAPHIC EQ" };   // back to the EQ, the dock's default
    juce::TextButton rackButton;                     // HIDE RACK / SHOW RACK, on the SIGNAL CHAIN strip
    SignalChain signalChain;
    int selectedBlock = eqBlock;

    // Other pages
    RigBrowser rigBrowser;
    SettingsPage settingsPage;
    std::vector<juce::Component*> processorPage;
    Page page = Page::processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
