#include "PluginEditor.h"

ParagonX1AudioProcessorEditor::ParagonX1AudioProcessorEditor (ParagonX1AudioProcessor& processorToEdit)
    : AudioProcessorEditor (processorToEdit),
      view (processorToEdit)
{
    setLookAndFeel (&look->lookAndFeel);

    view.setSize (MainView::width, MainView::height);
    addAndMakeVisible (view);

    // Keep the mockup's 16:9 shape at any size the user drags to.
    setResizable (true, true);
    setResizeLimits (960, 540, 1920, 1080);
    getConstrainer()->setFixedAspectRatio ((double) MainView::width / (double) MainView::height);
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto size = openingSize (display != nullptr ? display->userArea : juce::Rectangle<int> (defaultWidth, defaultHeight + 80));
    setSize (size.x, size.y);
}

juce::Point<int> ParagonX1AudioProcessorEditor::openingSize (juce::Rectangle<int> screen)
{
    // `screen` is the part above the taskbar (or below the menu bar). Leave
    // room for the DAW's window frame, title bar and plugin toolbar, which
    // sit above the plugin itself.
    const auto fitWidth = juce::jmin (screen.getWidth() - 40, (screen.getHeight() - 80) * MainView::width / MainView::height);
    const auto width = juce::jlimit (960, defaultWidth, fitWidth);
    return { width, width * MainView::height / MainView::width };
}

ParagonX1AudioProcessorEditor::~ParagonX1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ParagonX1AudioProcessorEditor::resized()
{
    // Scale the whole view rather than re-laying it out, so it looks the
    // same at every size. JUCE draws through the transform, so text and
    // shapes stay sharp.
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::width));
}
