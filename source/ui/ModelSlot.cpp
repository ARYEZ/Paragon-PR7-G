#include "ModelSlot.h"

#include "ParagonLookAndFeel.h"

ModelSlot::ModelSlot (const juce::String& titleText, const juce::String& wildcardToUse, const juce::String& hintText,
                      juce::AudioProcessorValueTreeState& parameters, const juce::String& powerParameterId)
    : title (titleText),
      wildcard (wildcardToUse),
      hint (hintText),
      powerAttachment (*parameters.getParameter (powerParameterId), [this] (float value) { powerOn = value > 0.5f; repaint(); }),
      previous (SlotWindow::makeArrow (true)),
      next (SlotWindow::makeArrow (false))
{
    previous->setTooltip ("Previous file in this folder");
    next->setTooltip ("Next file in this folder");
    clear.setTooltip ("Unload");
    clear.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);

    previous->onClick = [this] { step (-1); };
    next->onClick = [this] { step (1); };
    clear.onClick = [this] { if (onClear) onClear(); };

    for (auto* component : std::initializer_list<juce::Component*> { previous.get(), next.get(), &clear })
        addAndMakeVisible (component);

    powerAttachment.sendInitialUpdate();
    setStatus ({});
}

void ModelSlot::setStatus (const SlotStatus& newStatus)
{
    status = newStatus;
    const auto hasFile = status.file != juce::File();
    previous->setEnabled (hasFile);
    next->setEnabled (hasFile);
    clear.setVisible (hasFile && isMouseOver (true));

    // The full details (sample rate, IR length, path) show on hover.
    auto tip = (hasFile ? status.file.getFullPathName() : hint) + "\nClick the LED to switch the " + title.toLowerCase() + " on or off.";
    if (status.detail.isNotEmpty())
        tip << "\n" << status.detail;
    if (status.error.isNotEmpty())
        tip << "\n" << status.error;
    setTooltip (tip);
    repaint();
}

bool ModelSlot::matchesWildcard (const juce::File& file) const
{
    for (const auto& pattern : juce::StringArray::fromTokens (wildcard, ";", {}))
        if (file.getFileName().matchesWildcard (pattern, true))
            return true;
    return false;
}

void ModelSlot::chooseFile()
{
    // The chosen folder (or the loaded file's own folder inside it); with
    // no folder chosen, the loaded file's folder, or Documents.
    const auto library = libraryFolder ? libraryFolder() : juce::File();
    const auto loadedFolder = status.file != juce::File() ? status.file.getParentDirectory() : juce::File();
    const auto startFolder = library.isDirectory()
                               ? (loadedFolder.isAChildOf (library) || loadedFolder == library ? loadedFolder : library)
                               : loadedFolder != juce::File() ? loadedFolder
                                                              : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load " + title.toLowerCase(), startFolder, wildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (const auto file = fc.getResult(); file.existsAsFile() && onLoad)
                                  onLoad (file);
                          });
}

void ModelSlot::step (int direction)
{
    if (status.file == juce::File() || ! onLoad)
        return;

    juce::Array<juce::File> siblings;
    for (const auto& file : status.file.getParentDirectory().findChildFiles (juce::File::findFiles, false))
        if (matchesWildcard (file))
            siblings.add (file);

    if (siblings.isEmpty())
        return;

    std::sort (siblings.begin(), siblings.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    // Wrap around at either end. If the current file is gone, start from the first.
    const auto index = siblings.indexOf (status.file);
    const auto count = siblings.size();
    const auto nextIndex = index < 0 ? 0 : (index + direction + count) % count;
    onLoad (siblings[nextIndex]);
}

void ModelSlot::paint (juce::Graphics& g)
{
    SlotWindow::drawTitle (g, layout, title);
    SlotWindow::drawRingLed (g, ledCentre, powerOn);

    const auto hasFile = status.file != juce::File();
    SlotWindow::drawButton (g, layout.button, buttonHover, dragHover);

    // Under the block: what's loaded, or what to do, or what went wrong.
    const auto isError = status.error.isNotEmpty() && ! status.loading;
    const auto caption = status.loading ? juce::String ("Loading...")
                       : isError         ? status.error
                       : hasFile         ? status.file.getFileNameWithoutExtension()
                                         : hint;
    SlotWindow::drawCaption (g, layout.caption, caption, isError);
}

void ModelSlot::resized()
{
    layout = SlotWindow::layout (getLocalBounds());

    // Power LED just right of the title text, as in the mockup.
    const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (juce::FontOptions (14.0f, juce::Font::bold), title);
    ledCentre = { (float) layout.title.getCentreX() + (float) titleWidth / 2.0f + 13.0f, (float) layout.title.getCentreY() };
    clear.setBounds (layout.corner.withSizeKeepingCentre (20, 18));
    previous->setBounds (layout.previous);
    next->setBounds (layout.next);
}

void ModelSlot::mouseEnter (const juce::MouseEvent&)
{
    clear.setVisible (status.file != juce::File());
}

void ModelSlot::mouseMove (const juce::MouseEvent& event)
{
    const auto hover = layout.button.contains (event.getPosition());
    if (hover != buttonHover)
    {
        buttonHover = hover;
        repaint (layout.button.expanded (4));
    }
}

void ModelSlot::mouseExit (const juce::MouseEvent&)
{
    // Child components (the arrows, the LED) also count as "exiting" us.
    if (! isMouseOver (true))
        clear.setVisible (false);
    buttonHover = false;
    repaint (layout.button.expanded (4));
}

void ModelSlot::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;
    if (juce::Rectangle<float> (22.0f, 22.0f).withCentre (ledCentre).contains (event.position))
        powerAttachment.setValueAsCompleteGesture (powerOn ? 0.0f : 1.0f);
    else if (layout.button.contains (event.getPosition()))
        chooseFile();
    else if (layout.title.contains (event.getPosition()) && onTitleClicked)
        onTitleClicked();
}

bool ModelSlot::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && matchesWildcard (juce::File (files[0]));
}

void ModelSlot::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void ModelSlot::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void ModelSlot::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    if (onLoad && ! files.isEmpty())
        onLoad (juce::File (files[0]));
}
