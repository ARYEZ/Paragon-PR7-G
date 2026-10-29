#include "HavokPanel.h"

#include "Parameters.h"
#include "ui/Finish.h"
#include "ui/LedDisplay.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const auto orange = LedDisplay::ledOrange;
    constexpr float screenSize = 0.45f;   // the small screens' frame, as on the faceplate

    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centredLeft)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    // The original's position readout: 1 to 9 across, then 1 to 9 up.
    int padDigit (float position) { return 1 + juce::roundToInt (juce::jlimit (0.0f, 1.0f, position) * 8.0f); }
}

//==============================================================================
// The touch pad: a PR7 screen with a fine grid printed under the glass, the
// finger's spot glowing where you touch (a ring where HOLD keeps it).
class HavokPanel::Pad final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    explicit Pad (HavokPanel& ownerPanel) : owner (ownerPanel)
    {
        setTooltip ("The touch pad: click and drag to play the effect. Let go and it stops (HOLD keeps it).");
    }

    void paint (juce::Graphics& g) override
    {
        glass = LedDisplay::paintScreen (g, getLocalBounds().toFloat(), screenSize);
        const auto area = glass.reduced (4.0f);

        // The grid: eight divisions each way, the centre lines a little brighter.
        for (int i = 1; i < 8; ++i)
        {
            g.setColour (orange.withAlpha (i == 4 ? 0.16f : 0.07f));
            const auto fx = area.getX() + area.getWidth() * (float) i / 8.0f;
            const auto fy = area.getY() + area.getHeight() * (float) i / 8.0f;
            g.fillRect (juce::Rectangle<float> (fx - 0.5f, area.getY(), 1.0f, area.getHeight()));
            g.fillRect (juce::Rectangle<float> (area.getX(), fy - 0.5f, area.getWidth(), 1.0f));
        }

        const auto x = owner.raw (ParamIDs::havokX), y = owner.raw (ParamIDs::havokY);
        const auto touching = owner.raw (ParamIDs::havokTouch) > 0.5f;
        const auto held = owner.raw (ParamIDs::havokHold) > 0.5f;
        const auto on = owner.raw (ParamIDs::havokOn) > 0.5f;
        const juce::Point<float> spot { area.getX() + area.getWidth() * x, area.getBottom() - area.getHeight() * y };

        if (touching || held)
        {
            // Crosshairs through the spot, then its glow.
            g.setColour (orange.withAlpha (on ? 0.4f : 0.2f));
            g.fillRect (juce::Rectangle<float> (spot.x - 0.5f, area.getY(), 1.0f, area.getHeight()));
            g.fillRect (juce::Rectangle<float> (area.getX(), spot.y - 0.5f, area.getWidth(), 1.0f));
            const auto radius = touching ? 16.0f : 12.0f;
            g.setGradientFill (juce::ColourGradient (orange.withAlpha (on ? 0.85f : 0.4f), spot,
                                                     orange.withAlpha (0.0f), spot.translated (radius, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (spot));
            if (touching)
            {
                g.setColour (juce::Colour (0xffffe4dc).withAlpha (on ? 0.9f : 0.5f));
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (spot));
            }
            else
            {
                g.setColour (orange.withAlpha (on ? 0.9f : 0.5f));
                g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (spot), 1.5f);
            }
        }
        else
        {
            // Where the finger was, faintly.
            g.setColour (orange.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (spot));
        }
    }

    void paintOverChildren (juce::Graphics& g) override { LedDisplay::paintGlassOverlay (g, glass, screenSize); }

    void mouseDown (const juce::MouseEvent& event) override { press (event.position); }
    void mouseDrag (const juce::MouseEvent& event) override { move (event.position); }
    void mouseUp (const juce::MouseEvent&) override { lift(); }

    // A finger down: one gesture for X, Y and TOUCH until it lifts, so the DAW records the move as one.
    void press (juce::Point<float> point)
    {
        for (const auto* id : { ParamIDs::havokX, ParamIDs::havokY, ParamIDs::havokTouch })
            if (auto* parameter = owner.parameters.getParameter (id))
                parameter->beginChangeGesture();
        move (point);
        owner.setParameter (ParamIDs::havokTouch, 1.0f);
    }

    void lift()
    {
        owner.setParameter (ParamIDs::havokTouch, 0.0f);
        for (const auto* id : { ParamIDs::havokX, ParamIDs::havokY, ParamIDs::havokTouch })
            if (auto* parameter = owner.parameters.getParameter (id))
                parameter->endChangeGesture();
        repaint();
    }

    // Where a point lands on the pad, 0 to 1 across and up.
    juce::Point<float> positionOf (juce::Point<float> point) const
    {
        const auto area = (glass.isEmpty() ? getLocalBounds().toFloat().reduced (8.0f) : glass).reduced (4.0f);
        return { juce::jlimit (0.0f, 1.0f, (point.x - area.getX()) / area.getWidth()),
                 juce::jlimit (0.0f, 1.0f, (area.getBottom() - point.y) / area.getHeight()) };
    }

    // The point on the pad for a position (0 to 1 across and up).
    juce::Point<float> pointFor (juce::Point<float> position) const
    {
        const auto area = (glass.isEmpty() ? getLocalBounds().toFloat().reduced (8.0f) : glass).reduced (4.0f);
        return { area.getX() + area.getWidth() * position.x, area.getBottom() - area.getHeight() * position.y };
    }

    void move (juce::Point<float> point)
    {
        const auto position = positionOf (point);
        owner.setParameter (ParamIDs::havokX, position.x);
        owner.setParameter (ParamIDs::havokY, position.y);
        repaint();
    }

