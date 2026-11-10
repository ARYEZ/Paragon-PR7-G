#include "CipherPanel.h"

#include "Parameters.h"
#include "ui/Finish.h"
#include "ui/LedDisplay.h"
#include "ui/ParagonLookAndFeel.h"

using effects::Cipher;

namespace
{
    const auto orange = LedDisplay::ledOrange;
    constexpr float screenSize = 0.45f;

    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centredLeft)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    void screenCombo (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        box.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        box.setColour (juce::ComboBox::textColourId, orange);
        box.setColour (juce::ComboBox::arrowColourId, orange.withAlpha (0.6f));
    }

    // Short names for the slot keys.
    juce::String shortName (Cipher::Type type)
    {
        const char* names[] = { "-", "LAD LP", "LAD BP", "LAD HP", "DIG LP", "DIG BP", "DIG HP", "101 LP", "101 BP", "101 HP",
                                "M20 LP", "M20 BP", "M20 HP", "NOTCH", "DUAL", "FORM", "BELL", "LO SH", "HI SH", "GAIN", "PAN" };
        return names[juce::jlimit (0, (int) std::size (names) - 1, (int) type)];
    }
}

//==============================================================================
// The ENVELOPE EDITOR: the slot's envelope on a PR7 screen, with a grid of
// sixteen, the playhead, and the points to drag. Snaps to the grid across
// unless SHIFT is held, as the original's SNAP does.
class CipherPanel::Editor final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    explicit Editor (CipherPanel& ownerPanel) : owner (ownerPanel)
    {
        setTooltip ("The envelope: double-click to add a point, drag one to move it (SHIFT: off the grid), double-click it to "
                    "remove it; drag the small dot between two points up or down to bend that line");
    }

    juce::Rectangle<float> plotArea() const
    {
        auto area = getLocalBounds().toFloat().reduced (getWidth() * 0.035f, getHeight() * 0.06f);
        area.removeFromTop (16.0f);
        return area.reduced (6.0f, 4.0f);
    }

    juce::Point<float> toScreen (float x, float y) const
    {
        const auto a = plotArea();
        return { a.getX() + a.getWidth() * x, a.getBottom() - a.getHeight() * y };
    }

    juce::Point<float> fromScreen (juce::Point<float> p) const
    {
        const auto a = plotArea();
        return { juce::jlimit (0.0f, 1.0f, (p.x - a.getX()) / a.getWidth()), juce::jlimit (0.0f, 1.0f, (a.getBottom() - p.y) / a.getHeight()) };
    }

    void paint (juce::Graphics& g) override
    {
        glass = LedDisplay::paintScreen (g, getLocalBounds().toFloat(), screenSize);
        if (owner.shown == CipherPanel::masterPage)
        {
            paintChain (g);
            LedDisplay::paintGlassOverlay (g, glass, screenSize);
            return;
        }
        const auto a = plotArea();
        const auto slot = juce::jmin (owner.shown, 9);
        const auto type = owner.shownType();
        const auto& envelope = owner.processor.getCipherEnvelope (slot);

        // The title line: the slot, its effect and the main knob's value as moved.
        auto title = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.06f).removeFromTop (14.0f);
        g.setFont (font (10.0f));
        g.setColour (orange);
        g.drawText ("SLOT " + juce::String (slot + 1) + "  " + Cipher::typeNames[(int) type].toUpperCase(), title, juce::Justification::centredLeft, false);
        const auto moved = owner.processor.getCipher().getMovedMain (slot);
        const auto labels = Cipher::labelsFor (type);
        if (type != Cipher::Type::off)
            g.drawText (juce::String (labels.main) + " " + Cipher::mainText (type, moved), title, juce::Justification::centredRight, false);

        // The grid.
        for (int i = 1; i < 16; ++i)
        {
            g.setColour (orange.withAlpha (i % 4 == 0 ? 0.14f : 0.06f));
            const auto x = a.getX() + a.getWidth() * (float) i / 16.0f;
            g.fillRect (juce::Rectangle<float> (x - 0.5f, a.getY(), 1.0f, a.getHeight()));
        }
        for (int i = 1; i < 4; ++i)
        {
            g.setColour (orange.withAlpha (i == 2 ? 0.14f : 0.06f));
            const auto y = a.getY() + a.getHeight() * (float) i / 4.0f;
            g.fillRect (juce::Rectangle<float> (a.getX(), y - 0.5f, a.getWidth(), 1.0f));
        }

        // The envelope, filled underneath like a lit LED band.
        juce::Path line, fill;
        const int steps = juce::jmax (64, (int) a.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto x = (float) i / (float) steps;
            const auto p = toScreen (x, Cipher::envelopeAt (envelope, x));
            if (i == 0)
            {
                line.startNewSubPath (p);
                fill.startNewSubPath (a.getX(), a.getBottom());
            }
            else
                line.lineTo (p);
            fill.lineTo (p);
        }
        fill.lineTo (a.getRight(), a.getBottom());
        fill.closeSubPath();
        const auto active = type != Cipher::Type::off;
        g.setColour (orange.withAlpha (active ? 0.12f : 0.05f));
        g.fillPath (fill);
        g.setColour (orange.withAlpha (active ? 0.95f : 0.4f));
        g.strokePath (line, juce::PathStrokeType (1.6f));

        // The points and, between each pair, the dot that bends the line.
        for (size_t i = 0; i < envelope.size(); ++i)
        {
            const auto p = toScreen (envelope[i].x, envelope[i].y);
            g.setColour (orange);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            if (i + 1 < envelope.size() && envelope[i + 1].x - envelope[i].x > 0.02f)
            {
                g.setColour (orange.withAlpha (0.55f));
                g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (handle ((int) i)), 1.0f);
            }
        }

        // The playhead.
        if (active && owner.raw (ParamIDs::cipherOn) > 0.5f)
        {
            const auto x = a.getX() + a.getWidth() * owner.processor.getCipher().getPhase (slot);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (x - 0.75f, a.getY(), 1.5f, a.getHeight()));
        }
        LedDisplay::paintGlassOverlay (g, glass, screenSize);
    }

    // MASTER: the whole chain, in order: the drive where it is, the ten
    // slots (dim when empty or off), the compressor, CLEAN BASS.
    void paintChain (juce::Graphics& g)
    {
        auto area = glass.reduced (glass.getWidth() * 0.05f, glass.getHeight() * 0.08f);
        g.setFont (font (10.0f));
        g.setColour (orange);
        g.drawText ("MASTER  SIGNAL FLOW", area.removeFromTop (16.0f), juce::Justification::centredLeft, false);
        area.removeFromTop (4.0f);
        const auto on = [this] (const char* id) { return owner.raw (id) > 0.5f; };
        juce::StringArray lines;
        std::vector<bool> lit;
        const auto add = [&] (const juce::String& text, bool active) { lines.add (text); lit.push_back (active); };
        const auto driveText = "DRIVE  " + Cipher::driveNames[juce::roundToInt (owner.raw (ParamIDs::cipherDriveType))].toUpperCase();
        add ("IN", true);
        if (! on (ParamIDs::cipherDriveAfter))
            add (driveText, on (ParamIDs::cipherDriveOn));
        for (int i = 0; i < Cipher::numSlots; ++i)
        {
            const auto type = (Cipher::Type) juce::roundToInt (owner.raw (ParamIDs::cipherSlot (i + 1, "Type")));
            add (juce::String (i + 1) + "  " + Cipher::typeNames[(int) type].toUpperCase(),
                 type != Cipher::Type::off && owner.raw (ParamIDs::cipherSlot (i + 1, "On")) > 0.5f);
        }
        if (on (ParamIDs::cipherDriveAfter))
            add (driveText, on (ParamIDs::cipherDriveOn));
        add ("COMP  " + Cipher::compNames[juce::roundToInt (owner.raw (ParamIDs::cipherComp))].toUpperCase(), on (ParamIDs::cipherCompOn));
        add ("CLEAN BASS  " + juce::String (juce::roundToInt (owner.raw (ParamIDs::cipherCleanBassHz))) + " HZ", on (ParamIDs::cipherCleanBassOn));
        add ("OUT", true);
        // Two columns.
        const auto perColumn = (lines.size() + 1) / 2;
        const auto rowHeight = juce::jmin (14.0f, area.getHeight() / (float) perColumn);
        for (int i = 0; i < lines.size(); ++i)
        {
            const auto column = i / perColumn, row = i % perColumn;
            const juce::Rectangle<float> cell (area.getX() + area.getWidth() * 0.5f * (float) column, area.getY() + rowHeight * (float) row,
                                               area.getWidth() * 0.5f, rowHeight);
            g.setColour (orange.withAlpha (lit[(size_t) i] ? 1.0f : 0.3f));
            g.setFont (font (9.5f));
            g.drawText (lines[i], cell, juce::Justification::centredLeft, false);
        }
    }

    juce::Point<float> handle (int segment) const
    {
        const auto& envelope = owner.processor.getCipherEnvelope (juce::jmin (owner.shown, 9));
        const auto mid = (envelope[(size_t) segment].x + envelope[(size_t) segment + 1].x) * 0.5f;
        return toScreen (mid, Cipher::envelopeAt (envelope, mid));
    }

    int pointAt (juce::Point<float> position) const
    {
        const auto& envelope = owner.processor.getCipherEnvelope (juce::jmin (owner.shown, 9));
        for (int i = (int) envelope.size(); --i >= 0;)
            if (toScreen (envelope[(size_t) i].x, envelope[(size_t) i].y).getDistanceFrom (position) < 7.0f)
                return i;
        return -1;
    }

    int handleAt (juce::Point<float> position) const
    {
        const auto& envelope = owner.processor.getCipherEnvelope (juce::jmin (owner.shown, 9));
        for (int i = 0; i + 1 < (int) envelope.size(); ++i)
            if (envelope[(size_t) i + 1].x - envelope[(size_t) i].x > 0.02f && handle (i).getDistanceFrom (position) < 6.0f)
                return i;
        return -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = pointAt (e.position);
        bending = dragging < 0 ? handleAt (e.position) : -1;
        changed = false;
        if (bending >= 0)
            bendStart = owner.processor.getCipherEnvelope (juce::jmin (owner.shown, 9))[(size_t) bending].curve;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto slot = juce::jmin (owner.shown, 9);
        auto envelope = owner.processor.getCipherEnvelope (slot);
        if (dragging >= 0 && dragging < (int) envelope.size())
        {
            auto p = fromScreen (e.position);
            if (! e.mods.isShiftDown())
                p.x = std::round (p.x * 16.0f) / 16.0f;
            // A point stays between its neighbours.
            const auto lowest = dragging > 0 ? envelope[(size_t) dragging - 1].x : 0.0f;
            const auto highest = dragging + 1 < (int) envelope.size() ? envelope[(size_t) dragging + 1].x : 1.0f;
            envelope[(size_t) dragging].x = juce::jlimit (lowest, highest, p.x);
            envelope[(size_t) dragging].y = p.y;
        }
        else if (bending >= 0 && bending + 1 < (int) envelope.size())
        {
            // Dragging up bows the line up, down bows it down.
            const auto rising = envelope[(size_t) bending + 1].y >= envelope[(size_t) bending].y;
            const auto dy = (float) -e.getDistanceFromDragStartY() / 80.0f;
            envelope[(size_t) bending].curve = juce::jlimit (-1.0f, 1.0f, bendStart + (rising ? dy : -dy));
        }
        else
            return;
        changed = true;
        owner.processor.setCipherEnvelope (slot, envelope, false);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (changed)
            owner.processor.setCipherEnvelope (juce::jmin (owner.shown, 9), owner.processor.getCipherEnvelope (juce::jmin (owner.shown, 9)), true);
        dragging = bending = -1;
        changed = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto slot = juce::jmin (owner.shown, 9);
        auto envelope = owner.processor.getCipherEnvelope (slot);
        if (const auto hit = pointAt (e.position); hit >= 0)
        {
            if (envelope.size() > 1)
                envelope.erase (envelope.begin() + hit);
        }
        else
        {
            auto p = fromScreen (e.position);
            if (! e.mods.isShiftDown())
                p.x = std::round (p.x * 16.0f) / 16.0f;
            envelope.push_back ({ p.x, p.y, 0.0f });
        }
        owner.processor.setCipherEnvelope (slot, envelope, true);
        repaint();
    }

