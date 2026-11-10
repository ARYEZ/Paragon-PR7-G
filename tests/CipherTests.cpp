// CIPHER, the envelope-controlled filter and multi-effect (inspired by
// Devious Machines' Infiltrator 2): each filter and utility checked on what
// it does to the sound, the envelope and its LFO, and the master section.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/effects/Cipher.h"
#include "PluginProcessor.h"
#include <set>

using Catch::Matchers::WithinAbs;
using effects::Cipher;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    float dB (float gain) { return 20.0f * std::log10 (juce::jmax (1.0e-9f, gain)); }

    // Runs `seconds` of a sine (or the given signal) through CIPHER and
    // returns both sides.
    juce::AudioBuffer<float> run (Cipher& cipher, const Cipher::Settings& settings, double hz, double seconds, float level = 0.25f,
                                  std::function<void (Cipher::Settings&, double)> perBlock = {})
    {
        const auto length = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> out (2, length);
        auto s = settings;
        for (int start = 0; start < length; start += blockSize)
        {
            const auto count = juce::jmin (blockSize, length - start);
            for (int i = 0; i < count; ++i)
            {
                const auto v = level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (start + i) / sampleRate);
                out.setSample (0, start + i, v);
                out.setSample (1, start + i, v);
            }
            if (perBlock)
                perBlock (s, start / sampleRate);
            juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) count);
            cipher.process (block, s);
        }
        return out;
    }

    float rms (const juce::AudioBuffer<float>& b, int channel, double from, double to)
    {
        const auto a = (int) (from * sampleRate), z = juce::jmin (b.getNumSamples(), (int) (to * sampleRate));
        double sum = 0.0;
        for (int i = a; i < z; ++i)
            sum += (double) b.getSample (channel, i) * b.getSample (channel, i);
        return (float) std::sqrt (sum / juce::jmax (1, z - a));
    }

    // How much a sine at `hz` comes out louder (+) or quieter (-) than it went in, in dB.
    float gainAt (const Cipher::Settings& settings, double hz, int channel = 0)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        const auto out = run (cipher, settings, hz, 0.5);
        return dB (rms (out, channel, 0.3, 0.5) / (0.25f / std::sqrt (2.0f)));
    }

    Cipher::Settings oneSlot (Cipher::Type type, float main = 0.5f, float p1 = 0.0f, float p2 = 0.5f, int option = 3)
    {
        Cipher::Settings s;
        s.on = true;
        auto& slot = s.slots[0];
        slot.type = type;
        slot.main = main;
        slot.p1 = p1;
        slot.p2 = p2;
        slot.option = option;
        slot.envAmount = 0.0f;
        return s;
    }

    // The main knob's position for a cutoff in Hz (the knob is 20 Hz to 20 kHz, logarithmic).
    float at (float hz) { return std::log (hz / 20.0f) / std::log (1000.0f); }
}

TEST_CASE ("CIPHER: switched on with every slot empty, the sound passes untouched", "[effects][cipher]")
{
    Cipher::Settings s;
    s.on = true;
    for (const auto hz : { 60.0, 1000.0, 8000.0 })
        CHECK_THAT (gainAt (s, hz), WithinAbs (0.0, 0.05));
}

TEST_CASE ("CIPHER: every low-pass passes the lows and cuts above its cutoff, steeper settings cutting more", "[effects][cipher]")
{
    for (const auto type : { Cipher::Type::ladderLp, Cipher::Type::digitalLp, Cipher::Type::oneOhOneLp, Cipher::Type::model20Lp })
    {
        CAPTURE (Cipher::typeNames[(int) type]);
        const auto low = gainAt (oneSlot (type, at (1000.0f), 0.0f, 0.0f), 100.0);
        CAPTURE (low);
        CHECK (std::abs (low) < 3.5f);
        float last = 0.0f;
        for (int slope = 0; slope < 4; ++slope)
        {
            const auto high = gainAt (oneSlot (type, at (1000.0f), 0.0f, 0.0f, slope), 8000.0);
            CAPTURE (slope, high);
            if (type == Cipher::Type::oneOhOneLp)
                CHECK (high < -40.0f);   // always 24 dB an octave
            else
            {
                CHECK (high < -12.0f);   // at least 6 dB an octave over three octaves
                if (slope > 0)
                    CHECK (high < last - 5.0f);
            }
            last = high;
        }
    }
}

TEST_CASE ("CIPHER: high-pass and band-pass filters keep the right part of the sound", "[effects][cipher]")
{
    for (const auto type : { Cipher::Type::ladderHp, Cipher::Type::digitalHp, Cipher::Type::oneOhOneHp, Cipher::Type::model20Hp })
    {
        CAPTURE (Cipher::typeNames[(int) type]);
        CHECK (gainAt (oneSlot (type, at (1000.0f), 0.0f, 0.0f), 8000.0) > -3.5f);
        CHECK (gainAt (oneSlot (type, at (1000.0f), 0.0f, 0.0f), 100.0) < -20.0f);
    }
    for (const auto type : { Cipher::Type::ladderBp, Cipher::Type::digitalBp, Cipher::Type::oneOhOneBp, Cipher::Type::model20Bp })
    {
        CAPTURE (Cipher::typeNames[(int) type]);
        const auto centre = gainAt (oneSlot (type, at (1000.0f), 0.3f, 0.0f), 1000.0);
        CAPTURE (centre);
        CHECK (centre > -6.0f);
        CHECK (gainAt (oneSlot (type, at (1000.0f), 0.3f, 0.0f), 100.0) < centre - 12.0f);
        CHECK (gainAt (oneSlot (type, at (1000.0f), 0.3f, 0.0f), 10000.0) < centre - 12.0f);
    }
}

TEST_CASE ("CIPHER: RESONANCE peaks at the cutoff; the Ladder's is gentler than the 101's", "[effects][cipher]")
{
    const auto peak = [] (Cipher::Type type, float res) { return gainAt (oneSlot (type, at (1000.0f), res, 0.0f), 1000.0)
                                                                - gainAt (oneSlot (type, at (1000.0f), res, 0.0f), 150.0); };
    const auto ladder = peak (Cipher::Type::ladderLp, 0.9f), oneOhOne = peak (Cipher::Type::oneOhOneLp, 0.9f);
    CAPTURE (ladder, oneOhOne, peak (Cipher::Type::ladderLp, 0.0f));
    CHECK (ladder > peak (Cipher::Type::ladderLp, 0.0f) + 6.0f);
    CHECK (oneOhOne > ladder + 3.0f);
    CHECK (peak (Cipher::Type::digitalLp, 0.9f) > peak (Cipher::Type::digitalLp, 0.0f) + 6.0f);
}

TEST_CASE ("CIPHER: DRIVE on the 101 and Model-20 adds harmonics", "[effects][cipher]")
{
    for (const auto type : { Cipher::Type::oneOhOneLp, Cipher::Type::model20Lp })
    {
        const auto thirdHarmonic = [type] (float drive)
        {
            Cipher cipher;
            cipher.prepare (sampleRate, blockSize);
            const auto out = run (cipher, oneSlot (type, at (12000.0f), 0.0f, drive), 200.0, 0.5, 0.5f);
            // A Goertzel at 600 Hz over the steady part, against the 200 Hz.
            const auto level = [&out] (double hz)
            {
                double re = 0.0, im = 0.0;
                for (int i = (int) (0.2 * sampleRate); i < (int) (0.5 * sampleRate); ++i)
                {
                    const auto phase = juce::MathConstants<double>::twoPi * hz * i / sampleRate;
                    re += out.getSample (0, i) * std::cos (phase);
                    im += out.getSample (0, i) * std::sin (phase);
                }
                return (float) std::sqrt (re * re + im * im);
            };
            return dB (level (600.0) / level (200.0));
        };
        CAPTURE (Cipher::typeNames[(int) type], thirdHarmonic (0.0f), thirdHarmonic (1.0f));
        CHECK (thirdHarmonic (1.0f) > thirdHarmonic (0.0f) + 10.0f);
        CHECK (thirdHarmonic (1.0f) > -30.0f);
    }
}

