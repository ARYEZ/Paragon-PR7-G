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

    // Short names for the slot keys and the sequencer's rows.
    juce::String shortName (Cipher::Type type)
    {
        const char* names[] = { "-", "LAD LP", "LAD BP", "LAD HP", "DIG LP", "DIG BP", "DIG HP", "101 LP", "101 BP", "101 HP",
                                "M20 LP", "M20 BP", "M20 HP", "NOTCH", "DUAL", "FORM", "BELL", "LO SH", "HI SH", "GAIN", "PAN",
                                "COMB", "PHASER", "PITCH", "FSHIFT", "RING", "FM", "DIST", "DECIM", "DELAY", "VERB", "REFLCT",
                                "VOCODR", "LOOP", "STRTCH", "VARI", "SLOW", "REV", "GATE", "COMP",
                                "F SHFT", "F ROT", "F TWST", "F MIRR", "F BAND", "F STRT", "F SCAT", "F CASC", "F SMR",
                                "F VERB", "F COMP", "F CORR", "F LOFI", "F SPRD" };
        return names[juce::jlimit (0, (int) std::size (names) - 1, (int) type)];
    }
}

//==============================================================================
// The display: on a slot page the ENVELOPE EDITOR (grid of sixteen, the
// playhead, points to drag; snaps across unless SHIFT is held, as the
// original's SNAP does; START and loop markers with retriggering); on MASTER
// the signal flow; on SEQ the sequencer's grid.
class CipherPanel::Editor final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    explicit Editor (CipherPanel& ownerPanel) : owner (ownerPanel) { updateTooltip(); }

    void updateTooltip()
    {
        setTooltip (owner.shown == CipherPanel::seqPage
                        ? "The sequencer: click or drag to light a slot's steps; click a row's name to pick it for ROTATE, CLEAR and RANDOM"
                        : "The envelope: double-click to add a point, drag one to move it (SHIFT: off the grid), double-click it to remove it; "
                          "drag the small dot between two points up or down to bend that line. With retriggering on, drag the S (START) "
                          "and the loop's edges along the bottom");
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

    //--------------------------------------------------------------------------
    void paint (juce::Graphics& g) override
    {
        glass = LedDisplay::paintScreen (g, getLocalBounds().toFloat(), screenSize);
        if (owner.shown == CipherPanel::masterPage)
            paintChain (g);
        else if (owner.shown == CipherPanel::seqPage)
            paintSequencer (g);
        else if (owner.shown == CipherPanel::macroPage)
            paintMacros (g);
        else
            paintEnvelope (g);
        LedDisplay::paintGlassOverlay (g, glass, screenSize);
    }

    const Cipher::Envelope& envelope() const { return owner.processor.getCipherEnvelope (owner.editIndex()); }

    void paintEnvelope (juce::Graphics& g)
    {
        const auto a = plotArea();
        const auto slot = juce::jmin (owner.shown, 9);
        const auto type = owner.shownType();
        const auto& points = envelope();

        // The title line: the slot, its effect, which envelope, and the main knob's value as it moves.
        auto title = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.06f).removeFromTop (14.0f);
        g.setFont (font (10.0f));
        g.setColour (orange);
        g.drawText ("SLOT " + juce::String (slot + 1) + "  " + Cipher::typeNames[(int) type].toUpperCase() + (owner.editSecond ? "  ENV 2" : ""),
                    title, juce::Justification::centredLeft, false);
        const auto moved = owner.processor.getCipher().getMovedMain (slot);
        const auto labels = Cipher::labelsFor (type);
        if (type != Cipher::Type::off)
        {
            auto value = juce::String (labels.main) + " " + Cipher::mainText (type, moved, juce::roundToInt (owner.raw (owner.slotId ("Option"))));
            // PITCH SNAP shows the note, as the original does.
            if (juce::roundToInt (owner.raw (owner.slotId ("Style"))) == (int) Cipher::Style::snap && Cipher::canSnap (type)
                && type != Cipher::Type::pitchShift && Cipher::isFilter (type))
                value << "  " << Cipher::noteName (Cipher::cutoffHz (moved));
            g.drawText (value, title, juce::Justification::centredRight, false);
        }

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

        // Retriggering: the loop region shaded, the START line.
        const auto retrigger = owner.raw (owner.slotId ("RetrigAudio")) > 0.5f || owner.raw (owner.slotId ("RetrigMidi")) > 0.5f;
        if (retrigger)
        {
            if (owner.raw (owner.slotId ("EnvLoop")) > 0.5f)
            {
                const auto from = owner.raw (owner.slotId ("LoopStart")), to = owner.raw (owner.slotId ("LoopEnd"));
                const auto x0 = a.getX() + a.getWidth() * juce::jmin (from, to), x1 = a.getX() + a.getWidth() * juce::jmax (from, to);
                g.setColour (orange.withAlpha (0.14f));
                g.fillRect (juce::Rectangle<float> (x0, a.getY(), x1 - x0, a.getHeight()));
                g.setColour (orange.withAlpha (0.6f));
                for (auto x : { x0, x1 })
                    g.fillRect (juce::Rectangle<float> (x - 0.5f, a.getBottom() - 10.0f, 1.0f, 10.0f));
            }
            const auto sx = a.getX() + a.getWidth() * owner.raw (owner.slotId ("StartPos"));
            g.setColour (Finish::printedRed().brighter (0.3f));
            g.fillRect (juce::Rectangle<float> (sx - 1.0f, a.getY(), 2.0f, a.getHeight()));
            g.setFont (font (8.0f));
            g.drawText ("S", juce::Rectangle<float> (sx + 2.0f, a.getY(), 10.0f, 10.0f), juce::Justification::centredLeft, false);
        }

        // The envelope, filled underneath like a lit LED band.
        juce::Path line, fill;
        const int steps = juce::jmax (64, (int) a.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto x = (float) i / (float) steps;
            const auto p = toScreen (x, Cipher::envelopeAt (points, x));
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
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto p = toScreen (points[i].x, points[i].y);
            g.setColour (orange);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            if (i + 1 < points.size() && points[i + 1].x - points[i].x > 0.02f)
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
        const auto add = [&] (const juce::String& text, bool activeLine) { lines.add (text); lit.push_back (activeLine); };
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

    // SEQ: ten rows (the slots, named by effect) of 32 steps; the loop region
    // lit, the playing step marked, the picked row's name brighter.
    juce::Rectangle<float> gridArea() const
    {
        auto area = glass.isEmpty() ? getLocalBounds().toFloat().reduced (getWidth() * 0.035f, getHeight() * 0.06f)
                                    : glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.06f);
        area.removeFromTop (16.0f);
        area.removeFromLeft (44.0f);
        return area;
    }

    void paintSequencer (juce::Graphics& g)
    {
        auto title = glass.reduced (glass.getWidth() * 0.035f, glass.getHeight() * 0.06f).removeFromTop (14.0f);
        const auto seqOn = owner.raw (ParamIDs::cipherSeqOn) > 0.5f, midiOn = owner.raw (ParamIDs::cipherMidiOn) > 0.5f;
        g.setFont (font (10.0f));
        g.setColour (orange);
        g.drawText (midiOn ? "SEQUENCE  (MIDI IN CONTROL)" : seqOn ? "SEQUENCE" : "SEQUENCE  (OFF)", title, juce::Justification::centredLeft, false);
        const auto area = gridArea();
        const auto cellW = area.getWidth() / (float) Cipher::numSteps, cellH = area.getHeight() / (float) Cipher::numSlots;
        const auto first = juce::jmin ((int) owner.raw (ParamIDs::cipherSeqStart), (int) owner.raw (ParamIDs::cipherSeqEnd));
        const auto last = juce::jmax ((int) owner.raw (ParamIDs::cipherSeqStart), (int) owner.raw (ParamIDs::cipherSeqEnd));
        const auto playing = owner.processor.getCipher().getStep();
        for (int r = 0; r < Cipher::numSlots; ++r)
        {
            const auto type = (Cipher::Type) juce::roundToInt (owner.raw (ParamIDs::cipherSlot (r + 1, "Type")));
            g.setColour (orange.withAlpha (r == owner.seqRow ? 1.0f : 0.5f));
            g.setFont (font (8.0f));
            g.drawText (juce::String (r + 1) + " " + shortName (type), juce::Rectangle<float> (area.getX() - 44.0f, area.getY() + cellH * (float) r, 42.0f, cellH),
                        juce::Justification::centredLeft, false);
            const auto steps = owner.processor.getCipherSequenceRow (r);
            for (int c = 0; c < Cipher::numSteps; ++c)
            {
                const auto cell = juce::Rectangle<float> (area.getX() + cellW * (float) c, area.getY() + cellH * (float) r, cellW, cellH).reduced (0.8f);
                const auto inLoop = c >= first && c <= last;
                const auto lit = (steps >> (unsigned) c & 1u) != 0;
                g.setColour (orange.withAlpha (lit ? (inLoop ? 0.95f : 0.4f) : (inLoop ? 0.12f : 0.05f)));
                g.fillRect (cell);
            }
        }
        if (seqOn && ! midiOn && owner.raw (ParamIDs::cipherOn) > 0.5f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawRect (juce::Rectangle<float> (area.getX() + cellW * (float) playing, area.getY(), cellW, area.getHeight()), 1.0f);
        }
    }

    // MACRO: what the picked macro moves, one line each (click one to edit it).
    juce::Rectangle<float> linesArea() const
    {
        auto area = (glass.isEmpty() ? getLocalBounds().toFloat() : glass).reduced (getWidth() * 0.04f, getHeight() * 0.07f);
        area.removeFromTop (18.0f);
        return area;
    }

    void paintMacros (juce::Graphics& g)
    {
        auto title = glass.reduced (glass.getWidth() * 0.04f, glass.getHeight() * 0.07f).removeFromTop (14.0f);
        const auto macro = owner.editMacro;
        const auto position = owner.raw (ParamIDs::cipherMacro (macro + 1));
        g.setFont (font (10.0f));
        g.setColour (orange);
        g.drawText (owner.processor.getCipherMacroName (macro), title, juce::Justification::centredLeft, false);
        g.drawText (juce::String (juce::roundToInt (position * 100.0f)) + "%", title, juce::Justification::centredRight, false);
        const auto lines = owner.macroLines();
        const auto area = linesArea();
        if (lines.empty())
        {
            g.setColour (orange.withAlpha (0.5f));
            g.setFont (font (9.0f));
            g.drawFittedText ("NOTHING ASSIGNED: PICK A KNOB UNDER TARGET AND PRESS ADD", area.toNearestInt(), juce::Justification::centred, 2);
            return;
        }
        const auto rowHeight = juce::jmin (14.0f, area.getHeight() / (float) juce::jmax (1, (int) lines.size()));
        for (int i = 0; i < (int) lines.size(); ++i)
        {
            const auto row = juce::Rectangle<float> (area.getX(), area.getY() + rowHeight * (float) i, area.getWidth(), rowHeight);
            if (i == owner.editLine)
            {
                g.setColour (orange.withAlpha (0.18f));
                g.fillRect (row);
            }
            const auto& a = lines[(size_t) i];
            g.setColour (orange.withAlpha (i == owner.editLine ? 1.0f : 0.75f));
            g.setFont (font (9.0f));
            g.drawText (ParagonX1AudioProcessor::cipherMacroTargetName (a.target).toUpperCase(), row.withTrimmedLeft (3.0f), juce::Justification::centredLeft, false);
            const auto depth = juce::roundToInt (a.depth * 100.0f);
            g.drawText ((depth > 0 ? "+" : "") + juce::String (depth) + "%" + (std::abs (a.curve) > 0.005f ? "  C" + juce::String (a.curve, 2) : juce::String()),
                        row.withTrimmedRight (3.0f), juce::Justification::centredRight, false);
            // The bar: how far the knob is moved now.
            const auto now = a.depth * ParagonX1AudioProcessor::cipherMacroCurve (position, a.curve);
            const auto bar = row.withTrimmedTop (rowHeight - 2.0f);
            g.setColour (orange.withAlpha (0.6f));
            const auto centre = bar.getCentreX();
            g.fillRect (juce::Rectangle<float> (juce::jmin (centre, centre + now * bar.getWidth() * 0.5f), bar.getY(), std::abs (now) * bar.getWidth() * 0.5f, 2.0f));
        }
    }

    //--------------------------------------------------------------------------
    juce::Point<float> handle (int segment) const
    {
        const auto& points = envelope();
        const auto mid = (points[(size_t) segment].x + points[(size_t) segment + 1].x) * 0.5f;
        return toScreen (mid, Cipher::envelopeAt (points, mid));
    }

    int pointAt (juce::Point<float> position) const
    {
        const auto& points = envelope();
        for (int i = (int) points.size(); --i >= 0;)
            if (toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (position) < 7.0f)
                return i;
        return -1;
    }

    int handleAt (juce::Point<float> position) const
    {
        const auto& points = envelope();
        for (int i = 0; i + 1 < (int) points.size(); ++i)
            if (points[(size_t) i + 1].x - points[(size_t) i].x > 0.02f && handle (i).getDistanceFrom (position) < 6.0f)
                return i;
        return -1;
    }

    // With retriggering: which marker (START, the loop's start or end) is under the mouse.
    juce::String markerAt (juce::Point<float> position) const
    {
        const auto retrigger = owner.raw (owner.slotId ("RetrigAudio")) > 0.5f || owner.raw (owner.slotId ("RetrigMidi")) > 0.5f;
        if (! retrigger)
            return {};
        const auto a = plotArea();
        const auto isNear = [&] (const char* part) { return std::abs (a.getX() + a.getWidth() * owner.raw (owner.slotId (part)) - position.x) < 5.0f; };
        if (owner.raw (owner.slotId ("EnvLoop")) > 0.5f && position.y > a.getBottom() - 12.0f)
        {
            if (isNear ("LoopStart")) return "LoopStart";
            if (isNear ("LoopEnd")) return "LoopEnd";
        }
        if (isNear ("StartPos") && position.y < a.getY() + 14.0f)
            return "StartPos";
        return {};
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (owner.shown == CipherPanel::masterPage)
            return;
        if (owner.shown == CipherPanel::macroPage)
        {
            const auto area = linesArea();
            const auto lines = owner.macroLines();
            const auto rowHeight = juce::jmin (14.0f, area.getHeight() / (float) juce::jmax (1, (int) lines.size()));
            owner.selectAssignment ((int) ((e.position.y - area.getY()) / rowHeight));
            return;
        }
        if (owner.shown == CipherPanel::seqPage)
        {
            sequencerClick (e.position, true);
            return;
        }
        marker = markerAt (e.position);
        if (marker.isNotEmpty())
        {
            if (auto* p = owner.parameters.getParameter (owner.slotId (marker.toRawUTF8())))
                p->beginChangeGesture();
            return;
        }
        dragging = pointAt (e.position);
        bending = dragging < 0 ? handleAt (e.position) : -1;
        changed = false;
        if (bending >= 0)
            bendStart = envelope()[(size_t) bending].curve;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (owner.shown == CipherPanel::masterPage || owner.shown == CipherPanel::macroPage)
            return;
        if (owner.shown == CipherPanel::seqPage)
        {
            sequencerClick (e.position, false);
            return;
        }
        if (marker.isNotEmpty())
        {
            auto x = fromScreen (e.position).x;
            if (! e.mods.isShiftDown())
                x = std::round (x * 16.0f) / 16.0f;
            owner.setValue (owner.slotId (marker.toRawUTF8()), x);
            repaint();
            return;
        }
        auto points = envelope();
        if (dragging >= 0 && dragging < (int) points.size())
        {
            auto p = fromScreen (e.position);
            if (! e.mods.isShiftDown())
                p.x = std::round (p.x * 16.0f) / 16.0f;
            // A point stays between its neighbours.
            const auto lowest = dragging > 0 ? points[(size_t) dragging - 1].x : 0.0f;
            const auto highest = dragging + 1 < (int) points.size() ? points[(size_t) dragging + 1].x : 1.0f;
            points[(size_t) dragging].x = juce::jlimit (lowest, highest, p.x);
            points[(size_t) dragging].y = p.y;
        }
        else if (bending >= 0 && bending + 1 < (int) points.size())
        {
            // Dragging up bows the line up, down bows it down.
            const auto rising = points[(size_t) bending + 1].y >= points[(size_t) bending].y;
            const auto dy = (float) -e.getDistanceFromDragStartY() / 80.0f;
            points[(size_t) bending].curve = juce::jlimit (-1.0f, 1.0f, bendStart + (rising ? dy : -dy));
        }
        else
            return;
        changed = true;
        owner.processor.setCipherEnvelope (owner.editIndex(), points, false);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (marker.isNotEmpty())
            if (auto* p = owner.parameters.getParameter (owner.slotId (marker.toRawUTF8())))
                p->endChangeGesture();
        marker.clear();
        if (changed)
            owner.processor.setCipherEnvelope (owner.editIndex(), envelope(), true);
        dragging = bending = -1;
        changed = false;
        paintingValue = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (owner.shown >= CipherPanel::masterPage)
            return;
        auto points = envelope();
        if (const auto hit = pointAt (e.position); hit >= 0)
        {
            if (points.size() > 1)
                points.erase (points.begin() + hit);
        }
        else
        {
            auto p = fromScreen (e.position);
            if (! e.mods.isShiftDown())
                p.x = std::round (p.x * 16.0f) / 16.0f;
            points.push_back ({ p.x, p.y, 0.0f });
        }
        owner.processor.setCipherEnvelope (owner.editIndex(), points, true);
        repaint();
    }

    // A click on the grid lights or clears a step (a drag paints the same way);
    // a click on a row's name picks that row.
    void sequencerClick (juce::Point<float> position, bool first)
    {
        const auto area = gridArea();
        const auto r = (int) ((position.y - area.getY()) / (area.getHeight() / (float) Cipher::numSlots));
        if (r < 0 || r >= Cipher::numSlots)
            return;
        if (position.x < area.getX())
        {
            if (first)
                owner.seqRow = r;
            owner.repaint();
            return;
        }
        const auto c = (int) ((position.x - area.getX()) / (area.getWidth() / (float) Cipher::numSteps));
        if (c < 0 || c >= Cipher::numSteps)
            return;
        auto steps = owner.processor.getCipherSequenceRow (r);
        const auto bit = 1u << (unsigned) c;
        if (first)
            paintingValue = (steps & bit) != 0 ? 0 : 1;
        const auto wanted = paintingValue == 1 ? (steps | bit) : (steps & ~bit);
        if (wanted != steps)
            owner.processor.setCipherSequenceRow (r, wanted);
        owner.seqRow = r;
        repaint();
    }

private:
    CipherPanel& owner;
    juce::Rectangle<float> glass;
    int dragging = -1, bending = -1, paintingValue = -1;
    float bendStart = 0.0f;
    bool changed = false;
    juce::String marker;
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
    setUpKey (retrigAudio, "AUDIO", "RETRIGGER AUDIO: a peak in the sound restarts the envelope from START (set the level on the SEQ page)");
    setUpKey (retrigMidi, "MIDI|SEQ", "RETRIGGER MIDI | SEQ: a MIDI note, or the slot coming on in the sequencer, restarts the envelope");
    setUpKey (randomStart, "RND", "RANDOM START: each trigger starts the envelope somewhere new");
    setUpKey (envLoop, "LOOP", "ENVELOPE LOOP: after a trigger, go round (the loop region, if one is set); off, play once and wait");
    setUpKey (driveOn, "DRIVE", "The master drive on or off");
    setUpKey (driveAfter, "FX > DRIVE", "On: the drive after the ten slots (Effects -> Drive). Off: before them (Drive -> Effects)");
    setUpKey (compOn, "COMP", "The master compressor on or off");
    setUpKey (bassOn, "CLEAN BASS", "CLEAN BASS: below its frequency the sound passes untouched by CIPHER");
    setUpKey (bassListen, "LISTEN", "LISTEN: hear only the untouched lows, to set CLEAN BASS's frequency");
    setUpKey (seqOn, "SEQ ON", "The sequencer: slots play only on their lit steps");
    setUpKey (midiOn, "MIDI", "MIDI: slots play while their note is held (C for slot 1, C# for 2 ... A for 10, any octave)");

    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        auto& key = slotKeys[(size_t) i];
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setClickingTogglesState (false);
        key.setTooltip (i < 10 ? "Slot " + juce::String (i + 1) + ": its effect, envelope and knobs"
                        : i == masterPage ? juce::String ("MASTER: the drive, compressor, CLEAN BASS and output")
                        : i == seqPage ? juce::String ("SEQ: the sequencer, MIDI, the audio trigger and follower, and PITCH SNAP's scale")
                                       : juce::String ("MACRO: eight knobs that each move several others at once"));
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
    setUpChoice (shapeList, "SHAPE", "Load a shape into the envelope you're drawing (replaces the points)");
    shapeList.box.addItemList (Cipher::shapeNames, 1);
    shapeList.box.setTextWhenNothingSelected ("LOAD...");
    shapeList.box.onChange = [this]
    {
        if (const auto chosen = shapeList.box.getSelectedItemIndex(); chosen >= 0 && isSlotPage())
        {
            processor.setCipherEnvelope (editIndex(), Cipher::shape (chosen), true);
            shapeList.box.setSelectedId (0, juce::dontSendNotification);
            editor->repaint();
        }
    };
    setUpChoice (modSourceList, "MOD", "The second modulator: the audio follower (how loud you play) or ENVELOPE 2");
    modSourceList.box.addItemList (Cipher::modSourceNames, 1);
    setUpChoice (modDestList, "TO", "Where the second modulator goes");
    modDestList.box.addItemList (Cipher::modDestNames, 1);
    setUpChoice (driveList, "TYPE", "The master drive's type");
    driveList.box.addItemList (Cipher::driveNames, 1);
    setUpChoice (compList, "TYPE", "The master compressor: Squash, Maximizer, Multiband or Overkill");
    compList.box.addItemList (Cipher::compNames, 1);
    setUpChoice (lawList, "MIX LAW", "How MIX blends at half way: -6 dB suits filters, -3 dB echoes and reverbs, 0 dB keeps the dry up");
    lawList.box.addItemList (Cipher::mixLawNames, 1);
    setUpChoice (seqSpeedList, "STEP", "How long each step lasts");
    seqSpeedList.box.addItemList (Cipher::seqSpeedNames, 1);
    juce::StringArray stepNames;
    for (int i = 1; i <= Cipher::numSteps; ++i)
        stepNames.add (juce::String (i));
    setUpChoice (seqStartList, "LOOP", "The loop's first step (it follows the song's position)");
    seqStartList.box.addItemList (stepNames, 1);
    setUpChoice (seqEndList, "TO", "The loop's last step");
    seqEndList.box.addItemList (stepNames, 1);
    setUpChoice (scaleList, "SCALE", "PITCH SNAP's scale (ENV MOD SNAP on a slot)");
    scaleList.box.addItemList (Cipher::scaleNames, 1);
    setUpChoice (rootList, "KEY", "PITCH SNAP's key");
    rootList.box.addItemList (Cipher::rootNames, 1);

    for (int i = 0; i < (int) optionKeys.size(); ++i)
    {
        auto& key = optionKeys[(size_t) i];
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.onClick = [this, i]
        {
            const auto type = shownType();
            setChoice (slotId ("Option"), Cipher::labelsFor (type).modes != nullptr ? Cipher::optionFor (type, i) : i);
        };
        addAndMakeVisible (key);
    }
    const char* styles[] = { "BOTH", "UP", "DOWN", "SNAP" };
    for (int i = 0; i < (int) styleKeys.size(); ++i)
    {
        auto& key = styleKeys[(size_t) i];
        key.setButtonText (styles[i]);
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setTooltip ("ENV MOD STYLE: the envelope moves the main knob both ways, only up, only down, or both ways snapped to "
                        "the notes of the scale (set on the SEQ page)");
        key.onClick = [this, i] { setChoice (slotId ("Style"), i); };
        addAndMakeVisible (key);
    }
    const char* envs[] = { "ENV 1", "ENV 2" };
    for (int i = 0; i < 2; ++i)
    {
        auto& key = envKeys[(size_t) i];
        key.setButtonText (envs[i]);
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setTooltip (i == 0 ? "Draw ENVELOPE 1 (it moves the main knob)" : "Draw ENVELOPE 2 (the second modulator, with MOD set to Envelope 2)");
        key.onClick = [this, i] { editEnvelope2 (i == 1); };
        addAndMakeVisible (key);
    }
    for (auto* key : { &rotateLeft, &rotateRight, &clearKey, &randomKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*key);
    }
    rotateLeft.setTooltip ("ROTATE the picked row's steps one to the left");
    rotateRight.setTooltip ("ROTATE the picked row's steps one to the right");
    clearKey.setTooltip ("CLEAR the picked row's steps");
    randomKey.setTooltip ("RANDOM steps for the picked row, inside the loop");
    rotateLeft.onClick = [this] { editRow ([] (juce::uint32 r) { return (r >> 1) | (r << 31); }); };
    rotateRight.onClick = [this] { editRow ([] (juce::uint32 r) { return (r << 1) | (r >> 31); }); };
    clearKey.onClick = [this] { editRow ([] (juce::uint32) { return 0u; }); };
    randomKey.onClick = [this]
    {
        const auto first = juce::jmin ((int) raw (ParamIDs::cipherSeqStart), (int) raw (ParamIDs::cipherSeqEnd));
        const auto last = juce::jmax ((int) raw (ParamIDs::cipherSeqStart), (int) raw (ParamIDs::cipherSeqEnd));
        editRow ([first, last] (juce::uint32 r)
        {
            auto& random = juce::Random::getSystemRandom();
            for (int c = first; c <= last; ++c)
                r = random.nextBool() ? (r | (1u << (unsigned) c)) : (r & ~(1u << (unsigned) c));
            return r;
        });
    };

    const auto setUpKnob = [this] (Knob& knob, const char* name, const char* tip)
    {
        knob.name = name;
        knob.slider.getProperties().set (ParagonLookAndFeel::pedalKnobProperty, true);
        knob.slider.setPopupDisplayEnabled (true, true, nullptr);
        knob.slider.setTooltip (tip);
        addAndMakeVisible (knob.slider);
    };
    setUpKnob (mainKnob, "MAIN", "The main knob (cutoff, frequency, time...): the envelope moves it");
    setUpKnob (p1Knob, "KNOB 1", "The effect's first knob");
    setUpKnob (p2Knob, "KNOB 2", "The effect's second knob");
    setUpKnob (envKnob, "ENV AMT", "ENV AMOUNT: how far the envelope moves the main knob (left of centre: the other way)");
    setUpKnob (rateKnob, "RATE", "RATE: the envelope's speed in hertz, with SYNC off");
    setUpKnob (glideKnob, "GLIDE", "GLIDE: smooths the envelope's steps on the way to the main knob");
    setUpKnob (stereoKnob, "STEREO", "STEREO: moves the left and right sides' main knob apart");
    setUpKnob (gainKnob, "GAIN", "The slot's output level");
    setUpKnob (mixKnob, "MIX", "The slot's MIX with the sound coming into it");
    setUpKnob (modKnob, "MOD AMT", "How much the second modulator moves its destination (left of centre: the other way)");
    setUpKnob (driveKnob, "DRIVE", "How hard the master drive is pushed");
    setUpKnob (toneKnob, "TONE", "TONE: darker to the left, brighter to the right");
    setUpKnob (compressKnob, "COMPRESS", "How hard the master compressor works");
    setUpKnob (bassKnob, "BASS FREQ", "CLEAN BASS: below this the sound passes untouched");
    setUpKnob (outGainKnob, "GAIN", "CIPHER's output level");
    setUpKnob (outMixKnob, "MIX", "CIPHER's MIX: the dry sound to everything CIPHER does");
    setUpKnob (trigLevelKnob, "LEVEL", "AUDIO TRIGGER: a peak over this restarts the envelopes of slots set to retrigger on AUDIO");
    setUpKnob (trigHpfKnob, "FILTER", "AUDIO TRIGGER: listens through a high-pass (all the way down: off), say to trigger on the pick, not the bass");
    setUpKnob (trigHystKnob, "HYSTERESIS", "AUDIO TRIGGER: no new trigger until the sound falls this far below LEVEL");
    setUpKnob (trigHoldKnob, "HOLD-OFF", "AUDIO TRIGGER: no new trigger for this long after one");
    setUpKnob (followAttackKnob, "ATTACK", "AUDIO FOLLOWER: how fast it rises with the sound");
    setUpKnob (followHoldKnob, "HOLD", "AUDIO FOLLOWER: how long it stays up after rising");
    setUpKnob (followDecayKnob, "DECAY", "AUDIO FOLLOWER: how fast it falls back when the sound stops");
    setUpKnob (followGateKnob, "GATE", "AUDIO FOLLOWER: sound below this doesn't move it (keeps hiss from moving things)");

    // The MACRO page: the eight macro knobs (parameters: the DAW can automate
    // them, and the expression pedal can play MACRO 1), a key each to pick
    // which one to edit, its name, and what it moves.
    for (int m = 0; m < 8; ++m)
    {
        auto& knob = macroKnobs[(size_t) m];
        setUpKnob (knob, "MACRO", "A macro: turn it to move every knob assigned to it");
        knob.id = ParamIDs::cipherMacro (m + 1);
        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (parameters, knob.id, knob.slider);
        auto& key = macroKeys[(size_t) m];
        key.setButtonText (juce::String (m + 1));
        key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        key.setTooltip ("Edit macro " + juce::String (m + 1) + ": its name and what it moves");
        key.onClick = [this, m] { selectMacro (m); };
        addAndMakeVisible (key);
    }
    nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    nameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, orange.withAlpha (0.5f));
    nameEditor.setColour (juce::TextEditor::textColourId, orange);
    nameEditor.setFont (font (13.0f));
    nameEditor.setInputRestrictions (16);
    nameEditor.setTooltip ("The macro's name: type and press Enter");
    nameEditor.onReturnKey = [this] { processor.setCipherMacroName (editMacro, nameEditor.getText()); repaint(); };
    nameEditor.onFocusLost = nameEditor.onReturnKey;
    addAndMakeVisible (nameEditor);
    setUpChoice (targetList, "TARGET", "The knob to move: pick it, then ADD (or pick another to change the line you've picked)");
    for (const auto& id : ParagonX1AudioProcessor::cipherMacroTargets())
        targetList.box.addItem (ParagonX1AudioProcessor::cipherMacroTargetName (id), targetList.box.getNumItems() + 1);
    targetList.box.onChange = [this]
    {
        const auto chosen = targetList.box.getSelectedItemIndex();
        if (chosen >= 0 && editLine >= 0)
            changeAssignment ([chosen] (auto& a) { a.target = ParagonX1AudioProcessor::cipherMacroTargets()[chosen]; });
    };
    for (auto* key : { &addKey, &removeKey, &clearMacroKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        addAndMakeVisible (*key);
    }
    addKey.setTooltip ("ADD the knob under TARGET to this macro (half its travel to start with)");
    removeKey.setTooltip ("REMOVE the line you've picked from this macro");
    clearMacroKey.setTooltip ("CLEAR everything from this macro");
    addKey.onClick = [this]
    {
        auto all = processor.getCipherMacroAssignments();
        if ((int) all.size() >= ParagonX1AudioProcessor::cipherMaxAssignments)
            return;
        const auto chosen = juce::jmax (0, targetList.box.getSelectedItemIndex());
        all.push_back ({ editMacro, ParagonX1AudioProcessor::cipherMacroTargets()[chosen], 0.5f, 0.0f });
        processor.setCipherMacroAssignments (all);
        editLine = (int) macroLines().size() - 1;
        timerCallback();
        editor->repaint();
    };
    removeKey.onClick = [this]
    {
        auto all = processor.getCipherMacroAssignments();
        auto seen = 0;
        for (auto it = all.begin(); it != all.end(); ++it)
            if (it->macro == editMacro && seen++ == editLine)
            {
                all.erase (it);
                break;
            }
        processor.setCipherMacroAssignments (all);
        editLine = juce::jmin (editLine, (int) macroLines().size() - 1);
        editor->repaint();
    };
    clearMacroKey.onClick = [this]
    {
        auto all = processor.getCipherMacroAssignments();
        all.erase (std::remove_if (all.begin(), all.end(), [this] (const auto& a) { return a.macro == editMacro; }), all.end());
        processor.setCipherMacroAssignments (all);
        editLine = -1;
        editor->repaint();
    };
    for (auto* knob : { &depthKnob, &curveKnob })
    {
        knob->slider.setRange (-1.0, 1.0, 0.01);
        knob->slider.setDoubleClickReturnValue (true, knob == &depthKnob ? 0.5 : 0.0);
    }
    setUpKnob (depthKnob, "DEPTH", "How far the picked line's knob moves at the macro's top (left of centre: the other way)");
    setUpKnob (curveKnob, "CURVE", "How the picked line's knob moves along the macro's travel: straight at the centre, early to the right, late to the left");
    depthKnob.slider.onValueChange = [this] { const auto v = (float) depthKnob.slider.getValue(); changeAssignment ([v] (auto& a) { a.depth = v; }); };
    curveKnob.slider.onValueChange = [this] { const auto v = (float) curveKnob.slider.getValue(); changeAssignment ([v] (auto& a) { a.curve = v; }); };

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

void CipherPanel::setChoice (const juce::String& id, int index) { setValue (id, (float) index); }

void CipherPanel::setValue (const juce::String& id, float plain)
{
    if (auto* parameter = parameters.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
        parameter->endChangeGesture();
    }
}

void CipherPanel::editRow (const std::function<juce::uint32 (juce::uint32)>& change)
{
    processor.setCipherSequenceRow (seqRow, change (processor.getCipherSequenceRow (seqRow)));
    editor->repaint();
}

void CipherPanel::showSlot (int slot)
{
    shown = juce::jlimit (0, macroPage, slot);
    editor->updateTooltip();
    bind();
    resized();
    repaint();
}

void CipherPanel::editEnvelope2 (bool second)
{
    editSecond = second;
    editor->repaint();
    timerCallback();
}

std::vector<ParagonX1AudioProcessor::CipherMacroAssignment> CipherPanel::macroLines() const
{
    std::vector<ParagonX1AudioProcessor::CipherMacroAssignment> lines;
    for (const auto& a : processor.getCipherMacroAssignments())
        if (a.macro == editMacro)
            lines.push_back (a);
    return lines;
}

void CipherPanel::changeAssignment (const std::function<void (ParagonX1AudioProcessor::CipherMacroAssignment&)>& change)
{
    if (editLine < 0)
        return;
    auto all = processor.getCipherMacroAssignments();
    auto seen = 0;
    for (auto& a : all)
        if (a.macro == editMacro && seen++ == editLine)
        {
            auto changed = a;
            change (changed);
            if (changed == a)
                return;
            a = changed;
            break;
        }
    processor.setCipherMacroAssignments (all);
    editor->repaint();
}

void CipherPanel::selectMacro (int macro)
{
    editMacro = juce::jlimit (0, 7, macro);
    editLine = macroLines().empty() ? -1 : 0;
    nameEditor.setText (processor.getCipherMacroName (editMacro), juce::dontSendNotification);
    timerCallback();
    editor->repaint();
}

void CipherPanel::selectAssignment (int line)
{
    const auto count = (int) macroLines().size();
    editLine = line >= 0 && line < count ? line : editLine;
    timerCallback();
    editor->repaint();
}

void CipherPanel::bind()
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    const auto slotPage = isSlotPage();
    const auto master = shown == masterPage, seq = shown == seqPage, macros = shown == macroPage;
    for (auto& knob : macroKnobs)
        knob.slider.setVisible (macros);
    for (auto& key : macroKeys)
        key.setVisible (macros);
    for (auto* c : std::initializer_list<juce::Component*> { &nameEditor, &targetList.box, &addKey, &removeKey, &clearMacroKey,
                                                             &depthKnob.slider, &curveKnob.slider })
        c->setVisible (macros);

    // A slot's controls follow the slot shown.
    for (auto* knob : { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob, &modKnob })
        knob->attachment.reset();
    for (auto* key : { &slotOn, &syncKey, &retrigAudio, &retrigMidi, &randomStart, &envLoop })
        key->attachment.reset();
    for (auto* choice : { &typeList, &speedList, &modSourceList, &modDestList })
        choice->attachment.reset();
    if (slotPage)
    {
        const std::pair<Knob*, const char*> knobs[] = { { &mainKnob, "Main" }, { &p1Knob, "P1" }, { &p2Knob, "P2" }, { &envKnob, "EnvAmount" },
                                                        { &rateKnob, "Rate" }, { &glideKnob, "Glide" }, { &stereoKnob, "Stereo" },
                                                        { &gainKnob, "Gain" }, { &mixKnob, "Mix" }, { &modKnob, "ModAmount" } };
        for (const auto& [knob, part] : knobs)
        {
            knob->id = slotId (part);
            knob->attachment = std::make_unique<SA> (parameters, knob->id, knob->slider);
        }
        const std::pair<Key*, const char*> keys[] = { { &slotOn, "On" }, { &syncKey, "Sync" }, { &retrigAudio, "RetrigAudio" },
                                                      { &retrigMidi, "RetrigMidi" }, { &randomStart, "RandomStart" }, { &envLoop, "EnvLoop" } };
        for (const auto& [key, part] : keys)
            key->attachment = std::make_unique<BA> (parameters, slotId (part), key->button);
        const std::pair<Choice*, const char*> choices[] = { { &typeList, "Type" }, { &speedList, "Speed" }, { &modSourceList, "ModSource" },
                                                            { &modDestList, "ModDest" } };
        for (const auto& [choice, part] : choices)
            choice->attachment = std::make_unique<CA> (parameters, slotId (part), choice->box);
    }
    // The master's and the sequencer page's controls are bound once.
    if (driveKnob.attachment == nullptr)
    {
        const std::pair<Knob*, const char*> knobs[] = {
            { &driveKnob, ParamIDs::cipherDrive }, { &toneKnob, ParamIDs::cipherTone }, { &compressKnob, ParamIDs::cipherCompress },
            { &bassKnob, ParamIDs::cipherCleanBassHz }, { &outGainKnob, ParamIDs::cipherGain }, { &outMixKnob, ParamIDs::cipherMix },
            { &trigLevelKnob, ParamIDs::cipherTrigLevel }, { &trigHpfKnob, ParamIDs::cipherTrigHpf },
            { &trigHystKnob, ParamIDs::cipherTrigHysteresis }, { &trigHoldKnob, ParamIDs::cipherTrigHoldOff },
            { &followAttackKnob, ParamIDs::cipherFollowAttack }, { &followHoldKnob, ParamIDs::cipherFollowHold },
            { &followDecayKnob, ParamIDs::cipherFollowDecay }, { &followGateKnob, ParamIDs::cipherFollowGate } };
        for (const auto& [knob, id] : knobs)
        {
            knob->id = id;
            knob->attachment = std::make_unique<SA> (parameters, id, knob->slider);
        }
        const std::pair<Key*, const char*> keys[] = { { &driveOn, ParamIDs::cipherDriveOn }, { &driveAfter, ParamIDs::cipherDriveAfter },
                                                      { &compOn, ParamIDs::cipherCompOn }, { &bassOn, ParamIDs::cipherCleanBassOn },
                                                      { &bassListen, ParamIDs::cipherCleanBassListen }, { &seqOn, ParamIDs::cipherSeqOn },
                                                      { &midiOn, ParamIDs::cipherMidiOn } };
        for (const auto& [key, id] : keys)
            key->attachment = std::make_unique<BA> (parameters, id, key->button);
        const std::pair<Choice*, const char*> choices[] = { { &driveList, ParamIDs::cipherDriveType }, { &compList, ParamIDs::cipherComp },
                                                            { &lawList, ParamIDs::cipherMixLaw }, { &seqSpeedList, ParamIDs::cipherSeqSpeed },
                                                            { &seqStartList, ParamIDs::cipherSeqStart }, { &seqEndList, ParamIDs::cipherSeqEnd },
                                                            { &scaleList, ParamIDs::cipherScale }, { &rootList, ParamIDs::cipherScaleRoot } };
        for (const auto& [choice, id] : choices)
            choice->attachment = std::make_unique<CA> (parameters, id, choice->box);
    }

    for (auto* c : std::initializer_list<juce::Component*> {
             &slotOn.button, &syncKey.button, &retrigAudio.button, &retrigMidi.button, &randomStart.button, &envLoop.button,
             &typeList.box, &speedList.box, &shapeList.box, &modSourceList.box, &modDestList.box,
             &mainKnob.slider, &p1Knob.slider, &p2Knob.slider, &envKnob.slider, &rateKnob.slider, &glideKnob.slider, &stereoKnob.slider,
             &gainKnob.slider, &mixKnob.slider, &modKnob.slider, &envKeys[0], &envKeys[1] })
        c->setVisible (slotPage);
    for (auto& key : styleKeys)
        key.setVisible (slotPage);
    for (auto* c : std::initializer_list<juce::Component*> { &driveOn.button, &driveAfter.button, &compOn.button, &bassOn.button,
                                                             &bassListen.button, &driveList.box, &compList.box, &lawList.box,
                                                             &driveKnob.slider, &toneKnob.slider, &compressKnob.slider, &bassKnob.slider,
                                                             &outGainKnob.slider, &outMixKnob.slider })
        c->setVisible (master);
    for (auto* c : std::initializer_list<juce::Component*> { &seqOn.button, &midiOn.button, &seqSpeedList.box, &seqStartList.box, &seqEndList.box,
                                                             &scaleList.box, &rootList.box, &rotateLeft, &rotateRight, &clearKey, &randomKey,
                                                             &trigLevelKnob.slider, &trigHpfKnob.slider, &trigHystKnob.slider, &trigHoldKnob.slider,
                                                             &followAttackKnob.slider, &followHoldKnob.slider, &followDecayKnob.slider,
                                                             &followGateKnob.slider })
        c->setVisible (seq);
    if (! slotPage)
        for (auto& key : optionKeys)
            key.setVisible (false);
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

    // The slot keys show each slot's effect; the shown page's key is lit.
    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        auto& key = slotKeys[(size_t) i];
        const auto text = i < 10 ? juce::String (i + 1) + " " + shortName ((Cipher::Type) juce::roundToInt (raw (ParamIDs::cipherSlot (i + 1, "Type"))))
                                 : juce::String (i == masterPage ? "MASTER" : i == seqPage ? "SEQ" : "MACRO");
        if (key.getButtonText() != text)
            key.setButtonText (text);
        key.setToggleState (i == shown, juce::dontSendNotification);
    }
    envKeys[0].setToggleState (! editSecond, juce::dontSendNotification);
    envKeys[1].setToggleState (editSecond, juce::dontSendNotification);

    const auto type = shownType();
    const auto labels = Cipher::labelsFor (type);
    const auto option = juce::roundToInt (raw (slotId ("Option")));
    const auto style = juce::roundToInt (raw (slotId ("Style")));
    const auto slotPage = isSlotPage();
    for (int i = 0; i < (int) optionKeys.size(); ++i)
    {
        auto& key = optionKeys[(size_t) i];
        const auto modes = labels.modes;
        const auto show = slotPage && (modes != nullptr ? i < modes->size() : (labels.hasSlope && i < 4));
        key.setVisible (show);
        if (show)
            key.setButtonText (modes != nullptr ? (*modes)[i] : Cipher::slopeNames[i]);
        key.setToggleState (modes != nullptr ? i == Cipher::modeOf (type, option) : i == option, juce::dontSendNotification);
    }
    for (int i = 0; i < (int) styleKeys.size(); ++i)
        styleKeys[(size_t) i].setToggleState (i == style, juce::dontSendNotification);
    if (slotPage)
    {
        p1Knob.slider.setEnabled (*labels.p1 != 0);
        p2Knob.slider.setEnabled (*labels.p2 != 0);
        mainKnob.slider.setEnabled (type != Cipher::Type::off);
        const auto synced = raw (slotId ("Sync")) > 0.5f;
        rateKnob.slider.setEnabled (! synced);
        speedList.box.setEnabled (synced);
        const auto retrigger = raw (slotId ("RetrigAudio")) > 0.5f || raw (slotId ("RetrigMidi")) > 0.5f;
        randomStart.button.setEnabled (retrigger);
        envLoop.button.setEnabled (retrigger);
    }

    // The MACRO page: each key shows its macro's name; the picked line's DEPTH and CURVE.
    for (int m = 0; m < 8; ++m)
    {
        const auto name = processor.getCipherMacroName (m);
        if (macroKeys[(size_t) m].getButtonText() != name)
            macroKeys[(size_t) m].setButtonText (name);
        macroKeys[(size_t) m].setToggleState (m == editMacro, juce::dontSendNotification);
        macroKnobs[(size_t) m].name = name;
    }
    if (shown == macroPage)
    {
        if (! nameEditor.hasKeyboardFocus (true))
            nameEditor.setText (processor.getCipherMacroName (editMacro), juce::dontSendNotification);
        const auto lines = macroLines();
        if ((editLine < 0 || editLine >= (int) lines.size()) && ! lines.empty())
            editLine = 0;
        const auto picked = editLine >= 0 && editLine < (int) lines.size();
        depthKnob.slider.setEnabled (picked);
        curveKnob.slider.setEnabled (picked);
        if (picked)
        {
            depthKnob.slider.setValue (lines[(size_t) editLine].depth, juce::dontSendNotification);
            curveKnob.slider.setValue (lines[(size_t) editLine].curve, juce::dontSendNotification);
            targetList.box.setSelectedItemIndex (ParagonX1AudioProcessor::cipherMacroTargets().indexOf (lines[(size_t) editLine].target),
                                                 juce::dontSendNotification);
        }
        editor->repaint();
    }

    // A new type can bring different keys: lay them out again.
    if ((int) type != laidOutType)
    {
        laidOutType = (int) type;
        resized();
    }

    // The display moves with the envelope or the sequencer.
    if (editor->isVisible() && (shown == seqPage || (slotPage && type != Cipher::Type::off)))
        editor->repaint();

    juce::String state = juce::String (shown) + "|" + juce::String ((int) type) + "|" + juce::String (option) + "|" + juce::String ((int) editSecond);
    for (auto* knob : { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob, &modKnob,
                        &driveKnob, &toneKnob, &compressKnob, &bassKnob, &outGainKnob, &outMixKnob, &trigLevelKnob, &trigHpfKnob,
                        &trigHystKnob, &trigHoldKnob, &followAttackKnob, &followHoldKnob, &followDecayKnob, &followGateKnob,
                        &macroKnobs[0], &macroKnobs[1], &macroKnobs[2], &macroKnobs[3], &macroKnobs[4], &macroKnobs[5],
                        &macroKnobs[6], &macroKnobs[7], &depthKnob, &curveKnob })
        if (knob->slider.isVisible())
            state << "|" << knob->slider.getValue();
    if (shown == seqPage)
        state << "|" << juce::roundToInt (processor.getCipher().getFollower() * 20.0f);
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
    const auto keyWidth = juce::jmin (64, (keys.getWidth() - 13 * 4 - 24) / 13);
    for (int i = 0; i < (int) slotKeys.size(); ++i)
    {
        slotKeys[(size_t) i].setBounds (keys.removeFromLeft (i == masterPage ? keyWidth + 12 : i == macroPage ? keyWidth + 8 : i == seqPage ? keyWidth + 4 : keyWidth));
        keys.removeFromLeft (4);
    }

    area.removeFromTop (8);
    auto row2 = area.removeFromTop (24);
    area.removeFromTop (6);
    auto row3 = area.removeFromTop (24);
    area.removeFromTop (6);

    const auto placeChoice = [] (Choice& c, juce::Rectangle<int>& row, int labelWidth, int width)
    {
        c.label = row.removeFromLeft (labelWidth).toFloat();
        c.box.setBounds (row.removeFromLeft (width).reduced (0, 1));
        row.removeFromLeft (12);
    };
    const auto placeKey = [] (Key& k, juce::Rectangle<int>& row, int labelWidth)
    {
        k.label = row.removeFromLeft (labelWidth).toFloat();
        k.button.setBounds (row.removeFromLeft (48).withSizeKeepingCentre (44, 24));
        row.removeFromLeft (10);
    };

    if (isSlotPage())
    {
        auto row4 = area.removeFromTop (24);
        area.removeFromTop (6);
        placeChoice (typeList, row2, 40, 150);
        optionLabel = row2.removeFromLeft (56).toFloat();
        const auto labels = Cipher::labelsFor (shownType());
        const auto keyCount = labels.modes != nullptr ? labels.modes->size() : labels.hasSlope ? 4 : 0;
        for (int i = 0; i < (int) optionKeys.size(); ++i)
        {
            if (i >= keyCount)
                continue;
            optionKeys[(size_t) i].setBounds (row2.removeFromLeft (labels.modes != nullptr ? 76 : 38));
            row2.removeFromLeft (4);
        }

        styleLabel = row3.removeFromLeft (62).toFloat();
        for (auto& key : styleKeys)
        {
            key.setBounds (row3.removeFromLeft (48));
            row3.removeFromLeft (4);
        }
        row3.removeFromLeft (8);
        placeKey (syncKey, row3, 40);
        placeChoice (speedList, row3, 48, 104);
        placeChoice (shapeList, row3, 48, 104);
        placeKey (slotOn, row3, 36);

        // Row 4: RETRIGGER, the second modulator, which envelope to draw.
        retrigLabel = row4.removeFromLeft (44).toFloat();
        for (auto* key : { &retrigAudio, &retrigMidi, &randomStart, &envLoop })
            placeKey (*key, row4, key == &retrigAudio ? 38 : key == &retrigMidi ? 52 : 28);
        placeChoice (modSourceList, row4, 32, 112);
        placeChoice (modDestList, row4, 22, 72);
        envLabel = row4.removeFromLeft (4).toFloat();
        for (auto& key : envKeys)
        {
            key.setBounds (row4.removeFromLeft (50));
            row4.removeFromLeft (4);
        }

        layoutRow (area, { &mainKnob, &p1Knob, &p2Knob, &envKnob, &rateKnob, &glideKnob, &stereoKnob, &gainKnob, &mixKnob, &modKnob });
    }
    else if (shown == masterPage)
    {
        placeKey (driveOn, row2, 46);
        placeChoice (driveList, row2, 36, 120);
        placeKey (driveAfter, row2, 76);
        placeKey (compOn, row2, 44);
        placeChoice (compList, row2, 36, 120);

        placeKey (bassOn, row3, 84);
        placeKey (bassListen, row3, 52);
        placeChoice (lawList, row3, 60, 90);

        area.removeFromTop (16);
        layoutRow (area, { &driveKnob, &toneKnob, &compressKnob, &bassKnob, &outGainKnob, &outMixKnob });
    }
    else if (shown == macroPage)
    {
        for (auto& key : macroKeys)
        {
            key.setBounds (row2.removeFromLeft (84));
            row2.removeFromLeft (4);
        }
        nameLabel = row3.removeFromLeft (48).toFloat();
        nameEditor.setBounds (row3.removeFromLeft (170).reduced (0, 1));
        row3.removeFromLeft (16);
        placeChoice (targetList, row3, 52, 190);
        for (auto* key : { &addKey, &removeKey, &clearMacroKey })
        {
            key->setBounds (row3.removeFromLeft (70));
            row3.removeFromLeft (4);
        }
        area.removeFromTop (8);
        layoutRow (area, { &macroKnobs[0], &macroKnobs[1], &macroKnobs[2], &macroKnobs[3], &macroKnobs[4], &macroKnobs[5],
                           &macroKnobs[6], &macroKnobs[7], &depthKnob, &curveKnob });
    }
    else
    {
        placeKey (seqOn, row2, 54);
        placeKey (midiOn, row2, 36);
        placeChoice (seqSpeedList, row2, 44, 70);
        placeChoice (seqStartList, row2, 42, 56);
        placeChoice (seqEndList, row2, 28, 56);
        rowLabel = row2.removeFromLeft (48).toFloat();
        for (auto* key : { &rotateLeft, &rotateRight, &clearKey, &randomKey })
        {
            key->setBounds (row2.removeFromLeft (key == &rotateLeft || key == &rotateRight ? 30 : key == &clearKey ? 64 : 76));
            row2.removeFromLeft (4);
        }

        placeChoice (scaleList, row3, 48, 150);
        placeChoice (rootList, row3, 32, 56);

        area.removeFromTop (16);
        auto knobs = area;
        trigLabel = knobs.withHeight (12).withWidth (knobs.getWidth() / 2).toFloat().translated (0.0f, -14.0f);
        followLabel = trigLabel.translated ((float) knobs.getWidth() / 2.0f, 0.0f);
        layoutRow (knobs, { &trigLevelKnob, &trigHpfKnob, &trigHystKnob, &trigHoldKnob,
                            &followAttackKnob, &followHoldKnob, &followDecayKnob, &followGateKnob });
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
        stamped (g, c.name, c.label.withTrimmedRight (5.0f), 9.0f, juce::Justification::centredRight);
        glasses.push_back (LedDisplay::paintScreen (g, c.box.getBounds().toFloat().expanded (2.0f, 3.0f), 0.35f));
    };
    const auto drawKey = [&] (Key& k)
    {
        if (k.button.isVisible() && k.name.isNotEmpty() && ! k.label.isEmpty())
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
    const auto percent = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    const auto ms = [] (float t) { return t < 1000.0f ? juce::String (t < 10.0f ? t : (float) juce::roundToInt (t), t < 10.0f ? 1 : 0) + " ms"
                                                      : juce::String (t / 1000.0f, 1) + " s"; };

    if (isSlotPage())
    {
        for (auto* c : { &typeList, &speedList, &shapeList, &modSourceList, &modDestList })
            drawChoice (*c);
        for (auto* k : { &slotOn, &syncKey, &retrigAudio, &retrigMidi, &randomStart, &envLoop })
            drawKey (*k);
        if (labels.hasSlope || labels.modes != nullptr)
            stamped (g, labels.modes == &Cipher::fftSizeNames ? "FFT SIZE" : labels.modes != nullptr ? "MODE" : "SLOPE",
                     optionLabel, 9.0f, juce::Justification::centredRight);
        stamped (g, "ENV MOD", styleLabel, 9.0f, juce::Justification::centredRight);
        stamped (g, "RETRIG", retrigLabel, 9.0f, juce::Justification::centredLeft);

        const auto value = [this] (const char* part) { return raw (slotId (part)); };
        drawKnob (mainKnob, labels.main[0] != 0 ? labels.main : "MAIN", Cipher::mainText (type, value ("Main"), juce::roundToInt (value ("Option"))));
        drawKnob (p1Knob, labels.p1[0] != 0 ? labels.p1 : "-", Cipher::p1Text (type, value ("P1")));
        drawKnob (p2Knob, labels.p2[0] != 0 ? labels.p2 : "-", Cipher::p2Text (type, value ("P2")));
        drawKnob (envKnob, "ENV AMT", percent (value ("EnvAmount")));
        drawKnob (rateKnob, "RATE", juce::String (value ("Rate"), 2) + " Hz");
        drawKnob (glideKnob, "GLIDE", juce::String (juce::roundToInt (value ("Glide"))) + " ms");
        drawKnob (stereoKnob, labels.stereo, Cipher::stereoText (type, value ("Stereo")));
        drawKnob (gainKnob, "GAIN", juce::String (value ("Gain"), 1) + " dB");
        drawKnob (mixKnob, "MIX", percent (value ("Mix")));
        drawKnob (modKnob, "MOD AMT", percent (value ("ModAmount")));
    }
    else if (shown == masterPage)
    {
        drawChoice (driveList);
        drawChoice (compList);
        drawChoice (lawList);
        for (auto* key : { &driveOn, &driveAfter, &compOn, &bassOn, &bassListen })
            drawKey (*key);
        drawKnob (driveKnob, "DRIVE", percent (raw (ParamIDs::cipherDrive)));
        drawKnob (toneKnob, "TONE", percent (raw (ParamIDs::cipherTone)));
        drawKnob (compressKnob, "COMPRESS", percent (raw (ParamIDs::cipherCompress)));
        drawKnob (bassKnob, "BASS FREQ", juce::String (juce::roundToInt (raw (ParamIDs::cipherCleanBassHz))) + " Hz");
        drawKnob (outGainKnob, "GAIN", juce::String (raw (ParamIDs::cipherGain), 1) + " dB");
        drawKnob (outMixKnob, "MIX", percent (raw (ParamIDs::cipherMix)));
    }
    else if (shown == macroPage)
    {
        stamped (g, "NAME", nameLabel.withTrimmedRight (6.0f), 9.0f, juce::Justification::centredRight);
        glasses.push_back (LedDisplay::paintScreen (g, nameEditor.getBounds().toFloat().expanded (2.0f, 3.0f), 0.35f));
        drawChoice (targetList);
        for (int m = 0; m < 8; ++m)
            drawKnob (macroKnobs[(size_t) m], processor.getCipherMacroName (m), percent (raw (ParamIDs::cipherMacro (m + 1))));
        drawKnob (depthKnob, "DEPTH", percent ((float) depthKnob.slider.getValue()));
        drawKnob (curveKnob, "CURVE", juce::String (curveKnob.slider.getValue(), 2));
    }
    else
    {
        for (auto* c : { &seqSpeedList, &seqStartList, &seqEndList, &scaleList, &rootList })
            drawChoice (*c);
        drawKey (seqOn);
        drawKey (midiOn);
        stamped (g, "ROW " + juce::String (seqRow + 1), rowLabel, 9.0f, juce::Justification::centredRight);
        // The two groups' names, printed in the unit's red, with the follower's level as a bar.
        g.setColour (Finish::printedRed());
        g.setFont (font (9.0f));
        g.drawText ("AUDIO TRIGGER", trigLabel, juce::Justification::centredLeft, false);
        g.drawText ("AUDIO FOLLOWER", followLabel, juce::Justification::centredLeft, false);
        const auto level = processor.getCipher().getFollower();
        const auto bar = followLabel.withTrimmedLeft (100.0f).withWidth (120.0f).reduced (0.0f, 3.0f);
        g.setColour (orange.withAlpha (0.2f));
        g.fillRect (bar);
        g.setColour (orange);
        g.fillRect (bar.withWidth (bar.getWidth() * level));
        const auto trigHz = raw (ParamIDs::cipherTrigHpf);
        drawKnob (trigLevelKnob, "LEVEL", juce::String (raw (ParamIDs::cipherTrigLevel), 1) + " dB");
        drawKnob (trigHpfKnob, "FILTER", trigHz <= 25.0f ? juce::String ("OFF") : juce::String (juce::roundToInt (trigHz)) + " Hz");
        drawKnob (trigHystKnob, "HYSTERESIS", juce::String (raw (ParamIDs::cipherTrigHysteresis), 1) + " dB");
        drawKnob (trigHoldKnob, "HOLD-OFF", ms (raw (ParamIDs::cipherTrigHoldOff)));
        drawKnob (followAttackKnob, "ATTACK", ms (raw (ParamIDs::cipherFollowAttack)));
        drawKnob (followHoldKnob, "HOLD", ms (raw (ParamIDs::cipherFollowHold)));
        drawKnob (followDecayKnob, "DECAY", ms (raw (ParamIDs::cipherFollowDecay)));
        drawKnob (followGateKnob, "GATE", juce::String (raw (ParamIDs::cipherFollowGate), 1) + " dB");
    }
}

void CipherPanel::paintOverChildren (juce::Graphics& g)
{
    for (const auto& glass : glasses)
        LedDisplay::paintGlassOverlay (g, glass, 0.35f);
}