private:
    CipherPanel& owner;
    juce::Rectangle<float> glass;
    int dragging = -1, bending = -1;
    float bendStart = 0.0f;
    bool changed = false;
};

//==============================================================================
CipherPanel::CipherPanel (ParagonX1AudioProcessor& processorToUse)
    : processor (processorToUse), parameters (processorToUse.getParameters())
{
    editor = std::make_unique<Editor> (*this);
    addAndMakeVisible (*editor);

    const auto setUpKey = [this] (Key& key, const char* name, const char* tip)
    {
        key.name = name;
        key.button.getProperties().set (ParagonLookAndFeel::rackKeyProperty, true);
        key.button.setTooltip (tip);
        addAndMakeVisible (key.button);
    };
    setUpKey (onKey, "FX ON", "FX ON: switch CIPHER on or off");
    onKey.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters, ParamIDs::cipherOn, onKey.button);
    setUpKey (slotOn, "SLOT", "This slot on or off (it keeps its settings)");
    setUpKey (syncKey, "SYNC", "SYNC: the envelope's speed in notes, locked to the song; off, in hertz (RATE)");
    setUpKey (driveOn, "DRIVE", "The master drive on or off");
    setUpKey (driveAfter, "FX > DRIVE", "On: the drive after the ten slots (Effects -> Drive). Off: before them (Drive -> Effects)");
    setUpKey (compOn, "COMP", "The master compressor on or off");
    setUpKey (bassOn, "CLEAN BASS", "CLEAN BASS: below its frequency the sound passes untouched by CIPHER");
    setUpKey (bassListen, "LISTEN", "LISTEN: hear only the untouched lows, to set CLEAN BASS's frequency");

    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        auto& key = slotKeys[(size_t) i];
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setClickingTogglesState (false);
        key.setTooltip (i < 10 ? "Slot " + juce::String (i + 1) + ": its effect, envelope and knobs" : juce::String ("MASTER: the drive, compressor, CLEAN BASS and output"));
        key.onClick = [this, i] { showSlot (i); };
        addAndMakeVisible (key);
    }

    const auto setUpChoice = [this] (Choice& choice, const char* name, const char* tip)
    {
        choice.name = name;
        screenCombo (choice.box);
        choice.box.setTooltip (tip);
        addAndMakeVisible (choice.box);
    };
    setUpChoice (typeList, "TYPE", "The slot's effect");
    typeList.box.addItemList (Cipher::typeNames, 1);
    setUpChoice (speedList, "SPEED", "SPEED: how long one pass of the envelope takes, in notes");
    speedList.box.addItemList (Cipher::speedNames, 1);
    setUpChoice (shapeList, "SHAPE", "Load a shape into the envelope (replaces the points)");
    shapeList.box.addItemList (Cipher::shapeNames, 1);
    shapeList.box.setTextWhenNothingSelected ("LOAD...");
    shapeList.box.onChange = [this]
    {
        if (const auto chosen = shapeList.box.getSelectedItemIndex(); chosen >= 0 && shown < 10)
        {
            processor.setCipherEnvelope (shown, Cipher::shape (chosen), true);
            shapeList.box.setSelectedId (0, juce::dontSendNotification);
            editor->repaint();
        }
    };
    setUpChoice (driveList, "TYPE", "The master drive's type");
    driveList.box.addItemList (Cipher::driveNames, 1);
    setUpChoice (compList, "TYPE", "The master compressor: Squash, Maximizer, Multiband or Overkill");
    compList.box.addItemList (Cipher::compNames, 1);
    setUpChoice (lawList, "MIX LAW", "How MIX blends at half way: -6 dB suits filters, -3 dB echoes and reverbs, 0 dB keeps the dry up");
    lawList.box.addItemList (Cipher::mixLawNames, 1);

    for (int i = 0; i < (int) optionKeys.size(); ++i)
    {
        auto& key = optionKeys[(size_t) i];
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.onClick = [this, i] { setChoice (slotId ("Option"), i); };
        addAndMakeVisible (key);
    }
    const char* styles[] = { "BOTH", "UP", "DOWN" };
    for (int i = 0; i < (int) styleKeys.size(); ++i)
    {
        auto& key = styleKeys[(size_t) i];
        key.setButtonText (styles[i]);
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setTooltip ("ENV MOD STYLE: the envelope moves the main knob both ways, only up, or only down");
        key.onClick = [this, i] { setChoice (slotId ("Style"), i); };
        addAndMakeVisible (key);
    }

    const auto setUpKnob = [this] (Knob& knob, const char* name, const char* tip)
    {
        knob.name = name;
        knob.slider.getProperties().set (ParagonLookAndFeel::pedalKnobProperty, true);
        knob.slider.setPopupDisplayEnabled (true, true, nullptr);
        knob.slider.setTooltip (tip);
        addAndMakeVisible (knob.slider);
    };
    setUpKnob (mainKnob, "MAIN", "The main knob (cutoff, frequency, vowel, gain or position): the envelope moves it");
    setUpKnob (p1Knob, "KNOB 1", "The effect's first knob");
    setUpKnob (p2Knob, "KNOB 2", "The effect's second knob");
    setUpKnob (envKnob, "ENV AMT", "ENV AMOUNT: how far the envelope moves the main knob (left of centre: the other way)");
    setUpKnob (rateKnob, "RATE", "RATE: the envelope's speed in hertz, with SYNC off");
    setUpKnob (glideKnob, "GLIDE", "GLIDE: smooths the envelope's steps on the way to the main knob");
    setUpKnob (stereoKnob, "STEREO", "STEREO: moves the left and right sides' main knob apart");
    setUpKnob (gainKnob, "GAIN", "The slot's output level");
    setUpKnob (mixKnob, "MIX", "The slot's MIX with the sound coming into it");
    setUpKnob (driveKnob, "DRIVE", "How hard the master drive is pushed");
    setUpKnob (toneKnob, "TONE", "TONE: darker to the left, brighter to the right");
    setUpKnob (compressKnob, "COMPRESS", "How hard the master compressor works");
    setUpKnob (bassKnob, "BASS FREQ", "CLEAN BASS: below this the sound passes untouched");
    setUpKnob (outGainKnob, "GAIN", "CIPHER's output level");
    setUpKnob (outMixKnob, "MIX", "CIPHER's MIX: the dry sound to everything CIPHER does");

    bind();
    startTimerHz (30);
}