TEST_CASE ("CIPHER: Notch, Dual Filter and Formant", "[effects][cipher]")
{
    // Notch: a deep cut at the cutoff only.
    CHECK (gainAt (oneSlot (Cipher::Type::notch, at (1000.0f), 0.0f), 1000.0) < -20.0f);
    CHECK (gainAt (oneSlot (Cipher::Type::notch, at (1000.0f), 0.0f), 300.0) > -2.0f);
    CHECK (gainAt (oneSlot (Cipher::Type::notch, at (1000.0f), 0.0f), 3000.0) > -2.0f);
    // WIDTH widens it.
    CHECK (gainAt (oneSlot (Cipher::Type::notch, at (1000.0f), 1.0f), 700.0) < gainAt (oneSlot (Cipher::Type::notch, at (1000.0f), 0.0f), 700.0) - 6.0f);

    // Dual Filter: a band between two resonant edges; BANDWIDTH spreads them (1.5 octaves each side here).
    const auto dual = oneSlot (Cipher::Type::dualFilter, at (1000.0f), 0.5f, 0.5f);
    CHECK (gainAt (dual, 1000.0) > gainAt (dual, 60.0) + 20.0f);
    CHECK (gainAt (dual, 1000.0) > gainAt (dual, 16000.0) + 12.0f);
    CHECK (gainAt (dual, 2800.0) > gainAt (dual, 1000.0));   // the upper edge's resonant peak, about 2.8 kHz

    // Formant: "A" (main at the left) has its first formant near 730 Hz, "I"
    // (the middle) near 270 Hz and its second near 2290 Hz.
    const auto vowelA = oneSlot (Cipher::Type::formant, 0.0f, 0.5f), vowelI = oneSlot (Cipher::Type::formant, 0.5f, 0.5f);
    CHECK (gainAt (vowelA, 730.0) > gainAt (vowelA, 270.0) + 10.0f);
    CHECK (gainAt (vowelI, 270.0) > gainAt (vowelI, 730.0) + 10.0f);
    CHECK (gainAt (vowelI, 2290.0) > gainAt (vowelI, 1300.0) + 6.0f);
    // SHIFT moves every formant: +300 Hz moves "I"'s first up to about 570 Hz.
    const auto shifted = oneSlot (Cipher::Type::formant, 0.5f, 0.5f, 0.8f);
    CHECK (gainAt (shifted, 570.0) > gainAt (shifted, 270.0) + 6.0f);
}

TEST_CASE ("CIPHER: the EQ boosts and cuts by its GAIN", "[effects][cipher]")
{
    const auto plus12 = 0.5f + 12.0f / 36.0f, minus12 = 0.5f - 12.0f / 36.0f;
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::eqBell, at (1000.0f), plus12, 0.5f), 1000.0), WithinAbs (12.0, 0.5));
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::eqBell, at (1000.0f), minus12, 0.5f), 1000.0), WithinAbs (-12.0, 0.5));
    CHECK (std::abs (gainAt (oneSlot (Cipher::Type::eqBell, at (1000.0f), plus12, 0.5f), 100.0)) < 1.0f);
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::eqLowShelf, at (300.0f), plus12, 0.3f), 40.0), WithinAbs (12.0, 0.7));
    CHECK (std::abs (gainAt (oneSlot (Cipher::Type::eqLowShelf, at (300.0f), plus12, 0.3f), 8000.0)) < 0.7f);
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::eqHighShelf, at (2000.0f), minus12, 0.3f), 15000.0), WithinAbs (-12.0, 0.7));
    CHECK (std::abs (gainAt (oneSlot (Cipher::Type::eqHighShelf, at (2000.0f), minus12, 0.3f), 100.0)) < 0.7f);
}

TEST_CASE ("CIPHER: the Gain and Pan utilities", "[effects][cipher]")
{
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::gain, 0.5f), 1000.0), WithinAbs (0.0, 0.05));
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::gain, 0.25f), 1000.0), WithinAbs (-6.02, 0.1));
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::gain, 1.0f), 1000.0), WithinAbs (12.0, 0.1));
    CHECK (gainAt (oneSlot (Cipher::Type::gain, 0.0f), 1000.0) < -90.0f);

    // Pan hard right: the left side empties into the right. BALANCE: only turned down.
    const auto right = oneSlot (Cipher::Type::pan, 1.0f, 0.5f, 0.5f, 0);
    CHECK (gainAt (right, 1000.0, 0) < -90.0f);
    CHECK_THAT (gainAt (right, 1000.0, 1), WithinAbs (6.02, 0.1));   // both sides, now in one
    const auto balance = oneSlot (Cipher::Type::pan, 1.0f, 0.5f, 0.5f, 1);
    CHECK (gainAt (balance, 1000.0, 0) < -90.0f);
    CHECK_THAT (gainAt (balance, 1000.0, 1), WithinAbs (0.0, 0.05));
    // Centred, WIDTH at 100% changes nothing.
    CHECK_THAT (gainAt (oneSlot (Cipher::Type::pan, 0.5f, 0.5f, 0.5f, 2), 1000.0, 0), WithinAbs (0.0, 0.05));
}

TEST_CASE ("CIPHER: the envelope sweeps the main knob, once per LFO cycle, locked to the song while it plays", "[effects][cipher]")
{
    // A ramp from bottom to top over one bar (2 s at 120 BPM) sweeps a
    // low-pass up: a 3 kHz tone starts cut and ends open.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    cipher.setEnvelope (0, Cipher::shape (1));
    auto s = oneSlot (Cipher::Type::digitalLp, 0.5f);
    s.slots[0].envAmount = 1.0f;
    s.slots[0].speed = 3;   // 1 bar
    const auto out = run (cipher, s, 3000.0, 1.9);
    const auto early = dB (rms (out, 0, 0.05, 0.25)), late = dB (rms (out, 0, 1.6, 1.85));
    CAPTURE (early, late);
    CHECK (late > early + 30.0f);

    // Locked to the song: at beat 2 of a 1-bar cycle the envelope is half way.
    Cipher locked;
    locked.prepare (sampleRate, blockSize);
    auto playing = s;
    playing.songBeats = 2.0;
    run (locked, playing, 100.0, (double) blockSize / sampleRate);
    CHECK_THAT (locked.getPhase (0), WithinAbs (0.5, 0.01));

    // ENV MOD STYLE: Up only raises the main knob, Down only lowers it.
    for (const auto style : { Cipher::Style::up, Cipher::Style::down })
    {
        Cipher c;
        c.prepare (sampleRate, blockSize);
        c.setEnvelope (0, Cipher::shape (0));   // flat in the middle
        auto st = s;
        st.slots[0].style = style;
        st.slots[0].envAmount = 0.4f;
        run (c, st, 100.0, 0.05);
        CHECK_THAT (c.getMovedMain (0), WithinAbs (style == Cipher::Style::up ? 0.7 : 0.3, 0.001));
    }
}

TEST_CASE ("CIPHER: GLIDE smooths a stepped envelope; STEREO moves the two sides apart", "[effects][cipher]")
{
    // A square envelope (top for half, then bottom) with GLIDE: just after the
    // drop, the main knob is still on its way down.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    cipher.setEnvelope (0, Cipher::shape (5));
    auto s = oneSlot (Cipher::Type::digitalLp, 0.5f);
    s.slots[0].envAmount = 1.0f;
    s.slots[0].glideMs = 200.0f;
    run (cipher, s, 100.0, 1.05);   // the drop is at 1 s
    const auto moving = cipher.getMovedMain (0);
    CAPTURE (moving);
    CHECK (moving > 0.3f);
    CHECK (moving < 0.95f);

    // STEREO: the left side's cutoff lower, the right's higher.
    auto wide = oneSlot (Cipher::Type::digitalLp, at (2000.0f));
    wide.slots[0].stereo = 1.0f;
    CHECK (gainAt (wide, 3000.0, 1) > gainAt (wide, 3000.0, 0) + 6.0f);
}

TEST_CASE ("CIPHER: a slot's MIX and GAIN; slots in series", "[effects][cipher]")
{
    auto s = oneSlot (Cipher::Type::digitalHp, at (2000.0f));
    s.slots[0].mix = 0.0f;
    CHECK_THAT (gainAt (s, 100.0), WithinAbs (0.0, 0.05));
    s.slots[0].mix = 1.0f;
    s.slots[0].gainDb = 6.0f;
    CHECK_THAT (gainAt (s, 10000.0), WithinAbs (6.0, 0.5));

    // A high-pass then a low-pass make a band.
    auto chain = oneSlot (Cipher::Type::digitalHp, at (500.0f));
    chain.slots[1] = chain.slots[0];
    chain.slots[1].type = Cipher::Type::digitalLp;
    chain.slots[1].main = at (2000.0f);
    CHECK (gainAt (chain, 1000.0) > -3.0f);
    CHECK (gainAt (chain, 100.0) < -20.0f);
    CHECK (gainAt (chain, 10000.0) < -20.0f);
    // A slot switched off is skipped.
    chain.slots[1].on = false;
    CHECK (gainAt (chain, 10000.0) > -3.0f);
}

