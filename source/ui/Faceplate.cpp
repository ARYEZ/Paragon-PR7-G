#include "Faceplate.h"

#include "Finish.h"
#include "LedDisplay.h"
#include "Parameters.h"
#include "ParagonLookAndFeel.h"
#include "PedalBody.h"
#include "dsp/CabStage.h"
#include "dsp/effects/CompGate.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Drive.h"
#include "dsp/effects/Reverb.h"

// Every part here is one of the PR7's own: the unit's pearl chassis
// (PedalBody::paintUnit), its LCD screens (LedDisplay::paintScreen, the LED
// orange), big knobs, small keys with their light stripes, neon LED bars,
// and lettering stamped in ink or printed in red, all following the
// finish. (The helpers are the PR7-V's.)
namespace
{
    const PedalBody::Shape unitShape { 16.0f, 10.0f, 5.0f, 0.3f };
    constexpr float glowRoom = 7.0f;        // round the unit, for its light
    constexpr float screenSize = 0.45f;     // the small screens' frame and bezel, against the main display's
    const auto orange = LedDisplay::ledOrange;

    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centred)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    // The model's name, as PARAGON is on the badge, with a red printed line under it.
    void nameplate (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, const juce::String& line)
    {
        g.setFont (font (area.getHeight() * 0.62f, juce::Font::bold | juce::Font::italic));
        ParagonLookAndFeel::drawStampedText (g, name, area.removeFromTop (area.getHeight() * 0.66f), juce::Justification::bottomLeft, Finish::ink());
        g.setColour (Finish::printedRed());
        g.setFont (font (area.getHeight() * 0.72f));
        g.drawText (line, area, juce::Justification::centredLeft, false);
    }

    // A PR7 screen: returns its glass, to draw on before `finish`.
    juce::Rectangle<float> screen (juce::Graphics& g, juce::Rectangle<float> area) { return LedDisplay::paintScreen (g, area, screenSize); }
    void finish (juce::Graphics& g, juce::Rectangle<float> glass) { LedDisplay::paintGlassOverlay (g, glass, screenSize); }

    // A meter on the glass, the tuner's way: a row of LED bars, lit up to `amount` (0 to 1).
    void bars (juce::Graphics& g, juce::Rectangle<float> area, float amount, int count)
    {
        const auto width = area.getWidth() / (float) count;
        const auto lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, amount) * (float) count);
        for (int i = 0; i < count; ++i)
        {
            const auto bar = juce::Rectangle<float> (area.getX() + width * (float) i, area.getY(), width, area.getHeight()).reduced (width * 0.22f, 0.0f);
            if (i < lit)
            {
                g.setColour (orange.withAlpha (0.3f));
                g.fillRoundedRectangle (bar.expanded (2.0f), 2.0f);
                g.setColour (orange);
            }
            else
            {
                g.setColour (orange.withAlpha (0.1f));
            }
            g.fillRoundedRectangle (bar, 1.5f);
        }
    }

    // A small screen with a label stamped beside it and its value in orange.
    void readout (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label, const juce::String& value)
    {
        stamped (g, label, area.removeFromLeft (64.0f), 8.0f, juce::Justification::centredLeft);
        const auto glass = screen (g, area);
        g.setColour (orange);
        g.setFont (font (glass.getHeight() * 0.62f));
        g.drawFittedText (value, glass.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredRight, 1, 0.7f);
        finish (g, glass);
    }

    // A number on a screen in the main display's seven-segment digits (a
    // dot drawn for a decimal point), then a small unit after it.
    void sevenSegment (juce::Graphics& g, juce::Rectangle<float> glass, const juce::String& number, const juce::String& unitText)
    {
        auto area = glass.reduced (glass.getWidth() * 0.06f, glass.getHeight() * 0.2f);
        const auto unitWidth = area.getHeight() * 0.9f;
        g.setColour (orange);
        g.setFont (font (area.getHeight() * 0.38f));
        g.drawText (unitText, area.removeFromRight (unitWidth), juce::Justification::bottomLeft, false);
        const auto digitHeight = area.getHeight();
        const auto digitWidth = digitHeight * 0.55f;
        auto digits = 0;
        for (auto c : number)
            digits += c == '.' ? 0 : 1;
        auto x = area.getRight() - 4.0f - (float) digits * digitWidth * 1.15f;
        for (auto c : number)
        {
            if (c == '.')
            {
                g.setColour (orange);
                g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre ({ x - digitWidth * 0.08f, area.getBottom() - 2.0f }));
                continue;
            }
            LedDisplay::drawDigit (g, c, juce::Rectangle<float> (x, area.getY(), digitWidth, digitHeight), orange);
            x += digitWidth * 1.15f;
        }
    }

    // A small caption at the top left of a screen's glass, as on the tuner.
    void caption (juce::Graphics& g, juce::Rectangle<float>& plot, const juce::String& text)
    {
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (8.0f));
        g.drawText (text, plot.removeFromTop (10.0f), juce::Justification::topLeft, false);
    }

    // One of the unit's small keys with its name stamped under it, its stripe lit while `on`.
    void smallKey (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& name, bool on)
    {
        stamped (g, name, cell.removeFromBottom (12.0f), 8.0f);
        const auto key = cell.withSizeKeepingCentre (juce::jmin (52.0f, cell.getWidth()), juce::jmin (26.0f, cell.getHeight()));
        ParagonLookAndFeel::drawSmallKey (g, key, false, false);
        ParagonLookAndFeel::drawKeyStripe (g, key.reduced (4.0f, 2.0f), on, false);
    }

    // The angle a knob points at for `position` (0 to 1), as the unit's knobs turn.
    float knobAngle (float position) { return juce::MathConstants<float>::pi * (1.2f + 1.6f * juce::jlimit (0.0f, 1.0f, position)); }

    // One of the unit's big knobs with its name stamped under it.
    // (In a cell taller than the knob, the knob and its name stay together, centred.)
    void bigKnob (juce::Graphics& g, juce::Rectangle<float> cell, const juce::String& name, float position)
    {
        const auto size = juce::jmin (cell.getWidth(), cell.getHeight() + 4.0f);
        cell = cell.withSizeKeepingCentre (cell.getWidth(), juce::jmin (cell.getHeight(), size + 8.0f));
        stamped (g, name, cell.removeFromBottom (12.0f), 8.0f);
        ParagonLookAndFeel::drawPedalKnob (g, cell.withSizeKeepingCentre (size, size).translated (0.0f, -2.0f), knobAngle (position));
    }

    // What kind of pedal each drive MODEL is, printed under its name (in
    // Drive::modelNames' order).
    const char* driveKind (int model)
    {
        static const char* kinds[] = { "DISTORTION", "OVERDRIVE", "FUZZ", "FUZZ", "TIGHT OVERDRIVE", "TRANSPARENT OVERDRIVE", "METAL DISTORTION" };
        static_assert (std::size (kinds) == 7);
        return kinds[(size_t) juce::jlimit (0, (int) std::size (kinds) - 1, model)];
    }
}

