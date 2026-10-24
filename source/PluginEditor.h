#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/MainView.h"
#include "ui/ParagonLookAndFeel.h"

// The plugin window. All the content lives in MainView, laid out on a fixed
// 1600 x 900 canvas; the editor just scales it to whatever size the window
// is, keeping the mockup's 16:9 shape.
class ParagonX1AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ParagonX1AudioProcessorEditor (ParagonX1AudioProcessor&);
    ~ParagonX1AudioProcessorEditor() override;

    void resized() override;

    // Opens at up to 1824 x 1026: the largest 16:9 size that fits the
    // screen above the taskbar, with room for the DAW's own window frame
    // (about 1706 x 959 on a 1080p screen); drag to resize.
    static constexpr int defaultWidth = 1824;
    static constexpr int defaultHeight = 1026;

    // The size to open at, given the screen's area above the taskbar (logical pixels).
    static juce::Point<int> openingSize (juce::Rectangle<int> screen);

private:
    juce::SharedResourcePointer<SharedParagonLookAndFeel> look;   // declared first: outlives the view
    MainView view;
    juce::TooltipWindow tooltips { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParagonX1AudioProcessorEditor)
};