TEST_CASE ("CIPHER master: the drive, the compressors, CLEAN BASS and the output", "[effects][cipher]")
{
    // DRIVE: a clean sine comes out with harmonics; each type stays near the input's level.
    for (int type = 0; type < Cipher::driveNames.size(); ++type)
    {
        Cipher::Settings s;
        s.on = true;
        s.master.driveOn = true;
        s.master.driveType = type;
        s.master.drive = 0.6f;
        CAPTURE (Cipher::driveNames[type], gainAt (s, 500.0));
        CHECK (std::abs (gainAt (s, 500.0)) < 12.0f);
    }

    // The compressors squeeze a loud and a quiet note closer together.
    for (const auto comp : { Cipher::Comp::squash, Cipher::Comp::maximizer, Cipher::Comp::multiband, Cipher::Comp::overkill })
    {
        Cipher::Settings s;
        s.on = true;
        s.master.compOn = true;
        s.master.comp = comp;
        s.master.compress = 0.7f;
        const auto level = [&s] (float in)
        {
            Cipher cipher;
            cipher.prepare (sampleRate, blockSize);
            const auto out = run (cipher, s, 800.0, 0.6, in);
            return dB (rms (out, 0, 0.4, 0.6));
        };
        const auto spread = level (0.5f) - level (0.05f);
        CAPTURE (Cipher::compNames[(int) comp], spread);
        CHECK (spread < 14.0f);   // 20 dB apart going in
        if (comp == Cipher::Comp::maximizer)
            CHECK (level (2.0f) < dB (std::pow (10.0f, -0.5f / 20.0f) / std::sqrt (2.0f)) + 0.6f);   // nothing past -0.5 dBFS
    }

    // CLEAN BASS keeps the lows as they came in, whatever the slots do.
    auto cut = oneSlot (Cipher::Type::digitalHp, at (2000.0f));
    CHECK (gainAt (cut, 50.0) < -40.0f);
    cut.master.cleanBassOn = true;
    cut.master.cleanBassHz = 200.0f;
    CHECK (std::abs (gainAt (cut, 50.0)) < 1.0f);

    // OUTPUT: GAIN, and MIX back to the dry sound.
    Cipher::Settings out;
    out.on = true;
    out.master.gainDb = 6.0f;
    CHECK_THAT (gainAt (out, 1000.0), WithinAbs (6.0, 0.05));
    auto silent = oneSlot (Cipher::Type::gain, 0.0f);
    silent.master.mix = 0.0f;
    CHECK_THAT (gainAt (silent, 1000.0), WithinAbs (0.0, 0.05));
    silent.master.mix = 0.5f;
    silent.master.mixLaw = 0;   // -6 dB at the centre
    CHECK_THAT (gainAt (silent, 1000.0), WithinAbs (-6.02, 0.1));
    silent.master.mixLaw = 2;   // 0 dB: the dry doesn't dip until past half way
    CHECK_THAT (gainAt (silent, 1000.0), WithinAbs (0.0, 0.1));
}

TEST_CASE ("CIPHER: extreme settings stay finite", "[effects][cipher]")
{
    for (int t = 1; t < Cipher::typeNames.size(); ++t)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        cipher.setEnvelope (0, Cipher::shape (6));
        auto s = oneSlot ((Cipher::Type) t, 0.5f, 1.0f, 1.0f, 3);
        s.slots[0].envAmount = 1.0f;
        s.slots[0].sync = false;
        s.slots[0].rateHz = 40.0f;
        s.master.driveOn = s.master.compOn = true;
        s.master.drive = 1.0f;
        s.master.comp = Cipher::Comp::overkill;
        const auto out = run (cipher, s, 3000.0, 0.5, 1.0f);
        auto finite = true;
        auto peak = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                finite = finite && std::isfinite (out.getSample (c, i));
                peak = juce::jmax (peak, std::abs (out.getSample (c, i)));
            }
        CAPTURE (Cipher::typeNames[t], peak);
        CHECK (finite);
        CHECK (peak < 8.0f);
    }
}

TEST_CASE ("CIPHER envelopes are saved in rigs and projects, and undo covers them", "[effects][cipher][rigs]")
{
    const Cipher::Envelope drawn { { 0.0f, 0.1f, 0.5f }, { 0.3f, 0.9f, 0.0f }, { 1.0f, 0.2f, -0.25f } };
    CHECK (Cipher::toText (Cipher::fromText (Cipher::toText (drawn))) == Cipher::toText (drawn));

    ParagonX1AudioProcessor processor;
    processor.setCipherEnvelope (3, drawn);
    const auto rig = processor.captureRig();
    REQUIRE (rig.cipherEnvelopes.count (3) == 1);
    CHECK (rig.cipherEnvelopes.count (0) == 0);   // flat slots aren't written

    const auto reloaded = Rig::fromXml (*rig.toXml());
    REQUIRE (reloaded.has_value());
    CHECK (reloaded->cipherEnvelopes == rig.cipherEnvelopes);

    // A project keeps them.
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    ParagonX1AudioProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (Cipher::toText (other.getCipherEnvelope (3)) == Cipher::toText (drawn));
    CHECK (Cipher::toText (other.getCipherEnvelope (2)) == Cipher::toText (Cipher::shape (0)));

    // Undo takes a drawn envelope back.
    processor.flushUndoStep();
    processor.setCipherEnvelope (3, Cipher::shape (2));
    processor.flushUndoStep();
    processor.undo();
    CHECK (Cipher::toText (processor.getCipherEnvelope (3)) == Cipher::toText (drawn));
}

#include "ui/CipherPanel.h"

TEST_CASE ("CIPHER's panel: the slot keys move the knobs to that slot; a shape loads into the envelope", "[ui][cipher]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    ParagonX1AudioProcessor processor;
    auto& state = processor.getParameters();
    const auto set = [&state] (const juce::String& id, float value)
    {
        auto* p = state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (ParamIDs::cipherSlot (2, "Main"), 0.8f);
    set (ParamIDs::cipherSlot (2, "Type"), (float) Cipher::Type::formant);

    CipherPanel panel (processor);
    panel.setAreas ({ 0, 0, 380, 240 }, { 400, 0, 1000, 240 });
    std::vector<juce::Slider*> sliders;
    for (auto* child : panel.getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*> (child); slider != nullptr && slider->isVisible())
            sliders.push_back (slider);
    REQUIRE (! sliders.empty());
    CHECK_THAT (sliders[0]->getValue(), WithinAbs (0.5, 0.001));   // slot 1's main knob
    panel.showSlot (1);
    CHECK_THAT (sliders[0]->getValue(), WithinAbs (0.8, 0.001));   // now slot 2's
    // Turning it moves slot 2, not slot 1.
    sliders[0]->setValue (0.3, juce::sendNotificationSync);
    CHECK_THAT (state.getRawParameterValue (ParamIDs::cipherSlot (2, "Main"))->load(), WithinAbs (0.3, 0.001));
    CHECK_THAT (state.getRawParameterValue (ParamIDs::cipherSlot (1, "Main"))->load(), WithinAbs (0.5, 0.001));

    // The SHAPE list loads into the shown slot.
    for (auto* child : panel.getChildren())
        if (auto* box = dynamic_cast<juce::ComboBox*> (child); box != nullptr && box->getNumItems() == Cipher::shapeNames.size())
            box->setSelectedItemIndex (3, juce::sendNotificationSync);
    CHECK (Cipher::toText (processor.getCipherEnvelope (1)) == Cipher::toText (Cipher::shape (3)));
    CHECK (Cipher::toText (processor.getCipherEnvelope (0)) == Cipher::toText (Cipher::shape (0)));

    // MASTER shows the master's knobs instead.
    panel.showSlot (CipherPanel::masterPage);
    auto masterKnobs = 0;
    for (auto* child : panel.getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*> (child); slider != nullptr && slider->isVisible())
            ++masterKnobs;
    CHECK (masterKnobs == 6);
}

TEST_CASE ("CIPHER: CPU with all ten slots busy (prints the share of a core)", "[.cpu]")
{
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    Cipher::Settings s;
    s.on = true;
    for (int i = 0; i < Cipher::numSlots; ++i)
    {
        cipher.setEnvelope (i, Cipher::shape (3));
        // The heaviest effects: reverb, vocoder, phaser, pitch, stretch and the rest.
        const Cipher::Type heavy[] = { Cipher::Type::reverb, Cipher::Type::vocoder, Cipher::Type::phaser, Cipher::Type::pitchShift,
                                       Cipher::Type::stretch, Cipher::Type::delay, Cipher::Type::comb, Cipher::Type::freqShift,
                                       Cipher::Type::oneOhOneLp, Cipher::Type::distort };
        s.slots[(size_t) i].type = heavy[i];
        s.slots[(size_t) i].p1 = 0.7f;
    }
    s.master.driveOn = s.master.compOn = true;
    s.master.comp = Cipher::Comp::multiband;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    run (cipher, s, 220.0, 10.0);
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    std::printf ("CIPHER, ten busy slots: %.1f%% of a core\n", seconds / 10.0 * 100.0);
}

//==============================================================================
// Stage 2: the time effects.
namespace
{
    // The level of `hz` in a stretch of the output (a Goertzel), as a linear amplitude.
    float toneLevel (const juce::AudioBuffer<float>& b, int channel, double hz, double from, double to)
    {
        double re = 0.0, im = 0.0;
        const auto a = (int) (from * sampleRate), z = juce::jmin (b.getNumSamples(), (int) (to * sampleRate));
        for (int i = a; i < z; ++i)
        {
            const auto phase = juce::MathConstants<double>::twoPi * hz * i / sampleRate;
            re += b.getSample (channel, i) * std::cos (phase);
            im += b.getSample (channel, i) * std::sin (phase);
        }
        return (float) (2.0 * std::sqrt (re * re + im * im) / juce::jmax (1, z - a));
    }

    // The strongest level within 7% of `hz` (granular effects spread a note into nearby lines).
    float bandLevel (const juce::AudioBuffer<float>& b, int channel, double hz, double from, double to)
    {
        float best = 0.0f;
        for (double f = hz * 0.93; f <= hz * 1.07; f += 2.0)
            best = juce::jmax (best, toneLevel (b, channel, f, from, to));
        return best;
    }

    juce::AudioBuffer<float> runTone (const Cipher::Settings& s, double hz, double seconds, float level = 0.25f)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        return run (cipher, s, hz, seconds, level);
    }

    // A click at the start, then silence: for echoes and tails.
    juce::AudioBuffer<float> runClick (const Cipher::Settings& s, double seconds)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        const auto length = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> out (2, length);
        out.clear();
        for (int i = 0; i < 48; ++i)
            out.setSample (0, i, 0.5f), out.setSample (1, i, 0.5f);
        for (int start = 0; start < length; start += blockSize)
        {
            juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) juce::jmin (blockSize, length - start));
            cipher.process (block, s);
        }
        return out;
    }
}