CipherPanel::~CipherPanel() = default;

float CipherPanel::raw (const juce::String& id) const { return parameters.getRawParameterValue (id)->load(); }

Cipher::Type CipherPanel::shownType() const
{
    return (Cipher::Type) juce::jlimit (0, Cipher::typeNames.size() - 1,
                                        juce::roundToInt (raw (ParamIDs::cipherSlot (juce::jmin (shown, 9) + 1, "Type"))));
}

void CipherPanel::setChoice (const juce::String& id, int index)
{
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        parameter->endChangeGesture();
    }
}

void CipherPanel::showSlot (int slot)
{
    shown = juce::jlimit (0, masterPage, slot);
    bind();
    resized();
    repaint();
}

void CipherPanel::bind()
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    const auto master = shown == masterPage;

    for (auto* knob : { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob })
        knob->attachment.reset();
    for (auto* key : { &slotOn, &syncKey })
        key->attachment.reset();
    typeList.attachment.reset();
    speedList.attachment.reset();
    if (! master)
    {
        const std::pair<Knob*, const char*> knobs[] = { { &mainKnob, "Main" }, { &p1Knob, "P1" }, { &p2Knob, "P2" }, { &envKnob, "EnvAmount" },
                                                        { &rateKnob, "Rate" }, { &glideKnob, "Glide" }, { &stereoKnob, "Stereo" },
                                                        { &gainKnob, "Gain" }, { &mixKnob, "Mix" } };
        for (const auto& [knob, part] : knobs)
        {
            knob->id = slotId (part);
            knob->attachment = std::make_unique<SA> (parameters, knob->id, knob->slider);
        }
        slotOn.attachment = std::make_unique<BA> (parameters, slotId ("On"), slotOn.button);
        syncKey.attachment = std::make_unique<BA> (parameters, slotId ("Sync"), syncKey.button);
        typeList.attachment = std::make_unique<CA> (parameters, slotId ("Type"), typeList.box);
        speedList.attachment = std::make_unique<CA> (parameters, slotId ("Speed"), speedList.box);
    }
    else if (driveKnob.attachment == nullptr)
    {
        const std::pair<Knob*, const char*> knobs[] = { { &driveKnob, ParamIDs::cipherDrive }, { &toneKnob, ParamIDs::cipherTone },
                                                        { &compressKnob, ParamIDs::cipherCompress }, { &bassKnob, ParamIDs::cipherCleanBassHz },
                                                        { &outGainKnob, ParamIDs::cipherGain }, { &outMixKnob, ParamIDs::cipherMix } };
        for (const auto& [knob, id] : knobs)
        {
            knob->id = id;
            knob->attachment = std::make_unique<SA> (parameters, id, knob->slider);
        }
        const std::pair<Key*, const char*> keys[] = { { &driveOn, ParamIDs::cipherDriveOn }, { &driveAfter, ParamIDs::cipherDriveAfter },
                                                      { &compOn, ParamIDs::cipherCompOn }, { &bassOn, ParamIDs::cipherCleanBassOn },
                                                      { &bassListen, ParamIDs::cipherCleanBassListen } };
        for (const auto& [key, id] : keys)
            key->attachment = std::make_unique<BA> (parameters, id, key->button);
        driveList.attachment = std::make_unique<CA> (parameters, ParamIDs::cipherDriveType, driveList.box);
        compList.attachment = std::make_unique<CA> (parameters, ParamIDs::cipherComp, compList.box);
        lawList.attachment = std::make_unique<CA> (parameters, ParamIDs::cipherMixLaw, lawList.box);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &slotOn.button, &syncKey.button, &typeList.box, &speedList.box, &shapeList.box,
                                                             &mainKnob.slider, &p1Knob.slider, &p2Knob.slider, &envKnob.slider, &rateKnob.slider,
                                                             &glideKnob.slider, &stereoKnob.slider, &gainKnob.slider, &mixKnob.slider })
        c->setVisible (! master);
    for (auto& key : styleKeys)
        key.setVisible (! master);
    for (auto* c : std::initializer_list<juce::Component*> { &driveOn.button, &driveAfter.button, &compOn.button, &bassOn.button,
                                                             &bassListen.button, &driveList.box, &compList.box, &lawList.box,
                                                             &driveKnob.slider, &toneKnob.slider, &compressKnob.slider, &bassKnob.slider,
                                                             &outGainKnob.slider, &outMixKnob.slider })
        c->setVisible (master);
    shownState.clear();
    timerCallback();
}