private:
    HavokPanel& owner;
    juce::Rectangle<float> glass;
};

//==============================================================================
HavokPanel::HavokPanel (ParagonX1AudioProcessor& processorToUse)
    : processor (processorToUse), parameters (processorToUse.getParameters()),
      onAttachment (parameters, ParamIDs::havokOn, onKey),
      holdAttachment (parameters, ParamIDs::havokHold, holdKey),
      programKnobAttachment (parameters, ParamIDs::havokProgram, programKnob)
{
    pad = std::make_unique<Pad> (*this);
    addAndMakeVisible (*pad);

    for (auto* key : { &onKey, &holdKey })
    {
        key->getProperties().set (ParagonLookAndFeel::rackKeyProperty, true);
        addAndMakeVisible (*key);
    }
    onKey.setTooltip ("FX ON: switch HAVOK on or off");
    holdKey.setTooltip ("HOLD: keep the effect where you last touched the pad, as if your finger were still on it");

    recKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    recKey.setTooltip ("REC (programs 51-60): press to record a phrase, again to stop. Then touch the pad to play it. "
                       "Press again to record a new phrase.");
    recKey.onClick = [this] { processor.getHavok().pressRec(); };
    addChildComponent (recKey);

    for (int i = 0; i < (int) mapKeys.size(); ++i)
    {
        auto& key = mapKeys[(size_t) i];
        key.setButtonText (juce::String (i + 1));
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setTooltip ("PROGRAM MAP " + juce::String (i + 1) + ": press for its program (and held position). "
                        "Hold it for two seconds to store the program you're on there.");
        auto& hold = mapHolds[(size_t) i];
        hold = std::make_unique<HoldDetector> (2000);
        hold->onHold = [this, i] { storeMap (i + 1); };
        key.addMouseListener (hold.get(), false);
        key.onClick = [this, i] { if (! mapHolds[(size_t) i]->consumeHold()) recallMap (i + 1); };
        addAndMakeVisible (key);
    }

    programKnob.getProperties().set (ParagonLookAndFeel::pedalKnobProperty, true);
    programKnob.setPopupDisplayEnabled (true, true, nullptr);
    programKnob.setTooltip ("PROGRAM: turn to pick one of the 60 programs");
    addAndMakeVisible (programKnob);

    programList.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    programList.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    programList.setColour (juce::ComboBox::textColourId, orange);
    programList.setColour (juce::ComboBox::arrowColourId, orange.withAlpha (0.6f));
    programList.setTooltip ("The program: click for the list");
    programList.addItemList (effects::Havok::programNames(), 1);
    programListAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, ParamIDs::havokProgram, programList);
    addAndMakeVisible (programList);

    // SOUND, for the scratch programs (61-68): what's on the record.
    soundList.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    soundList.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    soundList.setColour (juce::ComboBox::textColourId, orange);
    soundList.setColour (juce::ComboBox::arrowColourId, orange.withAlpha (0.6f));
    soundList.setTooltip ("SOUND: what the scratch programs scratch (our own sounds)");
    soundList.addItemList (effects::Havok::scratchSoundNames, 1);
    soundListAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (parameters, ParamIDs::havokScratchSound, soundList);
    addChildComponent (soundList);

    startTimerHz (30);
}