TEST_CASE ("CIPHER stage 2: Comb, Phaser, Freq Shift, Ring Mod and FM change the spectrum as the manual says", "[effects][cipher]")
{
    // Comb at 500 Hz with resonance: 500 and 1000 Hz ring louder than 750.
    const auto comb = oneSlot (Cipher::Type::comb, std::log (500.0f / 20.0f) / std::log (250.0f), 0.8f, 1.0f);
    CHECK (gainAt (comb, 1000.0) > gainAt (comb, 750.0) + 10.0f);
    CHECK (gainAt (comb, 500.0) > gainAt (comb, 750.0) + 10.0f);

    // Phaser: a deep notch somewhere between 200 Hz and 5 kHz; DUO has fewer notches than TURBO.
    const auto notches = [] (int mode)
    {
        auto s = oneSlot (Cipher::Type::phaser, at (1000.0f), 0.0f, 0.5f, Cipher::optionFor (Cipher::Type::phaser, mode));
        int count = 0;
        float previous = 0.0f;
        bool falling = false;
        for (double hz = 100.0; hz < 10000.0; hz *= 1.08)
        {
            const auto g = gainAt (s, hz);
            if (falling && g > previous && previous < -12.0f)
                ++count;
            falling = g < previous;
            previous = g;
        }
        return count;
    };
    const auto turbo = notches (0), duo = notches (1);
    CAPTURE (turbo, duo);
    CHECK (duo >= 1);
    CHECK (turbo > duo);

    // Freq Shift +100 Hz: 1 kHz comes out at 1.1 kHz, and not at 900 Hz.
    const auto shiftMain = 0.5f + std::sqrt (100.0f / 2000.0f) * 0.5f;
    CHECK_THAT (Cipher::shiftHz (shiftMain), WithinAbs (100.0, 0.5));
    const auto shifted = runTone (oneSlot (Cipher::Type::freqShift, shiftMain), 1000.0, 0.5);
    CHECK (dB (toneLevel (shifted, 0, 1100.0, 0.2, 0.5) / toneLevel (shifted, 0, 900.0, 0.2, 0.5)) > 40.0f);
    CHECK (dB (toneLevel (shifted, 0, 1100.0, 0.2, 0.5) / toneLevel (shifted, 0, 1000.0, 0.2, 0.5)) > 30.0f);

    // Ring Mod at 300 Hz, full AMOUNT: 1 kHz becomes 700 and 1300 Hz.
    const auto ring = runTone (oneSlot (Cipher::Type::ringMod, at (300.0f), 1.0f, 0.0f), 1000.0, 0.5);
    CHECK (toneLevel (ring, 0, 700.0, 0.2, 0.5) > 4.0f * toneLevel (ring, 0, 1000.0, 0.2, 0.5));
    CHECK (toneLevel (ring, 0, 1300.0, 0.2, 0.5) > 4.0f * toneLevel (ring, 0, 1000.0, 0.2, 0.5));

    // FM: sidebands at the note plus and minus the modulator.
    const auto fmMain = std::log (200.0f / 0.5f) / std::log (10000.0f);   // a 200 Hz modulator
    const auto fm = runTone (oneSlot (Cipher::Type::fm, fmMain, 0.5f, 0.6f), 1000.0, 0.5);
    CHECK (toneLevel (fm, 0, 1200.0, 0.2, 0.5) > 0.1f * toneLevel (fm, 0, 1000.0, 0.2, 0.5));
    CHECK (toneLevel (runTone (oneSlot (Cipher::Type::fm, fmMain, 0.5f, 0.0f), 1000.0, 0.5), 0, 1200.0, 0.2, 0.5) < 0.001f);
}

TEST_CASE ("CIPHER stage 2: Pitch Shift, Distort and Decimate", "[effects][cipher]")
{
    // Pitch Shift +12: a 220 Hz note comes out mostly at 440.
    const auto up = runTone (oneSlot (Cipher::Type::pitchShift, 0.5f + 12.0f / 48.0f, 0.5f, 0.0f), 220.0, 0.8);
    CHECK (bandLevel (up, 0, 440.0, 0.3, 0.8) > 3.0f * bandLevel (up, 0, 220.0, 0.3, 0.8));
    // -12: at 110.
    const auto down = runTone (oneSlot (Cipher::Type::pitchShift, 0.5f - 12.0f / 48.0f, 0.5f, 0.0f), 220.0, 0.8);
    CHECK (bandLevel (down, 0, 110.0, 0.3, 0.8) > 3.0f * bandLevel (down, 0, 220.0, 0.3, 0.8));

    // Distort: harmonics, and HP takes the lows away before the drive.
    for (int mode = 0; mode < Cipher::driveModeNames.size(); ++mode)
    {
        auto s = oneSlot (Cipher::Type::distort, 0.6f, 0.0f, 1.0f, Cipher::optionFor (Cipher::Type::distort, mode));
        const auto out = runTone (s, 300.0, 0.5, 0.3f);
        const auto harmonics = toneLevel (out, 0, 600.0, 0.2, 0.5) + toneLevel (out, 0, 900.0, 0.2, 0.5);
        CAPTURE (Cipher::driveModeNames[mode], dB (harmonics / toneLevel (out, 0, 300.0, 0.2, 0.5)));
        CHECK (harmonics > 0.03f * toneLevel (out, 0, 300.0, 0.2, 0.5));
    }
    CHECK (gainAt (oneSlot (Cipher::Type::distort, 0.3f, 1.0f, 1.0f), 60.0) < gainAt (oneSlot (Cipher::Type::distort, 0.3f, 0.0f, 1.0f), 60.0) - 15.0f);

    // Decimate to 3 kHz: a 1 kHz tone gets a 2 kHz alias with DIRT up; far less with it down.
    const auto rateMain = std::log (3000.0f / 100.0f) / std::log (480.0f);
    const auto dirty = runTone (oneSlot (Cipher::Type::decimate, rateMain, 1.0f), 1000.0, 0.5);
    const auto clean = runTone (oneSlot (Cipher::Type::decimate, rateMain, 0.0f), 1000.0, 0.5);
    CHECK (toneLevel (dirty, 0, 2000.0, 0.2, 0.5) > 0.2f * toneLevel (dirty, 0, 1000.0, 0.2, 0.5));
    CHECK (toneLevel (clean, 0, 2000.0, 0.2, 0.5) < 0.5f * toneLevel (dirty, 0, 2000.0, 0.2, 0.5));
}

