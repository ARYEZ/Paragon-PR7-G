#include "PluginEditor.h"

ParagonX1AudioProcessorEditor::ParagonX1AudioProcessorEditor (ParagonX1AudioProcessor& processorToEdit)
    : AudioProcessorEditor (processorToEdit),
      view (processorToEdit)
{
    setLookAndFeel (&look->lookAndFeel);

    addAndMakeVisible (view);   // the view already has its height (shorter with the rack hidden)

    // Keep the view's shape at any size the user drags to.
    setResizable (true, true);
    fitShape();
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto size = openingSize (display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (defaultWidth, defaultHeight + 80));
    setSize (size.x, size.x * view.getHeight() / MainView::width);

    // HIDE RACK / SHOW RACK: the window keeps its width and gets shorter or
    // taller with the view.
    view.onHeightChanged = [this]
    {
        setSize (getWidth(), juce::roundToInt ((float) getWidth() * (float) view.getHeight() / (float) MainView::width));
        fitShape();
    };
}

void ParagonX1AudioProcessorEditor::fitShape()
{
    // 16:9 with the rack, wider without it; 960 to 1920 wide either way.
    const auto heightAt = [this] (int width) { return width * view.getHeight() / MainView::width; };
    // The new shape goes in before the limits too: setting the limits
    // re-fits the window, which would squeeze it back to the old shape.
    const auto shape = (double) MainView::width / (double) view.getHeight();
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (shape);
    setResizeLimits (960, heightAt (960), 1920, heightAt (1920));
    getConstrainer()->setFixedAspectRatio (shape);
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