//==============================================================================
Faceplate::Faceplate (ParagonX1AudioProcessor& processorToShow)
    : processor (processorToShow), parameters (processorToShow.getParameters())
{
    setInterceptsMouseClicks (false, false);
    Finish::changes().addChangeListener (this);
    startTimerHz (30);
}

Faceplate::~Faceplate()
{
    Finish::changes().removeChangeListener (this);
}

void Faceplate::resized()
{
    // Twice the size, so the grain stays fine on high-resolution screens (as on the unit).
    satin = Finish::makeSatinGrain (juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), 4321);
}

void Faceplate::changeListenerCallback (juce::ChangeBroadcaster*)
{
    resized();   // the grain differs on black
    repaint();
}

void Faceplate::setBlock (Unit newUnit, const juce::String& newTitle, Icons::Type newIcon, juce::Colour newColour,
                          const juce::String& newOnIds, const juce::String& newTypeId)
{
    unit = newUnit;
    title = newTitle;
    icon = newIcon;
    colour = newColour;
    onIds = juce::StringArray::fromTokens (newOnIds, ",", {});
    typeId = newTypeId;
    repaint();
}

Faceplate::Look Faceplate::getLook() const
{
    switch (unit)
    {
        case Unit::drive: return Look::drive;
        case Unit::amp:      return Look::amp;
        case Unit::cabinet:  return Look::cabinet;
        case Unit::pitch:    return Look::pitch;
        case Unit::dynamics: return juce::roundToInt (raw (ParamIDs::compType)) == (int) effects::CompGate::Type::daybreak ? Look::daybreak : Look::classic;
        case Unit::delay:    return Look::delay;
        case Unit::reverb:   return Look::reverb;
        case Unit::other:    break;
    }
    return Look::generic;
}

float Faceplate::raw (const juce::String& id) const { return parameters.getRawParameterValue (id)->load(); }

float Faceplate::normalised (const juce::String& id) const { return parameters.getParameter (id)->getValue(); }

juce::String Faceplate::text (const juce::String& id) const { return parameters.getParameter (id)->getCurrentValueAsText(); }

bool Faceplate::isOn() const
{
    for (const auto& id : onIds)
        if (raw (id) > 0.5f)
            return true;
    return false;
}

