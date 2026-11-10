// CIPHER, the envelope-controlled filter and multi-effect (inspired by
// Devious Machines' Infiltrator 2): each filter and utility checked on what
// it does to the sound, the envelope and its LFO, and the master section.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/effects/Cipher.h"
#include "PluginProcessor.h"

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
        s.slots[(size_t) i].type = (Cipher::Type) (1 + (i * 2) % 18);
        s.slots[(size_t) i].p1 = 0.7f;
    }
    s.master.driveOn = s.master.compOn = true;
    s.master.comp = Cipher::Comp::multiband;
    const auto started = juce::Time::getMillisecondCounterHiRes();
    run (cipher, s, 220.0, 10.0);
    const auto seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    std::printf ("CIPHER, ten busy slots: %.1f%% of a core\n", seconds / 10.0 * 100.0);
}