TEST_CASE ("CIPHER stage 2: Delay, Reflect and Reverb", "[effects][cipher]")
{
    // Delay synced to 1/8 at 120 BPM: an echo 250 ms after the click; FEEDBACK repeats it.
    const auto eighth = 1.0f - (float) Cipher::speedNames.indexOf ("1/8") / 13.0f + 3.0f / 13.0f;   // 16 - step = index of 1/8
    auto delay = oneSlot (Cipher::Type::delay, eighth, 0.5f, 0.5f);
    REQUIRE (Cipher::mainText (Cipher::Type::delay, eighth, delay.slots[0].option) == "1/8");
    const auto echoes = runClick (delay, 1.0);
    CHECK (rms (echoes, 0, 0.245, 0.255) > 0.05f);
    CHECK (rms (echoes, 0, 0.495, 0.505) > 0.02f);   // the repeat
    CHECK (rms (echoes, 0, 0.1, 0.2) < 0.001f);      // nothing between

    // PING-PONG: the first echo on the left, the next on the right.
    delay.slots[0].stereo = 1.0f;
    REQUIRE (Cipher::stereoText (Cipher::Type::delay, 1.0f) == "PING-PONG");
    const auto pingPong = runClick (delay, 1.0);
    CHECK (rms (pingPong, 0, 0.245, 0.255) > 4.0f * rms (pingPong, 1, 0.245, 0.255) + 1.0e-6f);
    CHECK (rms (pingPong, 1, 0.495, 0.505) > 4.0f * rms (pingPong, 0, 0.495, 0.505) + 1.0e-6f);

    // FREE: the time in milliseconds (100 ms here).
    auto free = oneSlot (Cipher::Type::delay, std::log (100.0f) / std::log (2000.0f), 0.0f, 0.5f, Cipher::optionFor (Cipher::Type::delay, 1));
    const auto freeEcho = runClick (free, 0.5);
    CHECK (rms (freeEcho, 0, 0.098, 0.106) > 0.05f);

    // Reflect: early echoes within the first tenth of a second, then quiet.
    const auto reflections = runClick (oneSlot (Cipher::Type::reflect, 0.5f, 0.3f, 0.0f), 1.0);
    CHECK (rms (reflections, 0, 0.005, 0.12) > 0.005f);
    CHECK (rms (reflections, 0, 0.5, 1.0) < 0.0005f);

    // Reverb: a longer TIME rings longer.
    const auto tail = [] (float main, int mode)
    {
        const auto out = runClick (oneSlot (Cipher::Type::reverb, main, 0.7f, 0.0f, Cipher::optionFor (Cipher::Type::reverb, mode)), 3.0);
        return dB (rms (out, 0, 1.5, 2.5));
    };
    for (int mode = 0; mode < 3; ++mode)
    {
        CAPTURE (Cipher::reverbModeNames[mode], tail (0.2f, mode), tail (0.7f, mode));
        CHECK (tail (0.7f, mode) > tail (0.2f, mode) + 20.0f);
    }
}

TEST_CASE ("CIPHER stage 2: Vocoder, Loop, Stretch, Varispeed, Slow and Reverse", "[effects][cipher]")
{
    // Vocoder: silence in, silence out; a voice in, the carrier's pitch out (110 Hz here).
    const auto vocMain = std::log (110.0f / 30.0f) / std::log (33.0f);
    CHECK (rms (runTone (oneSlot (Cipher::Type::vocoder, vocMain), 300.0, 0.5, 0.0f), 0, 0.1, 0.5) == 0.0f);
    const auto voiced = runTone (oneSlot (Cipher::Type::vocoder, vocMain, 0.5f, 0.2f), 800.0, 0.6);
    CHECK (toneLevel (voiced, 0, 770.0, 0.3, 0.6) > 3.0f * toneLevel (voiced, 0, 800.0, 0.3, 0.6));   // the 7th harmonic of 110, near the voice

    // Loop (FREE, 10 Hz): the output repeats every 100 ms while the input moves on (a rising sweep in).
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        auto s = oneSlot (Cipher::Type::loop, std::log (10.0f) / std::log (1000.0f), 0.0f, 0.0f);
        s.songBeats = -1.0;
        const auto length = (int) (0.5 * sampleRate);
        juce::AudioBuffer<float> out (2, length);
        double phase = 0.0;
        for (int i = 0; i < length; ++i)
        {
            phase += juce::MathConstants<double>::twoPi * (200.0 + 2000.0 * i / length) / sampleRate;
            out.setSample (0, i, 0.3f * (float) std::sin (phase));
            out.setSample (1, i, 0.3f * (float) std::sin (phase));
        }
        for (int start = 0; start < length; start += blockSize)
        {
            juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) juce::jmin (blockSize, length - start));
            cipher.process (block, s);
        }
        const auto period = (int) (0.1 * sampleRate);
        double difference = 0.0;
        for (int i = (int) (0.25 * sampleRate); i < (int) (0.25 * sampleRate) + 2000; ++i)
            difference += std::abs (out.getSample (0, i) - out.getSample (0, i + period));
        CHECK (difference / 2000.0 < 0.01);
    }

    // Stretch keeps the pitch while slowing: a 440 Hz note stays at 440.
    const auto stretched = runTone (oneSlot (Cipher::Type::stretch, 0.3f, 0.0f, 0.5f), 440.0, 1.0);
    CHECK (bandLevel (stretched, 0, 440.0, 0.3, 1.0) > 4.0f * bandLevel (stretched, 0, 220.0, 0.3, 1.0));
    CHECK (rms (stretched, 0, 0.3, 1.0) > 0.25f * 0.4f);

    // Varispeed at half speed: a 440 Hz note comes out at 220 (tape-style); Slow at 1/2 does the same.
    const auto half = (0.5f - 0.05f) / 0.95f;
    const auto vari = runTone (oneSlot (Cipher::Type::varispeed, half, 0.0f), 440.0, 1.0);
    CHECK (toneLevel (vari, 0, 220.0, 0.3, 1.0) > 4.0f * toneLevel (vari, 0, 440.0, 0.3, 1.0));
    const auto slowed = runTone (oneSlot (Cipher::Type::slow, 0.25f, 0.0f), 440.0, 1.0);   // the second step: 1/2
    REQUIRE (Cipher::mainText (Cipher::Type::slow, 0.25f) == "1/2");
    CHECK (toneLevel (slowed, 0, 220.0, 0.3, 1.0) > 4.0f * toneLevel (slowed, 0, 440.0, 0.3, 1.0));
    // Reverse at full speed keeps the pitch (a steady note backwards is the same
    // note). A new part every 1/8, so there's sound to play backwards.
    const auto backwards = runTone (oneSlot (Cipher::Type::reverse, 0.0f, 0.5f), 440.0, 1.0);
    CHECK (toneLevel (backwards, 0, 440.0, 0.3, 1.0) > 0.5f * 0.25f);
}

TEST_CASE ("CIPHER stage 2: Gate and Compress", "[effects][cipher]")
{
    // Gate at -30 dB: a -20 dB note passes, a -40 dB note is muted.
    const auto gate = oneSlot (Cipher::Type::gate, 50.0f / 80.0f, 0.1f, 0.1f);
    CHECK_THAT (gainAt (gate, 1000.0), WithinAbs (0.0, 0.1));   // 0.25 peak is -12 dB: over
    CHECK (rms (runTone (gate, 1000.0, 0.5, 0.01f), 0, 0.3, 0.5) < 1.0e-5f);   // -40 dB: under

    // Compress squeezes a loud and a quiet note together; MIX 0 leaves it alone.
    auto comp = oneSlot (Cipher::Type::compress, 0.7f);
    const auto level = [&comp] (float in) { return dB (rms (runTone (comp, 800.0, 0.6, in), 0, 0.4, 0.6)); };
    CHECK (level (0.5f) - level (0.05f) < 14.0f);
    comp.slots[0].mix = 0.0f;
    CHECK_THAT (level (0.5f) - level (0.05f), WithinAbs (20.0, 0.1));
}

//==============================================================================
// Stage 3: the sequencer, MIDI, retriggering, the second modulator, pitch snap.
TEST_CASE ("CIPHER stage 3: the sequencer plays a slot only on its lit steps, in time with the song", "[effects][cipher]")
{
    // A Gain slot turned all the way down (silent) plays on steps 1-8 of a
    // 16-step loop at 1/16: at 120 BPM each step is 125 ms, so the first
    // second is silent and the next passes untouched.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    cipher.setSequenceRow (0, 0x000000ffu);
    auto s = oneSlot (Cipher::Type::gain, 0.0f);
    s.master.seqOn = true;
    const auto out = run (cipher, s, 1000.0, 2.0, 0.25f, [] (Cipher::Settings& settings, double seconds) { settings.songBeats = seconds * 2.0; });
    CHECK (rms (out, 0, 0.05, 0.95) < 1.0e-4f);
    CHECK_THAT (dB (rms (out, 0, 1.05, 1.95) / (0.25f / std::sqrt (2.0f))), WithinAbs (0.0, 0.1));
    CHECK (cipher.getStep() >= 8);

    // The loop region: steps 9-12 only (all lit), so the slot always plays.
    Cipher looped;
    looped.prepare (sampleRate, blockSize);
    looped.setSequenceRow (0, 0x00000f00u);
    s.master.seqStart = 8;
    s.master.seqEnd = 11;
    const auto loopOut = run (looped, s, 1000.0, 1.0, 0.25f, [] (Cipher::Settings& settings, double seconds) { settings.songBeats = seconds * 2.0; });
    CHECK (rms (loopOut, 0, 0.05, 0.95) < 1.0e-4f);

    // Text round trip.
    std::array<juce::uint32, Cipher::numSlots> rows {};
    rows[0] = 0xdeadbeefu;
    rows[9] = 1u;
    CHECK (Cipher::sequenceFromText (Cipher::sequenceToText (rows)) == rows);
}