void CipherPanel::setAreas (juce::Rectangle<int> displayArea, juce::Rectangle<int> controlArea)
{
    const auto all = displayArea.getUnion (controlArea);
    setBounds (all);
    display = displayArea - all.getPosition();
    controls = controlArea - all.getPosition();
    resized();
}

juce::Rectangle<int> CipherPanel::getEditorBounds() const { return editor->getBounds(); }

juce::Point<int> CipherPanel::editorPointFor (juce::Point<float> position) const
{
    return (editor->toScreen (position.x, position.y) + editor->getPosition().toFloat()).roundToInt();
}

void CipherPanel::timerCallback()
{
    if (! isShowing() && ! shownState.isEmpty())
        return;

    // The slot keys show each slot's effect; the shown one is lit.
    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        auto& key = slotKeys[(size_t) i];
        const auto text = i < 10 ? juce::String (i + 1) + " " + shortName ((Cipher::Type) juce::roundToInt (raw (ParamIDs::cipherSlot (i + 1, "Type"))))
                                 : juce::String ("MASTER");
        if (key.getButtonText() != text)
            key.setButtonText (text);
        key.setToggleState (i == shown, juce::dontSendNotification);
    }

    const auto type = shownType();
    const auto labels = Cipher::labelsFor (type);
    const auto option = juce::roundToInt (raw (slotId ("Option")));
    const auto style = juce::roundToInt (raw (slotId ("Style")));
    const auto master = shown == masterPage;
    for (int i = 0; i < (int) optionKeys.size(); ++i)
    {
        auto& key = optionKeys[(size_t) i];
        const auto show = ! master && (labels.hasSlope || (labels.isPan && i < 3));
        key.setVisible (show);
        key.setButtonText (labels.isPan ? Cipher::panModeNames[juce::jmin (i, 2)] : Cipher::slopeNames[i]);
        key.setToggleState (labels.isPan ? i == (option >= 3 ? 0 : option) : i == option, juce::dontSendNotification);
    }
    for (int i = 0; i < (int) styleKeys.size(); ++i)
        styleKeys[(size_t) i].setToggleState (i == style, juce::dontSendNotification);
    if (! master)
    {
        p1Knob.slider.setEnabled (*labels.p1 != 0);
        p2Knob.slider.setEnabled (*labels.p2 != 0);
        mainKnob.slider.setEnabled (type != Cipher::Type::off);
        const auto synced = raw (slotId ("Sync")) > 0.5f;
        rateKnob.slider.setEnabled (! synced);
        speedList.box.setEnabled (synced);
    }

    // The editor's playhead moves with the envelope.
    if (editor->isVisible() && type != Cipher::Type::off)
        editor->repaint();

    juce::String state = juce::String (shown) + "|" + juce::String ((int) type) + "|" + juce::String (option);
    for (auto* knob : { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob,
                        &driveKnob, &toneKnob, &compressKnob, &bassKnob, &outGainKnob, &outMixKnob })
        if (knob->slider.isVisible())
            state << "|" << knob->slider.getValue();
    if (state != shownState)
    {
        shownState = state;
        repaint();
    }
}

