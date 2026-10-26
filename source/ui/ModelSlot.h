#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "SlotWindow.h"

// The AMP or CABINET window on the pedal: a title with a power LED, a black
// button showing the loaded file (click it to choose a file), arrows that
// step through the other files in the same folder, and a clear button.
// Files can also be dragged onto the window.
class ModelSlot final : public juce::Component,
                        public juce::SettableTooltipClient,
                        public juce::FileDragAndDropTarget
{
public:
    ModelSlot (const juce::String& title, const juce::String& wildcard,
               const juce::String& hint, juce::AudioProcessorValueTreeState& parameters, const juce::String& powerParameterId);

    // Called when the title is clicked (to show this block's settings).
    std::function<void()> onTitleClicked;

    std::function<void (const juce::File&)> onLoad;
    std::function<void()> onClear;

    // The folder chosen on the SETTINGS page for this kind of file, if any:
    // the file browser opens there.
    std::function<juce::File()> libraryFolder;

    void setStatus (const SlotStatus& newStatus);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void chooseFile();
    void step (int direction);
    bool matchesWildcard (const juce::File& file) const;

    juce::String title, wildcard, hint;
    SlotStatus status;
    bool dragHover = false, buttonHover = false;

    // The power LED is painted; clicks on it toggle the parameter.
    juce::ParameterAttachment powerAttachment;
    bool powerOn = true;
    std::unique_ptr<juce::Button> previous, next;
    juce::TextButton clear { "x" };
    SlotWindow::Layout layout;
    juce::Point<float> ledCentre;
    std::unique_ptr<juce::FileChooser> chooser;
};