TEST_CASE ("CIPHER stage 3: with MIDI on, a slot plays while its note is held (C for slot 1)", "[effects][cipher]")
{
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    auto s = oneSlot (Cipher::Type::gain, 0.0f);   // silent while it plays
    s.master.midiOn = true;
    // No note: the slot is off, the sound passes.
    auto out = run (cipher, s, 1000.0, 0.2);
    CHECK (rms (out, 0, 0.1, 0.2) > 0.1f);
    // Hold a C (any octave): it plays (silence).
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
    cipher.handleMidi (on);
    out = run (cipher, s, 1000.0, 0.2);
    CHECK (rms (out, 0, 0.1, 0.2) < 1.0e-4f);
    // A D doesn't count for slot 1; letting go of the C ends it.
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
    off.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 0);
    cipher.handleMidi (off);
    out = run (cipher, s, 1000.0, 0.2);
    CHECK (rms (out, 0, 0.1, 0.2) > 0.1f);
}

TEST_CASE ("CIPHER stage 3: retriggering restarts the envelope; LOOP off plays it once; a loop region keeps it inside", "[effects][cipher]")
{
    auto s = oneSlot (Cipher::Type::digitalLp, 0.5f);
    s.slots[0].retrigAudio = true;
    s.slots[0].envLoop = false;
    s.slots[0].startPos = 0.25f;
    s.slots[0].speed = 8;   // 1/4: half a second a pass at 120 BPM

    // Silence: the envelope waits at START.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    run (cipher, s, 1000.0, 0.3, 0.0f);
    CHECK_THAT (cipher.getPhase (0), WithinAbs (0.25, 0.001));
    // A note over the trigger level (-24 dB): it starts from START...
    run (cipher, s, 1000.0, 0.1, 0.25f);
    CHECK (cipher.getPhase (0) > 0.27f);
    CHECK (cipher.getPhase (0) < 0.5f);
    // ...and with LOOP off stops at the end, waiting for the next trigger.
    run (cipher, s, 1000.0, 1.0, 0.25f);
    CHECK_THAT (cipher.getPhase (0), WithinAbs (1.0, 0.001));

    // LOOP on with a loop region: round 0.5-0.75 for good.
    s.slots[0].envLoop = true;
    s.slots[0].loopStart = 0.5f;
    s.slots[0].loopEnd = 0.75f;
    Cipher looped;
    looped.prepare (sampleRate, blockSize);
    float lowest = 1.0f, highest = 0.0f;
    run (looped, s, 1000.0, 0.4, 0.25f);   // through the start and into the region
    for (int i = 0; i < 40; ++i)
    {
        run (looped, s, 1000.0, (double) blockSize / sampleRate, 0.25f);
        lowest = juce::jmin (lowest, looped.getPhase (0));
        highest = juce::jmax (highest, looped.getPhase (0));
    }
    CAPTURE (lowest, highest);
    CHECK (lowest >= 0.49f);
    CHECK (highest <= 0.76f);

    // MIDI | SEQ: a note restarts it.
    auto midi = oneSlot (Cipher::Type::digitalLp, 0.5f);
    midi.slots[0].retrigMidi = true;
    midi.slots[0].speed = 8;
    Cipher byNote;
    byNote.prepare (sampleRate, blockSize);
    run (byNote, midi, 1000.0, 0.3);
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    byNote.handleMidi (note);
    run (byNote, midi, 1000.0, (double) blockSize / sampleRate);
    CHECK (byNote.getPhase (0) < 0.03f);

    // RANDOM START: triggers land in different places.
    auto random = midi;
    random.slots[0].randomStart = true;
    std::set<int> places;
    Cipher randomCipher;
    randomCipher.prepare (sampleRate, blockSize);
    for (int i = 0; i < 6; ++i)
    {
        randomCipher.handleMidi (note);
        run (randomCipher, random, 1000.0, (double) blockSize / sampleRate);
        places.insert (juce::roundToInt (randomCipher.getPhase (0) * 20.0f));
    }
    CHECK (places.size() >= 3);
}

TEST_CASE ("CIPHER stage 3: the audio follower and ENVELOPE 2 as the second modulator", "[effects][cipher]")
{
    // The follower reads -12 dB as 0.8 (GATE -60 dB to full scale).
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    auto s = oneSlot (Cipher::Type::digitalLp, 0.3f);
    s.slots[0].modSource = Cipher::ModSource::audio;
    s.slots[0].modDest = Cipher::ModDest::main;
    s.slots[0].modAmount = 0.5f;
    run (cipher, s, 1000.0, 0.5, 0.25f);
    CHECK_THAT (cipher.getFollower(), WithinAbs (0.79, 0.03));   // -12 dB peak, read through the 10 ms attack
    CHECK_THAT (cipher.getMovedMain (0), WithinAbs (0.3 + 0.5 * 0.79, 0.03));
    // Quiet: it falls back (DECAY), and the main knob with it.
    run (cipher, s, 1000.0, 2.0, 0.0f);
    CHECK (cipher.getFollower() < 0.01f);
    CHECK_THAT (cipher.getMovedMain (0), WithinAbs (0.3, 0.01));

    // ENVELOPE 2 (index 10 for slot 1) on GAIN: its top at 100% adds 24 dB.
    Cipher second;
    second.prepare (sampleRate, blockSize);
    second.setEnvelope (Cipher::numSlots, { { 0.0f, 1.0f }, { 1.0f, 1.0f } });
    auto g = oneSlot (Cipher::Type::gain, 0.5f);
    g.slots[0].modSource = Cipher::ModSource::envelope2;
    g.slots[0].modDest = Cipher::ModDest::gain;
    g.slots[0].modAmount = -0.25f;   // -6 dB
    const auto out = run (second, g, 1000.0, 0.5);
    CHECK_THAT (dB (rms (out, 0, 0.2, 0.5) / (0.25f / std::sqrt (2.0f))), WithinAbs (-6.0, 0.1));
}

TEST_CASE ("CIPHER stage 3: PITCH SNAP holds the main knob on the scale's notes", "[effects][cipher]")
{
    // Chromatic: a cutoff a little off A4 snaps to 440 Hz.
    const auto nearA = at (440.0f * std::pow (2.0f, 0.3f / 12.0f));
    CHECK_THAT (Cipher::cutoffHz (Cipher::snapMain (Cipher::Type::digitalLp, nearA, 0, 0)), WithinAbs (440.0, 0.5));
    // C major: C#4 (277 Hz) isn't in it; it goes to C4 or D4.
    const auto snapped = Cipher::cutoffHz (Cipher::snapMain (Cipher::Type::digitalLp, at (277.18f), 3, 0));
    CAPTURE (snapped);
    CHECK ((std::abs (snapped - 261.63f) < 0.5f || std::abs (snapped - 293.66f) < 0.5f));
    // Pitch Shift: +1.4 st in A minor pentatonic (steps 0 3 5 7 10) goes to 0 or +3.
    const auto shift = Cipher::semitones (Cipher::snapMain (Cipher::Type::pitchShift, 0.5f + 1.4f / 48.0f, 4, 9));
    CHECK ((shift == 0.0f || shift == 3.0f));
    CHECK (Cipher::noteName (440.0f) == "A4");

    // In the engine: SNAP with the envelope sweeping lands only on notes.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    cipher.setEnvelope (0, Cipher::shape (1));
    auto s = oneSlot (Cipher::Type::digitalLp, 0.5f);
    s.slots[0].style = Cipher::Style::snap;
    s.slots[0].envAmount = 1.0f;
    for (int i = 0; i < 20; ++i)
    {
        run (cipher, s, 1000.0, 0.09);
        const auto hz = Cipher::cutoffHz (cipher.getMovedMain (0));
        const auto note = 69.0 + 12.0 * std::log2 (hz / 440.0);
        CHECK (std::abs (note - std::round (note)) < 0.01);
    }
}