void CipherPanel::layoutRow (juce::Rectangle<int> row, std::initializer_list<Knob*> knobs)
{
    // Each knob in a column: its name stamped above, the knob, its value on a small screen.
    const auto count = (int) knobs.size();
    const auto width = juce::jmin (88, row.getWidth() / juce::jmax (1, count));
    for (auto* knob : knobs)
    {
        auto column = row.removeFromLeft (width);
        knob->label = column.removeFromTop (14).toFloat();
        knob->screen = column.removeFromBottom (18).reduced (6, 0).toFloat();
        column.removeFromBottom (2);
        const auto side = juce::jmin (column.getWidth() - 12, column.getHeight());
        knob->slider.setBounds (column.withSizeKeepingCentre (side, side));
    }
}

void CipherPanel::resized()
{
    if (display.isEmpty())
        return;
    editor->setBounds (display);

    auto area = controls;
    auto keys = area.removeFromTop (24);
    keys.removeFromRight (120);   // the GRAPHIC EQ key's place
    onLabel = keys.removeFromLeft (46).toFloat();
    onKey.button.setBounds (keys.removeFromLeft (48).withSizeKeepingCentre (44, 24));
    keys.removeFromLeft (10);
    const auto keyWidth = juce::jmin (64, (keys.getWidth() - 10 * 4) / 11);
    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        slotKeys[(size_t) i].setBounds (keys.removeFromLeft (i == masterPage ? keyWidth + 12 : keyWidth));
        keys.removeFromLeft (4);
    }

    area.removeFromTop (10);
    auto row2 = area.removeFromTop (24);
    area.removeFromTop (8);
    auto row3 = area.removeFromTop (24);
    area.removeFromTop (8);

    const auto placeChoice = [] (Choice& c, juce::Rectangle<int>& row, int labelWidth, int width)
    {
        c.label = row.removeFromLeft (labelWidth).toFloat();
        c.box.setBounds (row.removeFromLeft (width).reduced (0, 1));
        row.removeFromLeft (14);
    };
    const auto placeKey = [] (Key& k, juce::Rectangle<int>& row, int labelWidth)
    {
        k.label = row.removeFromLeft (labelWidth).toFloat();
        k.button.setBounds (row.removeFromLeft (48).withSizeKeepingCentre (44, 24));
        row.removeFromLeft (12);
    };

    if (shown != masterPage)
    {
        placeChoice (typeList, row2, 40, 150);
        optionLabel = row2.removeFromLeft (46).toFloat();
        for (auto& key : optionKeys)
        {
            key.setBounds (row2.removeFromLeft (Cipher::labelsFor (shownType()).isPan ? 70 : 38));
            row2.removeFromLeft (4);
        }
        row2.removeFromLeft (10);
        placeKey (slotOn, row2, 40);

        styleLabel = row3.removeFromLeft (62).toFloat();
        for (auto& key : styleKeys)
        {
            key.setBounds (row3.removeFromLeft (50));
            row3.removeFromLeft (4);
        }
        row3.removeFromLeft (12);
        placeKey (syncKey, row3, 40);
        placeChoice (speedList, row3, 48, 110);
        placeChoice (shapeList, row3, 48, 110);

        layoutRow (area, { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob });
    }
    else
    {
        placeKey (driveOn, row2, 46);
        placeChoice (driveList, row2, 36, 120);
        placeKey (driveAfter, row2, 76);
        placeKey (compOn, row2, 44);
        placeChoice (compList, row2, 36, 120);

        placeKey (bassOn, row3, 84);
        placeKey (bassListen, row3, 52);
        placeChoice (lawList, row3, 60, 90);

        layoutRow (area, { &driveKnob, &toneKnob, &compressKnob, &bassKnob, &outGainKnob, &outMixKnob });
    }
}