void Faceplate::timerCallback()
{
    if (! isShowing())
        return;
    updateMeters();
    repaint();
}

void Faceplate::updateMeters()
{
    // A meter's ballistics: up at once, falling back slowly.
    const auto peak = processor.getFaceplateMeter().takePeak();
    shownInput = juce::jmax (peak, shownInput * 0.88f);

    // A needle's ballistics: quick to swing down, slower to come back.
    const auto reduction = juce::jmin (0.0f, processor.getCompGainReductionDb());
    shownReduction += (reduction < shownReduction ? 0.6f : 0.15f) * (reduction - shownReduction);
}

//==============================================================================
void Faceplate::paint (juce::Graphics& g)
{
    // The unit: the PR7's chassis, smaller, in the current finish.
    const auto chassis = getLocalBounds().toFloat().reduced (glowRoom);
    const auto face = chassis.withTrimmedBottom (unitShape.frontEdge).reduced (9.0f);
    PedalBody::paintUnit (g, chassis, face, [this] (juce::Graphics& gg) { gg.drawImage (satin, getLocalBounds().toFloat()); }, unitShape);

    // Its name top left, its status LED top right (as over the footswitches).
    auto content = face.reduced (12.0f, 8.0f);
    auto header = content.removeFromTop (30.0f);
    ParagonLookAndFeel::drawNeonBar (g, header.removeFromRight (34.0f).withSizeKeepingCentre (30.0f, 6.0f).translated (0.0f, -6.0f), isOn());
    content.removeFromTop (6.0f);
    switch (getLook())
    {
        case Look::drive:   paintDrive (g, header, content); break;
        case Look::amp:      paintAmp (g, header, content); break;
        case Look::cabinet:  paintCabinet (g, header, content); break;
        case Look::pitch:    paintPitch (g, header, content); break;
        case Look::classic:  paintClassic (g, header, content); break;
        case Look::daybreak: paintDaybreak (g, header, content); break;
        case Look::delay:    paintDelay (g, header, content); break;
        case Look::reverb:   paintReverb (g, header, content); break;
        case Look::generic: paintGeneric (g, header, content); break;
    }
}