TEST_CASE ("CIPHER stage 3: the sequencer and ENVELOPE 2 are saved in rigs and projects", "[effects][cipher][rigs]")
{
    ParagonX1AudioProcessor processor;
    processor.setCipherSequenceRow (2, 0x0f0f0f0fu);
    processor.setCipherEnvelope (Cipher::numSlots + 4, Cipher::shape (3));   // slot 5's ENVELOPE 2
    const auto rig = processor.captureRig();
    CHECK (rig.cipherSequence.isNotEmpty());
    REQUIRE (rig.cipherEnvelopes.count (14) == 1);
    const auto reloaded = Rig::fromXml (*rig.toXml());
    REQUIRE (reloaded.has_value());
    CHECK (reloaded->cipherSequence == rig.cipherSequence);
    CHECK (reloaded->cipherEnvelopes == rig.cipherEnvelopes);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    ParagonX1AudioProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (other.getCipherSequenceRow (2) == 0x0f0f0f0fu);
    CHECK (Cipher::toText (other.getCipherEnvelope (14)) == Cipher::toText (Cipher::shape (3)));

    // A rig without them clears them.
    Rig plain;
    processor.loadRig ({ plain, {}, false });
    CHECK (processor.getCipherSequenceRow (2) == 0u);
}

//==============================================================================
// Stage 4: the spectral effects.
namespace
{
    Cipher::Settings fftSlot (Cipher::Type type, float main, float p1 = 0.5f, float p2 = 0.5f, int sizeIndex = 2)
    {
        return oneSlot (type, main, p1, p2, Cipher::optionFor (type, sizeIndex));
    }

    // Two sines at once (for the effects that pick between loud and quiet frequencies).
    juce::AudioBuffer<float> runTwo (const Cipher::Settings& s, double hzA, float levelA, double hzB, float levelB, double seconds)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        const auto length = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> out (2, length);
        for (int i = 0; i < length; ++i)
        {
            const auto v = levelA * (float) std::sin (juce::MathConstants<double>::twoPi * hzA * i / sampleRate)
                         + levelB * (float) std::sin (juce::MathConstants<double>::twoPi * hzB * i / sampleRate);
            out.setSample (0, i, v);
            out.setSample (1, i, v);
        }
        for (int start = 0; start < length; start += blockSize)
        {
            juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) juce::jmin (blockSize, length - start));
            cipher.process (block, s);
        }
        return out;
    }
}