void CipherPanel::paint (juce::Graphics& g)
{
    glasses.clear();
    stamped (g, onKey.name, onLabel, 9.0f, juce::Justification::centredRight);

    const auto drawChoice = [&] (Choice& c)
    {
        if (! c.box.isVisible())
            return;
        stamped (g, c.name, c.label, 9.0f, juce::Justification::centredRight);
        glasses.push_back (LedDisplay::paintScreen (g, c.box.getBounds().toFloat().expanded (2.0f, 3.0f), 0.35f));
    };
    const auto drawKey = [&] (Key& k)
    {
        if (k.button.isVisible())
            stamped (g, k.name, k.label, 9.0f, juce::Justification::centredRight);
    };
    const auto type = shownType();
    const auto labels = Cipher::labelsFor (type);
    const auto drawKnob = [&] (Knob& k, const juce::String& name, const juce::String& value)
    {
        if (! k.slider.isVisible())
            return;
        const auto active = k.slider.isEnabled();
        g.setFont (font (9.0f));
        ParagonLookAndFeel::drawStampedText (g, name, k.label, juce::Justification::centred,
                                             active ? Finish::ink() : Finish::ink().withAlpha (0.35f));
        const auto glass = LedDisplay::paintScreen (g, k.screen, 0.35f);
        glasses.push_back (glass);
        g.setColour (orange.withAlpha (active ? 1.0f : 0.35f));
        g.setFont (font (10.0f));
        g.drawFittedText (active ? value : juce::String ("-"), glass.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    };

    if (shown != masterPage)
    {
        drawChoice (typeList);
        drawChoice (speedList);
        drawChoice (shapeList);
        drawKey (slotOn);
        drawKey (syncKey);
        if (labels.hasSlope || labels.isPan)
            stamped (g, labels.isPan ? "MODE" : "SLOPE", optionLabel, 9.0f, juce::Justification::centredRight);
        stamped (g, "ENV MOD", styleLabel, 9.0f, juce::Justification::centredRight);

        const auto value = [this] (const char* part) { return raw (slotId (part)); };
        drawKnob (mainKnob, labels.main[0] != 0 ? labels.main : "MAIN", Cipher::mainText (type, value ("Main")));
        drawKnob (p1Knob, labels.p1[0] != 0 ? labels.p1 : "-", Cipher::p1Text (type, value ("P1")));
        drawKnob (p2Knob, labels.p2[0] != 0 ? labels.p2 : "-", Cipher::p2Text (type, value ("P2")));
        drawKnob (envKnob, "ENV AMT", juce::String (juce::roundToInt (value ("EnvAmount") * 100.0f)) + "%");
        drawKnob (rateKnob, "RATE", juce::String (value ("Rate"), 2) + " Hz");
        drawKnob (glideKnob, "GLIDE", juce::String (juce::roundToInt (value ("Glide"))) + " ms");
        drawKnob (stereoKnob, "STEREO", juce::String (juce::roundToInt (value ("Stereo") * 100.0f)) + "%");
        drawKnob (gainKnob, "GAIN", juce::String (value ("Gain"), 1) + " dB");
        drawKnob (mixKnob, "MIX", juce::String (juce::roundToInt (value ("Mix") * 100.0f)) + "%");
    }
    else
    {
        drawChoice (driveList);
        drawChoice (compList);
        drawChoice (lawList);
        for (auto* key : { &driveOn, &driveAfter, &compOn, &bassOn, &bassListen })
            drawKey (*key);
        drawKnob (driveKnob, "DRIVE", juce::String (juce::roundToInt (raw (ParamIDs::cipherDrive) * 100.0f)) + "%");
        drawKnob (toneKnob, "TONE", juce::String (juce::roundToInt (raw (ParamIDs::cipherTone) * 100.0f)) + "%");
        drawKnob (compressKnob, "COMPRESS", juce::String (juce::roundToInt (raw (ParamIDs::cipherCompress) * 100.0f)) + "%");
        drawKnob (bassKnob, "BASS FREQ", juce::String (juce::roundToInt (raw (ParamIDs::cipherCleanBassHz))) + " Hz");
        drawKnob (outGainKnob, "GAIN", juce::String (raw (ParamIDs::cipherGain), 1) + " dB");
        drawKnob (outMixKnob, "MIX", juce::String (juce::roundToInt (raw (ParamIDs::cipherMix) * 100.0f)) + "%");
    }
}

void CipherPanel::paintOverChildren (juce::Graphics& g)
{
    for (const auto& glass : glasses)
        LedDisplay::paintGlassOverlay (g, glass, 0.35f);
}