HavokPanel::~HavokPanel()
{
    for (size_t i = 0; i < mapKeys.size(); ++i)
        mapKeys[i].removeMouseListener (mapHolds[i].get());
}

float HavokPanel::raw (const juce::String& id) const { return parameters.getRawParameterValue (id)->load(); }

void HavokPanel::setParameter (const juce::String& id, float plainValue)
{
    if (auto* parameter = parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
}

int HavokPanel::program() const
{
    return juce::jlimit (0, effects::Havok::numPrograms - 1, juce::roundToInt (raw (ParamIDs::havokProgram)));
}

void HavokPanel::setAreas (juce::Rectangle<int> displayArea, juce::Rectangle<int> controlArea)
{
    const auto all = displayArea.getUnion (controlArea);
    setBounds (all);
    display = displayArea - all.getPosition();
    controls = controlArea - all.getPosition();
    resized();
}

juce::Rectangle<int> HavokPanel::getPadBounds() const { return pad->getBounds(); }

void HavokPanel::touchPad (juce::Point<float> position)
{
    const auto bottomLeft = pad->pointFor ({ 0.0f, 0.0f }), topRight = pad->pointFor ({ 1.0f, 1.0f });
    const juce::Point<float> point { bottomLeft.x + (topRight.x - bottomLeft.x) * position.x,
                                     bottomLeft.y + (topRight.y - bottomLeft.y) * position.y };
    if (raw (ParamIDs::havokTouch) > 0.5f)
        pad->move (point);
    else
        pad->press (point);
}

void HavokPanel::releasePad() { pad->lift(); }

void HavokPanel::recallMap (int key)
{
    // The stored program, and the stored HOLD with its position.
    const auto storedHold = raw (ParamIDs::havokMap (key, "Hold")) > 0.5f;
    for (const auto& [id, value] : std::initializer_list<std::pair<juce::String, float>> {
             { ParamIDs::havokProgram, raw (ParamIDs::havokMap (key, "Program")) },
             { ParamIDs::havokHold, storedHold ? 1.0f : 0.0f } })
    {
        auto* parameter = parameters.getParameter (id);
        parameter->beginChangeGesture();
        setParameter (id, value);
        parameter->endChangeGesture();
    }
    if (storedHold)
        for (const auto* part : { "X", "Y" })
        {
            const auto id = juce::String (part) == "X" ? ParamIDs::havokX : ParamIDs::havokY;
            auto* parameter = parameters.getParameter (id);
            parameter->beginChangeGesture();
            setParameter (id, raw (ParamIDs::havokMap (key, part)));
            parameter->endChangeGesture();
        }
    programShownSince = juce::Time::getMillisecondCounter();
}

void HavokPanel::storeMap (int key)
{
    const std::pair<juce::String, juce::String> parts[] = { { ParamIDs::havokMap (key, "Program"), ParamIDs::havokProgram },
                                                            { ParamIDs::havokMap (key, "Hold"), ParamIDs::havokHold },
                                                            { ParamIDs::havokMap (key, "X"), ParamIDs::havokX },
                                                            { ParamIDs::havokMap (key, "Y"), ParamIDs::havokY } };
    for (const auto& [to, from] : parts)
    {
        auto* parameter = parameters.getParameter (to);
        parameter->beginChangeGesture();
        setParameter (to, raw (from));
        parameter->endChangeGesture();
    }
    // The key flashes to say it's stored.
    mapKeys[(size_t) (key - 1)].setToggleState (true, juce::dontSendNotification);
    blink = -12;
}

juce::String HavokPanel::getDisplayText() const
{
    // While touching: the position, X then Y. With HOLD: the program's
    // number for a second after it's chosen, then the held position.
    const auto touching = raw (ParamIDs::havokTouch) > 0.5f;
    const auto held = raw (ParamIDs::havokHold) > 0.5f;
    const auto position = juce::String (padDigit (raw (ParamIDs::havokX))) + "." + juce::String (padDigit (raw (ParamIDs::havokY)));
    if (touching)
        return position;
    if (held && juce::Time::getMillisecondCounter() - programShownSince > 1000)
        return position;
    return juce::String (program() + 1);
}

void HavokPanel::timerCallback()
{
    if (! isShowing())
        return;

    if (program() != shownProgram)
    {
        shownProgram = program();
        programShownSince = juce::Time::getMillisecondCounter();
        recKey.setVisible (effects::Havok::isSampler (shownProgram));
        soundList.setVisible (effects::Havok::isScratch (shownProgram));
    }

    // REC's light: blinking while it waits (for REC, or for sound with AUTO
    // REC), quickly while recording, steady once a phrase is in.
    ++blink;
    using Rec = effects::Havok::Rec;
    const auto rec = processor.getHavok().getRecState();
    const auto lit = rec == Rec::recorded || (rec == Rec::recording ? (blink / 2) % 2 == 0 : (blink / 8) % 2 == 0);
    recKey.setToggleState (lit, juce::dontSendNotification);
    if (blink == 0)
        for (auto& key : mapKeys)
            key.setToggleState (false, juce::dontSendNotification);
    repaint();
}

void HavokPanel::resized()
{
    recKey.setVisible (effects::Havok::isSampler (program()));     // REC belongs to programs 51-60
    soundList.setVisible (effects::Havok::isScratch (program()));  // SOUND to 61-68
    if (display.isEmpty())
        return;

    // The display: the pad, square, on the left; the readout screen beside it.
    auto left = display;
    const auto side = left.getHeight();
    pad->setBounds (left.removeFromLeft (side));
    left.removeFromLeft (10);
    screenArea = left.toFloat();

    // The face: FX ON, HOLD and REC; the PROGRAM MAP keys; the PROGRAM knob and list.
    auto area = controls;
    auto keys = area.removeFromTop (24);
    keys.removeFromRight (120);   // the GRAPHIC EQ key's place
    onLabel = keys.removeFromLeft (52).toFloat();
    onKey.setBounds (keys.removeFromLeft (48).withSizeKeepingCentre (44, 24));
    keys.removeFromLeft (18);
    holdLabel = keys.removeFromLeft (44).toFloat();
    holdKey.setBounds (keys.removeFromLeft (48).withSizeKeepingCentre (44, 24));
    keys.removeFromLeft (26);
    const auto recPlace = keys;
    recKey.setBounds (keys.removeFromLeft (64));
    auto soundPlace = recPlace;
    soundLabel = soundPlace.removeFromLeft (52).toFloat();
    soundPlace.removeFromLeft (10);
    soundList.setBounds (soundPlace.removeFromLeft (180).reduced (0, 1));

    area.removeFromTop (14);
    auto mapRow = area.removeFromTop (24);
    mapLabel = mapRow.removeFromLeft (104).toFloat();
    for (auto& key : mapKeys)
    {
        key.setBounds (mapRow.removeFromLeft (52));
        mapRow.removeFromLeft (8);
    }

    area.removeFromTop (12);
    auto knobColumn = area.removeFromLeft (96);
    knobLabel = knobColumn.removeFromBottom (14).toFloat();
    programKnob.setBounds (knobColumn.withSizeKeepingCentre (juce::jmin (knobColumn.getWidth(), knobColumn.getHeight()),
                                                             juce::jmin (knobColumn.getWidth(), knobColumn.getHeight())));
    area.removeFromLeft (14);
    programList.setBounds (area.removeFromTop (26).withWidth (juce::jmin (area.getWidth(), 360)).reduced (0, 1));
    area.removeFromTop (8);
    infoArea = area.toFloat();
}

void HavokPanel::paint (juce::Graphics& g)
{
    glasses.clear();
    const auto prog = program();
    const auto& info = effects::Havok::programs()[(size_t) prog];

    // The readout screen: the original's two big digits, then the program's
    // name and what X and Y do.
    const auto glass = LedDisplay::paintScreen (g, screenArea, screenSize);
    glasses.push_back (glass);
    auto inside = glass.reduced (glass.getWidth() * 0.06f, glass.getHeight() * 0.08f);
    const auto text = getDisplayText();
    {
        auto digits = inside.removeFromTop (inside.getHeight() * 0.48f);
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (8.0f));
        g.drawText (raw (ParamIDs::havokTouch) > 0.5f || text.contains (".") ? "POSITION" : "PROGRAM", digits.removeFromLeft (52.0f),
                    juce::Justification::topLeft, false);
        const auto height = digits.getHeight();
        const auto width = height * 0.55f;
        auto x = digits.getRight() - 2.0f * width * 1.2f;
        for (int i = 0, d = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            if (c == '.')
            {
                g.setColour (orange);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x - width * 0.12f, digits.getBottom() - 3.0f }));
                continue;
            }
            // Two digits' room: a one-digit program sits in the right-hand one.
            if (d == 0 && text.length() == 1)
                x += width * 1.2f;
            LedDisplay::drawDigit (g, c, juce::Rectangle<float> (x, digits.getY(), width, height), orange);
            x += width * 1.2f;
            ++d;
        }
    }
    inside.removeFromTop (6.0f);
    const auto line = [&] (const juce::String& s, float alpha, float height)
    {
        g.setColour (orange.withAlpha (alpha));
        g.setFont (font (height));
        g.drawFittedText (s, inside.removeFromTop (height + 3.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    };
    line (juce::String (prog + 1) + " " + info.name, 1.0f, 12.0f);
    // The scratch programs' X shows the speed it's set to.
    line ("X  " + (effects::Havok::isScratch (prog) && prog != effects::Havok::numPrograms - 1
                       ? "RATE " + effects::Havok::scratchDivisionNames[effects::Havok::scratchDivision (raw (ParamIDs::havokX))]
                       : juce::String (info.x)), 0.75f, 10.0f);
    line ("Y  " + juce::String (info.y), 0.75f, 10.0f);
    if (effects::Havok::isSampler (prog))
    {
        using Rec = effects::Havok::Rec;
        const auto& havok = processor.getHavok();
        const auto rec = havok.getRecState();
        const auto seconds = juce::String (havok.getRecordedSeconds(), 1) + " s";
        line (rec == Rec::ready ? juce::String ("PRESS REC")
              : rec == Rec::waiting ? juce::String ("WAITING FOR SOUND")
              : rec == Rec::recording ? "RECORDING " + seconds
              : "PHRASE " + seconds, 0.75f, 10.0f);
    }

    // The face: names stamped in the unit's ink, hints printed in its red.
    stamped (g, "FX ON", onLabel, 9.0f, juce::Justification::centredRight);
    stamped (g, "HOLD", holdLabel, 9.0f, juce::Justification::centredRight);
    stamped (g, "PROGRAM MAP", mapLabel, 9.0f);
    stamped (g, "PROGRAM", knobLabel, 9.0f, juce::Justification::centred);
    glasses.push_back (LedDisplay::paintScreen (g, programList.getBounds().toFloat().expanded (2.0f, 3.0f), 0.35f));
    if (soundList.isVisible())
    {
        stamped (g, "SOUND", soundLabel, 9.0f, juce::Justification::centredRight);
        glasses.push_back (LedDisplay::paintScreen (g, soundList.getBounds().toFloat().expanded (2.0f, 3.0f), 0.35f));
    }

    auto hints = infoArea;
    g.setColour (Finish::printedRed());
    g.setFont (font (10.0f));
    g.drawText (juce::String (info.hint).toUpperCase(), hints.removeFromTop (14.0f), juce::Justification::centredLeft, false);
    g.setFont (font (8.5f));
    g.drawText (effects::Havok::isScratch (prog) && prog != effects::Havok::numPrograms - 1
                    ? "TOUCH THE PAD (OR MIDI CC 92) TO SCRATCH, IN TIME WITH THE SONG  /  HOLD A MAP KEY 2 S TO STORE"
                    : "TOUCH THE PAD TO PLAY  /  HOLD A MAP KEY 2 S TO STORE",
                hints.removeFromTop (14.0f), juce::Justification::centredLeft, false);
}

void HavokPanel::paintOverChildren (juce::Graphics& g)
{
    for (const auto& glass : glasses)
        LedDisplay::paintGlassOverlay (g, glass, 0.35f);
}