TEST_CASE ("CIPHER stage 4: a spectral slot delays the sound by its FFT SIZE, reports it, and passes a full band at unity", "[effects][cipher]")
{
    // FFT Bandpass, 8 octaves wide around 600 Hz: a 600 Hz tone comes out at its level.
    CHECK_THAT (gainAt (fftSlot (Cipher::Type::fftBandpass, at (600.0f), 1.0f), 600.0), WithinAbs (0.0, 0.3));
    // A click comes out exactly one frame (2048 samples) later, and the engine reports it.
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    const auto s = fftSlot (Cipher::Type::fftBandpass, at (600.0f), 1.0f);
    juce::AudioBuffer<float> out (2, 8192);
    out.clear();
    out.setSample (0, 3000, 1.0f);   // after CIPHER's 20 ms fade in from off
    out.setSample (1, 3000, 1.0f);
    for (int start = 0; start < 8192; start += blockSize)
    {
        juce::dsp::AudioBlock<float> block (out.getArrayOfWritePointers(), 2, (size_t) start, (size_t) blockSize);
        cipher.process (block, s);
    }
    int peakAt = 0;
    for (int i = 0; i < 8192; ++i)
        if (std::abs (out.getSample (0, i)) > std::abs (out.getSample (0, peakAt)))
            peakAt = i;
    CHECK (peakAt == 3000 + 2048);
    CHECK (cipher.getLatencySamples() == 2048);
    // Two spectral slots add up; the smallest size is 512.
    auto two = s;
    two.slots[1] = s.slots[0];
    two.slots[1].option = Cipher::optionFor (Cipher::Type::fftBandpass, 0);
    Cipher both;
    both.prepare (sampleRate, blockSize);
    run (both, two, 600.0, 0.05);
    CHECK (both.getLatencySamples() == 2048 + 512);

    // MIX at half way: the dry sound is delayed to match, so nothing cancels (a 0 dB law sums both at full level).
    auto half = s;
    half.slots[0].mix = 0.5f;
    CHECK_THAT (gainAt (half, 600.0), WithinAbs (0.0, 0.3));
    auto master = s;
    master.master.mix = 0.5f;
    master.master.mixLaw = 0;   // -6 dB law: wet and dry at half, in time, add back to the whole
    CHECK_THAT (gainAt (master, 600.0), WithinAbs (0.0, 0.3));

    // The plugin tells the host: the amp's latency plus CIPHER's.
    ParagonX1AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    auto& state = processor.getParameters();
    const auto set = [&state] (const juce::String& id, float value)
    {
        auto* p = state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (ParamIDs::cipherOn, 1.0f);
    set (ParamIDs::cipherSlot (1, "Type"), (float) Cipher::Type::fftSmear);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock (buffer, midi);
    CHECK (processor.getCipher().getLatencySamples() == 2048);
    for (int i = 0; i < 20 && processor.getLatencySamples() != 2048; ++i)
    {
        juce::Thread::sleep (30);
        juce::Timer::callPendingTimersSynchronously();   // the processor's timer reports it
    }
    CHECK (processor.getLatencySamples() == 2048);
}

TEST_CASE ("CIPHER stage 4: Shift, Rotate, Mirror, Twist and Bandpass move or pick frequencies", "[effects][cipher]")
{
    // Shift +200 Hz: 1 kHz comes out at 1.2 kHz.
    const auto translate = 0.5f + std::sqrt (200.0f / 2000.0f) * 0.5f;
    auto out = runTone (fftSlot (Cipher::Type::fftShift, translate), 1000.0, 1.0);
    CHECK (bandLevel (out, 0, 1200.0, 0.4, 1.0) > 8.0f * bandLevel (out, 0, 1000.0, 0.4, 1.0));
    // SCALE x2: 500 Hz to 1 kHz.
    out = runTone (fftSlot (Cipher::Type::fftShift, 0.5f, 0.75f), 500.0, 1.0);
    CHECK (bandLevel (out, 0, 1000.0, 0.4, 1.0) > 8.0f * bandLevel (out, 0, 500.0, 0.4, 1.0));

    // Mirror around 1 kHz (2 octaves): 800 Hz comes out at 1.25 kHz.
    out = runTone (fftSlot (Cipher::Type::fftMirror, at (1000.0f), (2.0f - 0.1f) / 5.9f), 800.0, 1.0);
    CHECK (bandLevel (out, 0, 1250.0, 0.4, 1.0) > 8.0f * bandLevel (out, 0, 800.0, 0.4, 1.0));

    // Rotate: a tone moves up by the shift (a quarter of the spectrum, 6 kHz here).
    out = runTone (fftSlot (Cipher::Type::fftRotate, 0.75f), 1000.0, 1.0);
    CHECK (bandLevel (out, 0, 7000.0, 0.4, 1.0) > 8.0f * bandLevel (out, 0, 1000.0, 0.4, 1.0));

    // Twist half way round a 2-octave band around 1 kHz: 600 Hz (inside it) moves; 5 kHz (outside) doesn't.
    out = runTone (fftSlot (Cipher::Type::fftTwist, at (1000.0f), 0.5f, (2.0f - 0.1f) / 5.9f), 600.0, 1.0);
    CHECK (bandLevel (out, 0, 600.0, 0.4, 1.0) < 0.2f * 0.25f);
    CHECK_THAT (gainAt (fftSlot (Cipher::Type::fftTwist, at (1000.0f), 0.5f, (2.0f - 0.1f) / 5.9f), 5000.0), WithinAbs (0.0, 0.5));

    // Bandpass, 1 octave around 1 kHz: brick-wall steep.
    const auto narrow = fftSlot (Cipher::Type::fftBandpass, at (1000.0f), (1.0f - 0.1f) / 7.9f);
    CHECK_THAT (gainAt (narrow, 1000.0), WithinAbs (0.0, 0.5));
    CHECK (gainAt (narrow, 300.0) < -40.0f);
    CHECK (gainAt (narrow, 3000.0) < -40.0f);
}

TEST_CASE ("CIPHER stage 4: Smear, Reverb, Scatter and Cascade keep sound going after it stops", "[effects][cipher]")
{
    // A note for half a second, then silence: these ring on; Bandpass stops.
    const auto tail = [] (Cipher::Type type, float main, float p1 = 0.5f, float p2 = 0.5f)
    {
        Cipher cipher;
        cipher.prepare (sampleRate, blockSize);
        const auto s = fftSlot (type, main, p1, p2);
        const auto note = run (cipher, s, 700.0, 0.5);
        const auto after = run (cipher, s, 700.0, 1.0, 0.0f);
        return dB (rms (after, 0, 0.3, 0.6) / juce::jmax (1.0e-9f, rms (note, 0, 0.2, 0.5)));
    };
    CHECK (tail (Cipher::Type::fftBandpass, at (700.0f), 1.0f) < -80.0f);
    CHECK (tail (Cipher::Type::fftSmear, 0.8f, 0.0f) > -30.0f);
    CHECK (tail (Cipher::Type::fftReverb, 0.6f) > -30.0f);
    CHECK (tail (Cipher::Type::fftScatter, 0.6f, 0.6f, 0.0f) > -40.0f);
    CHECK (tail (Cipher::Type::fftCascade, 0.6f, 0.6f, 0.2f) > -40.0f);
    // A longer reverb TIME rings longer.
    CHECK (tail (Cipher::Type::fftReverb, 0.7f) > tail (Cipher::Type::fftReverb, 0.2f) + 15.0f);
}

TEST_CASE ("CIPHER stage 4: Stretch keeps the pitch; Compress, Corrupt, Lofi and Spread reshape the spectrum", "[effects][cipher]")
{
    // Stretch at 30%: a 440 Hz note stays at 440.
    const auto stretched = runTone (fftSlot (Cipher::Type::fftStretch, 0.3f, 0.0f, 0.0f), 440.0, 1.0);
    CHECK (bandLevel (stretched, 0, 440.0, 0.4, 1.0) > 0.5f * 0.25f);

    // A loud 500 Hz and a quiet 2 kHz (26 dB apart).
    const auto ratio = [] (const juce::AudioBuffer<float>& out) { return dB (toneLevel (out, 0, 2000.0, 0.5, 1.0) / toneLevel (out, 0, 500.0, 0.5, 1.0)); };
    const auto plain = ratio (runTwo (oneSlot (Cipher::Type::off), 500.0, 0.4f, 2000.0, 0.02f, 1.0));
    CAPTURE (plain);
    // Compress pulls them together (towards white at TONE right).
    CHECK (ratio (runTwo (fftSlot (Cipher::Type::fftCompress, 0.8f, 1.0f), 500.0, 0.4f, 2000.0, 0.02f, 1.0)) > plain + 10.0f);
    // Corrupt left takes out the quiet one; a little right of centre, the loud one.
    CHECK (ratio (runTwo (fftSlot (Cipher::Type::fftCorrupt, 0.1f, 0.0f), 500.0, 0.4f, 2000.0, 0.02f, 1.0)) < plain - 20.0f);
    CHECK (ratio (runTwo (fftSlot (Cipher::Type::fftCorrupt, 0.6f, 0.0f), 500.0, 0.4f, 2000.0, 0.02f, 1.0)) > plain + 20.0f);
    // Lofi at full FACTOR keeps only the loudest frequencies: the quiet one goes.
    CHECK (ratio (runTwo (fftSlot (Cipher::Type::fftLofi, 1.0f, 0.0f, 0.0f), 500.0, 0.4f, 2000.0, 0.02f, 1.0)) < plain - 20.0f);
    // Spread: a pure tone's energy reaches its neighbours (200 Hz away).
    const auto pure = runTone (fftSlot (Cipher::Type::fftSpread, 0.0f), 1000.0, 1.0);
    const auto spread = runTone (fftSlot (Cipher::Type::fftSpread, 0.6f), 1000.0, 1.0);
    CHECK (toneLevel (spread, 0, 1200.0, 0.4, 1.0) > 10.0f * toneLevel (pure, 0, 1200.0, 0.4, 1.0));
}

TEST_CASE ("CIPHER: CPU with ten spectral slots at 4096 (prints the share of a core)", "[.cpu]")
{
    Cipher cipher;
    cipher.prepare (sampleRate, blockSize);
    Cipher::Settings s;
    s.on = true;
    for (int i = 0; i < Cipher::numSlots; ++i)
    {
        auto& slot = s.slots[(size_t) i];
        slot.type = (Cipher::Type) ((int) Cipher::Type::fftShift + i);
        slot.option = Cipher::optionFor (slot.type, 3);
        slot.p1 = slot.p2 = 0.5f;
    }
    const auto started = juce::Time::getMillisecondCounterHiRes();
    run (cipher, s, 220.0, 10.0);
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    std::printf ("CIPHER, ten spectral slots at 4096: %.1f%% of a core, latency %d samples\n", seconds / 10.0 * 100.0, cipher.getLatencySamples());
}

//==============================================================================
// Stage 5: the macros.
TEST_CASE ("CIPHER stage 5: a macro moves its knobs by DEPTH along CURVE; the pedal can play MACRO 1", "[effects][cipher]")
{
    ParagonX1AudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    auto& state = processor.getParameters();
    const auto set = [&state] (const juce::String& id, float value)
    {
        auto* p = state.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    set (ParamIDs::cipherOn, 1.0f);
    set (ParamIDs::cipherSlot (1, "Type"), (float) Cipher::Type::gain);
    set (ParamIDs::cipherSlot (1, "Main"), 0.5f);   // 0 dB
    set (ParamIDs::cipherSlot (1, "EnvAmount"), 0.0f);
    processor.setCipherMacroAssignments ({ { 0, ParamIDs::cipherSlot (1, "Main"), -0.25f, 0.0f } });   // at the top: main 0.25, -6 dB

    const auto level = [&processor]
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        float last = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (0, i, 0.25f), buffer.setSample (1, i, 0.25f);
            processor.processBlock (buffer, midi);
            last = buffer.getSample (0, blockSize - 1);
        }
        return last;
    };
    const auto atZero = level();
    set (ParamIDs::cipherMacro (1), 1.0f);
    const auto atTop = level();
    CAPTURE (atZero, atTop);
    CHECK_THAT (dB (atTop / atZero), WithinAbs (-6.02, 0.15));
    // The knob's own setting doesn't change: the macro moves it as the sound is made.
    CHECK_THAT (state.getRawParameterValue (ParamIDs::cipherSlot (1, "Main"))->load(), WithinAbs (0.5, 0.001));

    // CURVE: at half way, a straight line is half the depth; +1 rises early (x^0.25), -1 late (x^4).
    CHECK_THAT (ParagonX1AudioProcessor::cipherMacroCurve (0.5f, 0.0f), WithinAbs (0.5, 1.0e-4));
    CHECK_THAT (ParagonX1AudioProcessor::cipherMacroCurve (0.5f, 1.0f), WithinAbs (std::pow (0.5, 0.25), 1.0e-4));
    CHECK_THAT (ParagonX1AudioProcessor::cipherMacroCurve (0.5f, -1.0f), WithinAbs (0.0625, 1.0e-4));

    // A master target: the output GAIN (24 dB either way across its travel, so +0.25 of it is +12 dB).
    processor.setCipherMacroAssignments ({ { 1, ParamIDs::cipherGain, 0.25f, 0.0f } });
    set (ParamIDs::cipherMacro (2), 1.0f);
    CHECK_THAT (dB (level() / atZero), WithinAbs (12.0, 0.15));

    // The expression pedal on CIPHER MACRO 1 plays it, whatever the knob says.
    processor.setCipherMacroAssignments ({ { 0, ParamIDs::cipherSlot (1, "Main"), -0.25f, 0.0f } });
    set (ParamIDs::cipherMacro (1), 0.0f);
    set (ParamIDs::cipherMacro (2), 0.0f);
    set (ParamIDs::expressionTarget, (float) ExpressionTargets::cipherMacro1);
    set (ParamIDs::expression, 1.0f);
    CHECK_THAT (dB (level() / atZero), WithinAbs (-6.02, 0.15));
}

TEST_CASE ("CIPHER stage 5: macros and their names are saved in rigs and projects, and undo covers them", "[effects][cipher][rigs]")
{
    ParagonX1AudioProcessor processor;
    const std::vector<ParagonX1AudioProcessor::CipherMacroAssignment> list {
        { 0, ParamIDs::cipherSlot (3, "P1"), 0.4f, 0.25f }, { 7, ParamIDs::cipherDrive, -0.5f, 0.0f } };
    processor.setCipherMacroAssignments (list);
    processor.setCipherMacroName (0, "sweep");
    processor.setCipherMacroName (2, "space");
    CHECK (processor.getCipherMacroName (0) == "SWEEP");
    CHECK (processor.getCipherMacroName (1) == "MACRO 2");

    const auto rig = processor.captureRig();
    const auto reloaded = Rig::fromXml (*rig.toXml());
    REQUIRE (reloaded.has_value());
    CHECK (reloaded->cipherMacros == rig.cipherMacros);
    CHECK (reloaded->cipherMacroNames == rig.cipherMacroNames);
    CHECK (ParagonX1AudioProcessor::cipherMacrosFromText (rig.cipherMacros) == list);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    ParagonX1AudioProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (other.getCipherMacroAssignments() == list);
    CHECK (other.getCipherMacroName (2) == "SPACE");
    CHECK (other.getCipherMacroName (1) == "MACRO 2");

    processor.flushUndoStep();
    processor.setCipherMacroAssignments ({});
    processor.flushUndoStep();
    processor.undo();
    CHECK (processor.getCipherMacroAssignments() == list);
    CHECK (ParagonX1AudioProcessor::cipherMacroTargetName (ParamIDs::cipherSlot (3, "P1")) == "Slot 3 Knob 1");
}