void Faceplate::paintGeneric (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // The block's name and icon, and its TYPE on a screen.
    nameplate (g, header, title, "NEURAL MULTI-FX PROCESSOR");
    Icons::draw (g, icon, body.removeFromLeft (body.getHeight()).reduced (body.getHeight() * 0.14f), Finish::ink(), 2.2f, true);
    body.removeFromLeft (14.0f);
    const auto glass = screen (g, body.withSizeKeepingCentre (body.getWidth(), juce::jmin (body.getHeight(), 70.0f)));
    auto inside = glass.reduced (glass.getWidth() * 0.05f, glass.getHeight() * 0.16f);
    g.setColour (orange.withAlpha (0.7f));
    g.setFont (font (inside.getHeight() * 0.24f));
    g.drawText (typeId.isNotEmpty() ? "TYPE" : "STATUS", inside.removeFromTop (inside.getHeight() * 0.32f), juce::Justification::topLeft, false);
    g.setColour (orange.withAlpha (isOn() ? 1.0f : 0.45f));
    g.setFont (font (inside.getHeight() * 0.5f));
    g.drawFittedText ((typeId.isNotEmpty() ? text (typeId) : juce::String (isOn() ? "ON" : "OFF")).toUpperCase(),
                      inside.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    finish (g, glass);
}

void Faceplate::paintInputMeter (juce::Graphics& g, juce::Rectangle<float> area)
{
    // The guitar's level going in, in LED bars (-48 to 0 dB).
    stamped (g, "INPUT", area.removeFromLeft (52.0f), 8.0f, juce::Justification::centredLeft);
    const auto glass = screen (g, area);
    const auto db = juce::Decibels::gainToDecibels (shownInput, -60.0f);
    bars (g, glass.reduced (glass.getWidth() * 0.03f, glass.getHeight() * 0.28f), (db + 48.0f) / 48.0f, 28);
    finish (g, glass);
}

void Faceplate::paintDrive (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // A pedal: the MODEL's name in the badge's lettering and what kind of
    // pedal it is, its knobs, and the level going in.
    const auto model = juce::roundToInt (raw (ParamIDs::driveModel));
    const auto& names = effects::Drive::modelNames;
    nameplate (g, header, names[juce::jlimit (0, names.size() - 1, model)].toUpperCase(), driveKind (model));

    paintInputMeter (g, body.removeFromBottom (22.0f));
    body.removeFromBottom (6.0f);

    // Skyline's ATTACK: eight small keys, the chosen one's stripe lit.
    if (model == (int) effects::Drive::Model::skyline)
    {
        auto keys = body.removeFromBottom (30.0f);
        stamped (g, "ATTACK", keys.removeFromLeft (52.0f), 8.0f, juce::Justification::centredLeft);
        const auto chosen = juce::roundToInt (raw (ParamIDs::driveAttack));
        const auto width = keys.getWidth() / 8.0f;
        for (int position = 0; position < 8; ++position)
        {
            const auto key = juce::Rectangle<float> (keys.getX() + width * (float) position, keys.getY(), width, keys.getHeight()).reduced (3.0f, 2.0f);
            ParagonLookAndFeel::drawSmallKey (g, key, false, false);
            ParagonLookAndFeel::drawKeyStripe (g, key.reduced (4.0f, 2.0f), position == chosen, false);
        }
        body.removeFromBottom (4.0f);
    }

    // The knobs: DRIVE, its tone controls, LEVEL.
    std::vector<std::pair<const char*, const char*>> knobs { { ParamIDs::driveGain, "DRIVE" } };
    if (model == (int) effects::Drive::Model::abyss)
    {
        knobs.insert (knobs.end(), { { ParamIDs::driveHigh, "HIGH" }, { ParamIDs::driveLow, "LOW" },
                                     { ParamIDs::driveMiddle, "MIDDLE" }, { ParamIDs::driveMidFreq, "MID FREQ" } });
    }
    else
    {
        knobs.push_back ({ ParamIDs::driveTone, "TONE" });
        if (model == (int) effects::Drive::Model::skyline)
            knobs.push_back ({ ParamIDs::driveGate, "GATE" });
    }
    knobs.push_back ({ ParamIDs::driveLevel, "LEVEL" });

    const auto cellWidth = body.getWidth() / (float) knobs.size();
    for (const auto& [id, name] : knobs)
        bigKnob (g, body.removeFromLeft (cellWidth).reduced (cellWidth > 80.0f ? cellWidth * 0.12f : 2.0f, 0.0f), name, normalised (id));
}

void Faceplate::paintAmp (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // An amp head: the neural model's name and details on a screen, its
    // knobs, NORMALISE on a key, and the level going in.
    nameplate (g, header, "NAMP", "NEURAL AMP MODEL");

    paintInputMeter (g, body.removeFromBottom (22.0f));
    body.removeFromBottom (6.0f);

    // RIG LEVEL, PRESENCE and NORMALISE on the right.
    auto controls = body.removeFromRight (body.getWidth() * 0.46f);
    body.removeFromRight (14.0f);
    auto keyCell = controls.removeFromRight (70.0f);
    stamped (g, "NORMALISE", keyCell.removeFromBottom (12.0f), 8.0f);
    const auto key = keyCell.withSizeKeepingCentre (52.0f, 26.0f);
    ParagonLookAndFeel::drawSmallKey (g, key, false, false);
    ParagonLookAndFeel::drawKeyStripe (g, key.reduced (4.0f, 2.0f), raw (ParamIDs::ampNormalise) > 0.5f, false);
    const auto half = controls.getWidth() / 2.0f;
    bigKnob (g, controls.removeFromLeft (half), "RIG LEVEL", normalised (ParamIDs::rigLevel));
    bigKnob (g, controls, "PRESENCE", normalised (ParamIDs::presence));

    // The model on a screen: its name, then its details (or what went wrong).
    const auto status = processor.getAmpStatus();
    const auto hasModel = status.file != juce::File();
    const auto glass = screen (g, body);
    auto inside = glass.reduced (glass.getWidth() * 0.06f, glass.getHeight() * 0.14f);
    g.setColour (orange.withAlpha (0.7f));
    g.setFont (font (9.0f));
    g.drawText ("MODEL", inside.removeFromTop (12.0f), juce::Justification::topLeft, false);
    g.setColour (orange.withAlpha (isOn() ? 1.0f : 0.45f));
    g.setFont (font (juce::jmin (22.0f, inside.getHeight() * 0.3f)));
    g.drawFittedText (status.loading ? juce::String ("LOADING...") : hasModel ? status.file.getFileNameWithoutExtension().toUpperCase() : juce::String ("NO MODEL LOADED"),
                      inside.removeFromTop (inside.getHeight() * 0.55f).toNearestInt(), juce::Justification::centredLeft, 2, 0.75f);
    g.setColour (orange.withAlpha (0.7f));
    g.setFont (font (10.0f));
    g.drawFittedText (status.error.isNotEmpty() ? status.error : status.detail, inside.toNearestInt(), juce::Justification::topLeft, 2, 0.8f);
    finish (g, glass);
}

void Faceplate::paintCabinet (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // A speaker cabinet: the IR's name and length on a screen, then what it
    // does to the tone, as its frequency response.
    nameplate (g, header, "CABINET", "IMPULSE RESPONSE");
    const auto on = isOn();
    const auto status = processor.getCabStatus();
    const auto hasIr = status.file != juce::File() && status.error.isEmpty();

    auto nameArea = body.removeFromTop (34.0f);
    {
        const auto glass = screen (g, nameArea);
        auto inside = glass.reduced (glass.getWidth() * 0.03f, 0.0f);
        g.setColour (orange.withAlpha (0.7f));
        g.setFont (font (10.0f));
        g.drawText (status.detail, inside.removeFromRight (90.0f), juce::Justification::centredRight, false);
        g.setColour (orange.withAlpha (on ? 1.0f : 0.45f));
        g.setFont (font (glass.getHeight() * 0.5f));
        g.drawFittedText (status.loading ? juce::String ("LOADING...")
                              : status.error.isNotEmpty() ? status.error.toUpperCase()
                              : hasIr ? status.file.getFileNameWithoutExtension().toUpperCase()
                              : juce::String ("NO IR LOADED"),
                          inside.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        finish (g, glass);
    }
    body.removeFromTop (6.0f);

    // The response: 40 Hz to 20 kHz across, 0 to -36 dB down.
    const auto glass = screen (g, body);
    {
        auto plot = glass.reduced (glass.getWidth() * 0.04f, glass.getHeight() * 0.1f);
        caption (g, plot, "FREQUENCY RESPONSE");
        auto labels = plot.removeFromBottom (10.0f);
        const auto xFor = [&plot] (float hz)
        {
            return plot.getX() + plot.getWidth() * std::log (hz / CabStage::responseLowHz) / std::log (CabStage::responseHighHz / CabStage::responseLowHz);
        };
        constexpr float floorDb = -36.0f;
        const auto yFor = [&plot, floorDb] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, db / floorDb); };

        // The grid: a line at each decade and every 12 dB, faint, as the tuner's scale.
        g.setFont (font (7.5f));
        for (auto [hz, name] : { std::pair { 100.0f, "100" }, std::pair { 1000.0f, "1k" }, std::pair { 10000.0f, "10k" } })
        {
            g.setColour (orange.withAlpha (0.12f));
            g.drawVerticalLine (juce::roundToInt (xFor (hz)), plot.getY(), plot.getBottom());
            g.setColour (orange.withAlpha (0.55f));
            g.drawText (name, juce::Rectangle<float> (30.0f, labels.getHeight()).withCentre ({ xFor (hz), labels.getCentreY() }), juce::Justification::centred, false);
        }
        for (auto db : { -12.0f, -24.0f })
        {
            g.setColour (orange.withAlpha (0.12f));
            g.drawHorizontalLine (juce::roundToInt (yFor (db)), plot.getX(), plot.getRight());
        }

        const auto& response = status.responseDb;
        if (hasIr && ! response.empty())
        {
            juce::Path curve;
            for (size_t point = 0; point < response.size(); ++point)
            {
                const auto x = plot.getX() + plot.getWidth() * (float) point / (float) (response.size() - 1);
                if (point == 0)
                    curve.startNewSubPath (x, yFor (response[point]));
                else
                    curve.lineTo (x, yFor (response[point]));
            }
            juce::Path fill (curve);
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (plot.toNearestInt());
            g.setGradientFill (juce::ColourGradient (orange.withAlpha (on ? 0.35f : 0.12f), 0.0f, plot.getY(), orange.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
            g.fillPath (fill);
            g.setColour (orange.withAlpha (on ? 0.3f : 0.1f));
            g.strokePath (curve, juce::PathStrokeType (4.0f));
            g.setColour (orange.withAlpha (on ? 1.0f : 0.4f));
            g.strokePath (curve, juce::PathStrokeType (1.5f));
        }
        else
        {
            // No IR: the signal passes straight through, so the line is flat.
            const float dashes[] = { 3.0f, 3.0f };
            g.setColour (orange.withAlpha (0.4f));
            g.drawDashedLine (juce::Line<float> (plot.getX(), yFor (0.0f) + 1.0f, plot.getRight(), yFor (0.0f) + 1.0f), dashes, 2, 1.2f);
        }
    }
    finish (g, glass);
}

void Faceplate::paintPitch (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // The PITCH block: the OCTAVER's voices and FREEZE's level as faders, as
    // a little mixer; TUNING on a screen; OCTAVER and FREEZE on keys.
    nameplate (g, header, "PITCH", "OCTAVER  /  FREEZE  /  TUNING");

    auto controls = body.removeFromRight (body.getWidth() * 0.38f);
    body.removeFromRight (18.0f);
    const auto faderWidth = body.getWidth() / 5.0f;
    for (auto [id, name] : { std::pair { ParamIDs::octSubTwo, "2 DOWN" }, std::pair { ParamIDs::octSubOne, "1 DOWN" },
                             std::pair { ParamIDs::octDry, "DRY" }, std::pair { ParamIDs::octUp, "1 UP" },
                             std::pair { ParamIDs::freezeLevel, "FREEZE" } })
    {
        auto cell = body.removeFromLeft (faderWidth);
        stamped (g, name, cell.removeFromBottom (11.0f), 7.5f);
        g.setColour (Finish::printedRed());
        g.setFont (font (8.0f));
        g.drawText (text (id), cell.removeFromTop (11.0f), juce::Justification::centred, false);
        const auto top = cell.getY() + 8.0f, bottom = cell.getBottom() - 8.0f;
        const auto capY = bottom - (bottom - top) * normalised (id);
        ParagonLookAndFeel::drawFader (g, cell.getCentreX(), top, bottom, bottom, capY, cell.getWidth(), colour);
    }

    readout (g, controls.removeFromTop (30.0f), "TUNING", text (ParamIDs::pitchTune).toUpperCase());
    controls.removeFromTop (10.0f);
    auto keys = controls.withSizeKeepingCentre (controls.getWidth(), juce::jmin (controls.getHeight(), 52.0f));
    const auto half = keys.getWidth() / 2.0f;
    smallKey (g, keys.removeFromLeft (half), "OCTAVER", raw (ParamIDs::octOn) > 0.5f);
    smallKey (g, keys, "FREEZE", raw (ParamIDs::freezeOn) > 0.5f);
}

void Faceplate::paintClassic (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // A studio compressor: a gain-reduction needle on a screen, and its settings.
    nameplate (g, header, "CLASSIC", "STUDIO COMPRESSOR  /  GATE");
    const auto on = isOn();

    auto settings = body.removeFromRight (body.getWidth() * 0.42f);
    body.removeFromRight (14.0f);

    // The meter: the needle and scale drawn in the screen's LED orange.
    const auto glass = screen (g, body);
    const juce::Point<float> pivot (glass.getCentreX(), glass.getBottom() + glass.getHeight() * 0.25f);
    const auto radius = glass.getHeight() * 1.0f;
    const auto angleFor = [] (float db) { return 0.62f - 1.24f * std::pow (juce::jlimit (0.0f, 1.0f, db / 20.0f), 0.7f); };   // 0 dB right
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (glass.toNearestInt());
        g.setFont (font (juce::jmax (7.0f, glass.getHeight() * 0.085f)));
        for (auto db : { 0, 1, 2, 3, 5, 7, 10, 15, 20 })
        {
            const auto angle = angleFor ((float) db);
            const auto numbered = db == 0 || db == 3 || db == 5 || db == 10 || db == 20;
            g.setColour (orange.withAlpha (0.75f));
            g.drawLine (juce::Line<float> (pivot.getPointOnCircumference (radius * (numbered ? 0.88f : 0.92f), angle),
                                           pivot.getPointOnCircumference (radius * 0.98f, angle)), numbered ? 1.4f : 1.0f);
            if (numbered)
                g.drawText (juce::String (db), juce::Rectangle<float> (16.0f, 9.0f).withCentre (pivot.getPointOnCircumference (radius * 0.8f, angle)),
                            juce::Justification::centred, false);
        }
        g.setColour (orange.withAlpha (0.6f));
        g.setFont (font (8.0f));
        g.drawText ("GAIN REDUCTION dB", glass.reduced (8.0f, 5.0f), juce::Justification::bottomLeft, false);
        const auto needle = juce::Line<float> (pivot, pivot.getPointOnCircumference (radius * 0.98f, angleFor (on ? -shownReduction : 0.0f)));
        g.setColour (orange.withAlpha (0.3f));
        g.drawLine (needle, 4.0f);
        g.setColour (orange);
        g.drawLine (needle, 1.6f);
    }
    finish (g, glass);

    // THRESHOLD, RATIO, LEVEL and the GATE, each on its own small screen.
    const auto row = settings.getHeight() / 4.0f;
    for (auto [id, name] : { std::pair { ParamIDs::compThreshold, "THRESHOLD" }, std::pair { ParamIDs::compRatio, "RATIO" },
                             std::pair { ParamIDs::compLevel, "LEVEL" }, std::pair { ParamIDs::gateThreshold, "GATE" } })
        readout (g, settings.removeFromTop (row).reduced (0.0f, 3.0f), name, text (id));
}

void Faceplate::paintDaybreak (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // A pedal compressor: its knobs, INTENSITY on a key, the GATE, and the
    // gain reduction on a screen.
    nameplate (g, header, "DAYBREAK", "PEDAL COMPRESSOR  /  GATE");

    auto meter = body.removeFromBottom (24.0f);
    body.removeFromBottom (6.0f);

    // INTENSITY on one of the unit's small keys, the GATE under it.
    auto side = body.removeFromRight (150.0f);
    readout (g, side.removeFromBottom (26.0f), "GATE", text (ParamIDs::gateThreshold));
    side.removeFromBottom (4.0f);
    smallKey (g, side.withSizeKeepingCentre (78.0f, juce::jmin (side.getHeight(), 52.0f)), "INTENSITY", raw (ParamIDs::compIntensity) > 0.5f);
    body.removeFromRight (8.0f);

    // COMP, DETAIL, MIX and LEVEL: the unit's big knobs.
    const auto cellWidth = body.getWidth() / 4.0f;
    for (auto [id, name] : { std::pair { ParamIDs::compAmount, "COMP" }, std::pair { ParamIDs::compDetail, "DETAIL" },
                             std::pair { ParamIDs::compMix, "MIX" }, std::pair { ParamIDs::compLevel, "LEVEL" } })
        bigKnob (g, body.removeFromLeft (cellWidth).reduced (2.0f, 0.0f), name, normalised (id));

    // Gain reduction on a screen, in LED bars.
    stamped (g, "GAIN REDUCTION", meter.removeFromLeft (92.0f), 8.0f, juce::Justification::centredLeft);
    const auto glass = screen (g, meter);
    bars (g, glass.reduced (glass.getWidth() * 0.03f, glass.getHeight() * 0.28f), isOn() ? -shownReduction / 20.0f : 0.0f, 24);
    finish (g, glass);
}

void Faceplate::paintDelay (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // The delay's TYPE as its name; its repeats on a screen; TIME in digits.
    static const char* names[] = { "DIGITAL DELAY", "ANALOG DELAY", "TAPE ECHO", "MOD DELAY", "REVERSE DELAY",
                                   "MULTI-TAP", "SHIMMER DELAY", "DUCKING DELAY" };
    static_assert (std::size (names) == 8);
    const auto type = juce::jlimit (0, (int) std::size (names) - 1, juce::roundToInt (raw (ParamIDs::delayType)));
    nameplate (g, header, names[type], "ECHO");
    const auto on = isOn();

    // The time it repeats at (seconds), synced or not.
    const auto syncs = (int) std::size (effects::Delay::syncBeats);
    const auto sync = juce::roundToInt (raw (ParamIDs::delaySync));
    const auto synced = sync > 0 && sync < syncs;
    const auto period = juce::jmax (0.001, synced ? (double) effects::Delay::syncBeats[sync] * 60.0 / juce::jmax (20.0, processor.getTempoBpm())
                                                  : (double) raw (ParamIDs::delayTime) / 1000.0);
    const auto feedback = raw (ParamIDs::delayFeedback);

    // The screen: each repeat a bar, as tall as it is loud (FEEDBACK each
    // time), over the next few seconds; a light runs along them at the
    // delay time (in time with it, not with the audio itself).
    auto settings = body.removeFromRight (body.getWidth() * 0.4f);
    body.removeFromRight (14.0f);
    const auto glass = screen (g, body);
    {
        auto plot = glass.reduced (glass.getWidth() * 0.05f, glass.getHeight() * 0.14f);
        caption (g, plot, raw (ParamIDs::delayPingPong) > 0.5f ? "REPEATS  PING-PONG" : "REPEATS");
        const auto span = period * 6.5;
        const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        const auto lane = plot.reduced (0.0f, 2.0f);
        g.setColour (orange.withAlpha (0.15f));
        g.fillRect (lane.withTop (lane.getBottom() - 1.0f));
        const auto ringing = (int) std::fmod (now / period, 6.0);   // the repeat the light is on
        auto level = 1.0f;
        for (int repeat = 0; repeat < 6; ++repeat)
        {
            const auto x = lane.getX() + lane.getWidth() * (float) (((double) repeat + 1.0) * period / span);
            const auto bar = juce::Rectangle<float> (5.0f, lane.getHeight() * juce::jmax (0.04f, level)).withCentre ({ x, 0.0f })
                                 .withBottomY (lane.getBottom());
            const auto lit = on && repeat == ringing;
            g.setColour (orange.withAlpha (lit ? 0.35f : 0.15f));
            g.fillRoundedRectangle (bar.expanded (2.0f, 1.0f), 2.0f);
            g.setColour (orange.withAlpha (on ? (lit ? 1.0f : 0.6f) : 0.3f));
            g.fillRoundedRectangle (bar, 1.5f);
            level *= feedback;
            if (level < 0.02f)
                break;
        }
    }
    finish (g, glass);

    // TIME (the note length while synced), then FEEDBACK and MIX knobs.
    auto timeArea = settings.removeFromTop (settings.getHeight() * 0.38f);
    const auto timeGlass = screen (g, timeArea);
    if (synced)
    {
        g.setColour (orange);
        g.setFont (font (timeGlass.getHeight() * 0.5f));
        g.drawText (effects::Delay::syncNames[sync], timeGlass.reduced (8.0f, 0.0f), juce::Justification::centredRight, false);
    }
    else
    {
        sevenSegment (g, timeGlass, juce::String (juce::roundToInt (period * 1000.0)), "ms");
    }
    finish (g, timeGlass);
    settings.removeFromTop (4.0f);
    const auto half = settings.getWidth() / 2.0f;
    bigKnob (g, settings.removeFromLeft (half), "FEEDBACK", normalised (ParamIDs::delayFeedback));
    bigKnob (g, settings, "MIX", normalised (ParamIDs::delayMix));
}

void Faceplate::paintReverb (juce::Graphics& g, juce::Rectangle<float> header, juce::Rectangle<float> body)
{
    // The reverb's TYPE as its name; its shape on a screen; DECAY in seconds.
    static const char* names[] = { "CLASSIC", "ROOM", "HALL", "CATHEDRAL", "PLATE", "SPRING", "DIGITAL", "REVERSE" };
    static_assert (std::size (names) == 8);
    const auto type = juce::jlimit (0, (int) std::size (names) - 1, juce::roundToInt (raw (ParamIDs::reverbType)));
    nameplate (g, header, names[type], "REVERB");
    const auto on = isOn();
    const auto decay = effects::Reverb::decaySeconds ((effects::Reverb::Type) type, raw (ParamIDs::reverbSize));
    const auto preDelay = raw (ParamIDs::reverbPreDelay) / 1000.0f;

    // The screen: level over time. A gap for PRE-DELAY, then the tail
    // fading over DECAY (rising, for REVERSE; with a spring's ripple, for SPRING).
    auto settings = body.removeFromRight (body.getWidth() * 0.4f);
    body.removeFromRight (14.0f);
    const auto glass = screen (g, body);
    {
        auto plot = glass.reduced (glass.getWidth() * 0.05f, glass.getHeight() * 0.14f);
        caption (g, plot, "PRE-DELAY " + juce::String (juce::roundToInt (preDelay * 1000.0f)) + " ms");
        const auto span = preDelay + decay * 1.15f + 0.05f;
        const auto xFor = [&plot, span] (float seconds) { return plot.getX() + plot.getWidth() * seconds / span; };
        const auto reverse = type == (int) effects::Reverb::Type::reverse;
        const auto spring = type == (int) effects::Reverb::Type::spring;
        juce::Path tail;
        tail.startNewSubPath (xFor (preDelay), plot.getBottom());
        for (auto t = 0.0f; t <= decay; t += decay / 120.0f)
        {
            auto level = reverse ? std::pow (t / decay, 2.0f) : std::pow (10.0f, -3.0f * t / decay);   // 60 dB down over DECAY
            if (spring)
                level *= 0.8f + 0.2f * std::cos (t * 70.0f);
            tail.lineTo (xFor (preDelay + t), plot.getBottom() - plot.getHeight() * 0.9f * level);
        }
        tail.lineTo (xFor (preDelay + decay), plot.getBottom());
        tail.closeSubPath();
        g.setGradientFill (juce::ColourGradient (orange.withAlpha (on ? 0.45f : 0.15f), 0.0f, plot.getY(), orange.withAlpha (0.05f), 0.0f, plot.getBottom(), false));
        g.fillPath (tail);
        g.setColour (orange.withAlpha (on ? 1.0f : 0.4f));
        g.strokePath (tail, juce::PathStrokeType (1.4f));
        g.setColour (orange.withAlpha (0.15f));
        g.fillRect (plot.withTop (plot.getBottom() - 1.0f));
    }
    finish (g, glass);

    // DECAY in seconds, then SIZE and MIX knobs.
    auto decayArea = settings.removeFromTop (settings.getHeight() * 0.38f);
    const auto decayGlass = screen (g, decayArea);
    sevenSegment (g, decayGlass, juce::String (decay, decay < 10.0f ? 1 : 0), "s");
    finish (g, decayGlass);
    settings.removeFromTop (4.0f);
    const auto half = settings.getWidth() / 2.0f;
    bigKnob (g, settings.removeFromLeft (half), "SIZE", normalised (ParamIDs::reverbSize));
    bigKnob (g, settings, "MIX", normalised (ParamIDs::reverbMix));
}
