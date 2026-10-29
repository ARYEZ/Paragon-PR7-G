#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/effects/Havok.h"
#include "ui/Faceplate.h"
#include "ui/HavokPanel.h"
#include "ui/SignalChain.h"

// HAVOK, the touch pad (inspired by the original Korg Kaoss Pad): the
// manual's programs and pad behaviour, and what each kind of program does
// to the sound.

using Catch::Matchers::WithinAbs;
using effects::Havok;

namespace
{
    constexpr double rate = 48000.0;
    constexpr int blockSize = 480;   // 10 ms
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // A Havok on the bench: plays a signal through, block by block, and keeps the output.
    struct Bench
    {
        Havok havok;
        Havok::Settings settings;
        juce::AudioBuffer<float> buffer { 2, blockSize };
        double time = 0.0;

        explicit Bench (int program)
        {
            havok.prepare (rate, blockSize);
            settings.on = true;
            settings.program = program;
            settings.touch = false;
            play (0.05f, [] (double) { return 0.0f; });   // the switch to the program settles
        }

        // `seconds` of `source (time)` on both sides; returns the output (left, right).
        template <typename Source>
        std::pair<std::vector<float>, std::vector<float>> play (float seconds, Source&& source)
        {
            std::pair<std::vector<float>, std::vector<float>> out;
            const auto blocks = (int) std::lround (seconds * rate / blockSize);
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    const auto x = source (time + i / rate);
                    buffer.setSample (0, i, x);
                    buffer.setSample (1, i, x);
                }
                time += blockSize / rate;
                havok.process (juce::dsp::AudioBlock<float> (buffer), settings);
                out.first.insert (out.first.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
                out.second.insert (out.second.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + blockSize);
            }
            return out;
        }

        std::pair<std::vector<float>, std::vector<float>> sine (float seconds, float hz, float amplitude = 0.25f)
        {
            return play (seconds, [hz, amplitude] (double t) { return amplitude * (float) std::sin (std::fmod (twoPi * hz * t, (double) twoPi)); });
        }
        std::pair<std::vector<float>, std::vector<float>> silence (float seconds)
        {
            return play (seconds, [] (double) { return 0.0f; });
        }

        void touch (float x, float y) { settings.touch = true; settings.x = x; settings.y = y; }
        void release() { settings.touch = false; }
    };

    float rms (const std::vector<float>& signal, size_t from = 0, size_t to = 0)
    {
        to = to == 0 ? signal.size() : juce::jmin (to, signal.size());
        double sum = 0.0;
        for (auto i = from; i < to; ++i)
            sum += (double) signal[i] * signal[i];
        return to > from ? (float) std::sqrt (sum / (double) (to - from)) : 0.0f;
    }

    // One frequency's amplitude in part of a signal (Goertzel).
    float level (const std::vector<float>& signal, float hz, size_t from = 0)
    {
        const auto coefficient = 2.0 * std::cos (twoPi * hz / rate);
        double s1 = 0.0, s2 = 0.0;
        for (auto i = from; i < signal.size(); ++i)
        {
            const auto s = signal[i] + coefficient * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        const auto n = (double) (signal.size() - from);
        return (float) (2.0 * std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2)) / n);
    }

    float db (float gain) { return juce::Decibels::gainToDecibels (gain, -200.0f); }

    void set (ParagonX1AudioProcessor& processor, const juce::String& id, float value)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    template <typename Type>
    void findAll (juce::Component& parent, std::vector<Type*>& found)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* match = dynamic_cast<Type*> (child))
                found.push_back (match);
            findAll (*child, found);
        }
    }
}

TEST_CASE ("HAVOK has the manual's 60 programs, with what X and Y do on each", "[havok]")
{
    const auto& programs = Havok::programs();
    REQUIRE (programs.size() == 60);
    CHECK (juce::String (programs[0].name) == "LOW PASS FILTER");
    CHECK (juce::String (programs[0].y) == "DI/FX BALANCE, RESONANCE");
    CHECK (juce::String (programs[17].name) == "PITCH SHIFT");
    CHECK (juce::String (programs[20].name) == "TAPE ECHO");
    CHECK (juce::String (programs[31].name) == "GATE REVERB (HALL)");
    CHECK (juce::String (programs[46].x) == "PAN (L/R)");
    CHECK (juce::String (programs[59].name) == "SAMPLE/PLAY2+PAN");
    CHECK (Havok::programNames()[0] == "1 LOW PASS FILTER");
    CHECK (Havok::programNames()[59] == "60 SAMPLE/PLAY2+PAN");
    for (int p = 0; p < 60; ++p)
    {
        CAPTURE (p);
        CHECK (Havok::isSampler (p) == (p >= 50));
        CHECK (juce::String (programs[(size_t) p].hint).isNotEmpty());
    }
}

TEST_CASE ("HAVOK: the effect is on only while the pad is touched, or HOLD keeps it", "[havok]")
{
    // The low-pass filter, closed right down: 2 kHz goes through untouched until the pad is touched.
    Bench bench (1);
    const auto open = bench.sine (0.2f, 2000.0f);
    CHECK_THAT (db (rms (open.first, 2400)), WithinAbs (db (0.25f / std::sqrt (2.0f)), 0.1));

    bench.touch (0.2f, 0.0f);
    const auto touched = bench.sine (0.2f, 2000.0f);
    CHECK (db (rms (touched.first, 2400)) < db (0.25f / std::sqrt (2.0f)) - 30.0f);

    // Let go: straight back to the dry sound.
    bench.release();
    const auto released = bench.sine (0.2f, 2000.0f);
    CHECK_THAT (db (rms (released.first, 2400)), WithinAbs (db (0.25f / std::sqrt (2.0f)), 0.2));

    // HOLD keeps the effect at the last position without a finger.
    bench.settings.hold = true;
    const auto held = bench.sine (0.2f, 2000.0f);
    CHECK (db (rms (held.first, 2400)) < db (0.25f / std::sqrt (2.0f)) - 30.0f);

    // Switched off (FX ON), it's the dry sound whatever the pad does.
    bench.settings.on = false;
    const auto off = bench.sine (0.2f, 2000.0f);
    CHECK_THAT (db (rms (off.first, 2400)), WithinAbs (db (0.25f / std::sqrt (2.0f)), 0.1));
}

TEST_CASE ("HAVOK: X sweeps the filter (low-pass: lows pass, highs go as X comes down)", "[havok]")
{
    Bench bench (1);
    bench.touch (0.95f, 0.0f);   // nearly wide open
    const auto openHigh = bench.sine (0.3f, 5000.0f);
    bench.touch (0.55f, 0.0f);   // about 540 Hz
    const auto closedHigh = bench.sine (0.3f, 5000.0f);
    const auto closedLow = bench.sine (0.3f, 60.0f);
    CHECK (db (rms (openHigh.first, 4800)) > db (0.177f) - 1.0f);
    CHECK (db (rms (closedHigh.first, 4800)) < db (0.177f) - 35.0f);   // over three octaves above, at 12 dB per octave
    CHECK (db (rms (closedLow.first, 4800)) > db (0.177f) - 3.0f);
}

TEST_CASE ("HAVOK: tap the delay on one note and only that note echoes", "[havok]")
{
    // DELAY (22): X the time, Y the level. A click while touching, a click while not.
    Bench bench (21);
    bench.touch (0.6f, 1.0f);   // about 240 ms (10 ms to 2 s across the pad)
    const auto expectedMs = 10.0f * std::pow (200.0f, 0.6f);
    bench.silence (0.5f);   // the time glides to where X is
    auto click = [] (double t) { return t < 0.002 ? 0.8f : 0.0f; };
    bench.time = 0.0;
    const auto tapped = bench.play (0.6f, click);
    const auto echoAt = (size_t) (expectedMs * 0.001f * rate);
    float peak = 0.0f;
    size_t peakAt = 0;
    for (size_t i = 200; i < tapped.first.size(); ++i)
        if (std::abs (tapped.first[i]) > peak)
        {
            peak = std::abs (tapped.first[i]);
            peakAt = i;
        }
    CAPTURE (peakAt, echoAt);
    CHECK (peak > 0.2f);
    CHECK (std::abs ((int) peakAt - (int) echoAt) < 100);

    // Let go and let the echoes die; a click now doesn't go in.
    bench.release();
    bench.silence (3.0f);
    bench.time = 0.0;
    const auto untapped = bench.play (0.6f, click);
    CHECK (rms (untapped.first, 400) < 1.0e-4f);
}

TEST_CASE ("HAVOK: a reverb rings out after you let go; the gate reverbs cut off", "[havok]")
{
    const auto tailAfter = [] (int program, float seconds)
    {
        Bench bench (program);
        bench.touch (0.0f, 1.0f);   // no low cut, full level
        juce::Random noise (1);
        bench.play (0.3f, [&] (double) { return 0.2f * (noise.nextFloat() * 2.0f - 1.0f); });
        bench.release();
        bench.silence (seconds);
        return rms (bench.silence (0.1f).first);
    };
    // The hall's tail still sounds half a second on; the gated hall's has stopped.
    const auto hall = tailAfter (30, 0.5f), gated = tailAfter (31, 0.5f);
    CAPTURE (hall, gated);
    CHECK (db (hall) > -50.0f);
    CHECK (db (gated) < db (hall) - 30.0f);
    // The REVERB program's TIME (X): longer at the right.
    Bench shortRoom (39), longRoom (39);
    juce::Random noise (2);
    for (auto* bench : { &shortRoom, &longRoom })
    {
        bench->touch (bench == &shortRoom ? 0.1f : 0.9f, 1.0f);
        bench->play (0.3f, [&] (double) { return 0.2f * (noise.nextFloat() * 2.0f - 1.0f); });
        bench->release();
        bench->silence (1.0f);
    }
    CHECK (db (rms (longRoom.silence (0.1f).first)) > db (rms (shortRoom.silence (0.1f).first)) + 20.0f);
}

TEST_CASE ("HAVOK: at the pad's centre every effect program stays near the dry level", "[havok]")
{
    // Programs 1-50 touched in the middle: within 10 dB of the dry sound
    // (filters cut and echoes build, so not exactly level; none jumps out).
    const auto tones = [] (double t) { return 0.2f * (float) std::sin (twoPi * 220.0 * t) + 0.1f * (float) std::sin (twoPi * 1700.0 * t); };
    for (int p = 0; p < Havok::firstSampler; ++p)
    {
        CAPTURE (p + 1, Havok::programs()[(size_t) p].name);
        Bench bench (p);
        const auto dry = rms (bench.play (0.6f, tones).first, 9600);
        bench.touch (0.5f, 0.5f);
        const auto touched = rms (bench.play (0.8f, tones).first, 9600);
        CHECK (std::abs (db (touched) - db (dry)) < 10.0f);
    }
}

TEST_CASE ("HAVOK: PITCH SHIFT moves each side by up to two octaves", "[havok]")
{
    Bench bench (17);
    bench.touch (1.0f, 0.5f);   // left side two octaves up, right side unison
    const auto out = bench.sine (0.5f, 220.0f);
    CHECK (level (out.first, 880.0f, 9600) > 4.0f * level (out.first, 220.0f, 9600));
    CHECK (level (out.second, 220.0f, 9600) > 4.0f * level (out.second, 880.0f, 9600));
}

TEST_CASE ("HAVOK: the ring modulator makes sum and difference tones", "[havok]")
{
    // RING MOD+FILTER: X the ring frequency (1 Hz to 12 kHz), Y the filter (open at the top).
    Bench bench (41);
    const auto x = std::log (1000.0f) / std::log (12000.0f);   // 1 kHz
    bench.touch (x, 1.0f);
    const auto out = bench.sine (0.5f, 200.0f);
    CHECK (level (out.first, 800.0f, 9600) > 0.05f);
    CHECK (level (out.first, 1200.0f, 9600) > 0.05f);
    CHECK (level (out.first, 200.0f, 9600) < 0.01f);
}

TEST_CASE ("HAVOK: DISTORTION adds harmonics as X turns the gain up; TREMOLO throbs", "[havok]")
{
    Bench gentle (9), hot (9);
    gentle.touch (0.0f, 0.8f);
    hot.touch (1.0f, 0.8f);
    const auto clean = gentle.sine (0.3f, 200.0f), driven = hot.sine (0.3f, 200.0f);
    const auto third = [] (const std::vector<float>& s) { return level (s, 600.0f, 4800) / level (s, 200.0f, 4800); };
    CHECK (third (clean.first) < 0.01f);
    CHECK (third (driven.first) > 0.1f);

    // TREMOLO: X the speed, Y the depth. Full depth: the 10 ms levels swing widely.
    Bench tremolo (18);
    tremolo.touch (0.3f, 1.0f);
    const auto out = tremolo.sine (1.0f, 1000.0f);
    float low = 1.0f, high = 0.0f;
    for (size_t at = 4800; at + 480 <= out.first.size(); at += 480)
    {
        const auto r = rms (out.first, at, at + 480);
        low = juce::jmin (low, r);
        high = juce::jmax (high, r);
    }
    CHECK (db (high) - db (low) > 12.0f);
}

TEST_CASE ("HAVOK: SAMPLE/PLAY records a phrase with REC and plays it from the pad", "[havok][sampler]")
{
    Bench bench (50);   // SAMPLE/PLAY1
    CHECK (bench.havok.getRecState() == Havok::Rec::ready);
    bench.havok.pressRec();
    bench.sine (0.5f, 440.0f);   // recording: the sound passes unchanged
    CHECK (bench.havok.getRecState() == Havok::Rec::recording);
    bench.havok.pressRec();
    bench.silence (0.01f);
    CHECK (bench.havok.getRecState() == Havok::Rec::recorded);
    CHECK_THAT (bench.havok.getRecordedSeconds(), WithinAbs (0.5, 0.02));

    // Not touching: nothing plays. Touching: the phrase, in place of the sound coming in.
    CHECK (rms (bench.silence (0.2f).first) < 1.0e-5f);
    bench.touch (1.0f, 1.0f);   // the whole phrase, full level
    const auto played = bench.sine (0.4f, 1000.0f);
    CHECK (level (played.first, 440.0f, 2400) > 0.15f);
    CHECK (level (played.first, 1000.0f, 2400) < 0.02f);
    CHECK (bench.havok.getPlayPosition() >= 0.0f);
    bench.release();

    // DIRECT MIX (57): the sound coming in stays under the phrase.
    Bench direct (56);
    direct.havok.pressRec();
    direct.sine (0.5f, 440.0f);
    direct.havok.pressRec();
    direct.touch (1.0f, 1.0f);
    const auto mixed = direct.sine (0.4f, 1000.0f);
    CHECK (level (mixed.first, 440.0f, 2400) > 0.15f);
    CHECK (level (mixed.first, 1000.0f, 2400) > 0.2f);

    // Changing program erases the phrase, as the manual says.
    bench.settings.program = 51;
    bench.silence (0.05f);
    CHECK (bench.havok.getRecState() == Havok::Rec::ready);
}

TEST_CASE ("HAVOK: AUTO REC waits for sound; FORWARD/REVERSE plays backwards left of centre; TIME STRETCH keeps the pitch", "[havok][sampler]")
{
    Bench autoRec (53);   // SAMPLE/PLAY1 (AUTO REC)
    autoRec.havok.pressRec();
    autoRec.silence (0.2f);
    CHECK (autoRec.havok.getRecState() == Havok::Rec::waiting);
    autoRec.sine (0.3f, 440.0f);
    CHECK (autoRec.havok.getRecState() == Havok::Rec::recording);
    CHECK_THAT (autoRec.havok.getRecordedSeconds(), WithinAbs (0.3, 0.02));

    // A rising ramp recorded, played from the far left: it comes out falling.
    Bench scratch (51);   // SAMPLE/PLAY2
    scratch.havok.pressRec();
    scratch.time = 0.0;
    scratch.play (0.5f, [] (double t) { return (float) t; });
    scratch.havok.pressRec();
    scratch.touch (0.0f, 1.0f);
    const auto out = scratch.silence (0.2f).first;
    int falling = 0, rising = 0;
    for (size_t i = 2400; i + 1 < out.size(); ++i)
        (out[i + 1] < out[i] ? falling : rising) += 1;
    CHECK (falling > 4 * rising);

    // TIME STRETCH at its slowest: still 440 Hz.
    Bench stretch (52);   // SAMPLE/PLAY3
    stretch.havok.pressRec();
    stretch.sine (0.5f, 440.0f);
    stretch.havok.pressRec();
    stretch.touch (0.0f, 1.0f);
    const auto slow = stretch.silence (0.5f).first;
    CHECK (level (slow, 440.0f, 2400) > 4.0f * level (slow, 220.0f, 2400));
    CHECK (level (slow, 440.0f, 2400) > 4.0f * level (slow, 880.0f, 2400));
}

TEST_CASE ("HAVOK: every program runs cleanly, touched or held, and switching programs doesn't click", "[havok]")
{
    Bench bench (0);
    juce::Random noise (3);
    for (int p = 0; p < 60; ++p)
    {
        CAPTURE (p);
        bench.settings.program = p;
        bench.release();
        bench.silence (0.03f);
        for (const auto position : { 0.0f, 0.5f, 1.0f })
        {
            bench.touch (position, 1.0f - position);
            const auto out = bench.play (0.15f, [&] (double t) { return 0.3f * (float) std::sin (twoPi * 330.0 * t) + 0.05f * (noise.nextFloat() - 0.5f); });
            float peak = 0.0f;
            for (auto v : out.first) peak = juce::jmax (peak, std::abs (v));
            for (auto v : out.second) peak = juce::jmax (peak, std::abs (v));
            CHECK (std::isfinite (peak));
            CHECK (peak < 4.0f);
        }
    }

    // A switch dips through the dry sound: no step bigger than the tone's own.
    bench.settings.program = 0;
    bench.touch (0.5f, 0.5f);
    bench.sine (0.2f, 200.0f);
    bench.settings.program = 9;
    const auto out = bench.sine (0.1f, 200.0f, 0.25f).first;
    float step = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        step = juce::jmax (step, std::abs (out[i] - out[i - 1]));
    CHECK (step < 0.1f);
}

TEST_CASE ("HAVOK in the plugin: last in the chain, played by MIDI CC 12, 13 and 92 as the original sends them", "[havok]")
{
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (1, 2, rate, blockSize);
    processor.prepareToPlay (rate, blockSize);
    set (processor, ParamIDs::havokOn, 1.0f);
    set (processor, ParamIDs::havokProgram, 1.0f);   // LOW PASS FILTER (2)

    juce::AudioBuffer<float> buffer (2, blockSize);
    double time = 0.0;
    const auto play = [&] (juce::MidiBuffer& midi, int blocks)
    {
        float sum = 0.0f;
        int count = 0;
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (0, i, 0.25f * (float) std::sin (twoPi * 3000.0 * (time + i / rate)));
            time += blockSize / rate;
            processor.processBlock (buffer, midi);
            midi.clear();
            if (b > blocks / 2)
                for (int i = 0; i < blockSize; ++i, ++count)
                    sum += buffer.getSample (0, i) * buffer.getSample (0, i);
        }
        return std::sqrt (sum / (float) count);
    };

    juce::MidiBuffer midi;
    const auto dry = play (midi, 20);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 12, 0), 0);    // X: far left (filter shut)
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 13, 0), 0);    // Y: bottom
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 92, 127), 0);  // touching
    const auto touched = play (midi, 20);
    CHECK (db (touched) < db (dry) - 40.0f);

    // The timer copies the MIDI pad into the parameters (so the DAW records it and the pad shows it).
    processor.flushMidiPad();
    CHECK (processor.getParameters().getRawParameterValue (ParamIDs::havokTouch)->load() > 0.5f);
    CHECK_THAT (processor.getParameters().getRawParameterValue (ParamIDs::havokX)->load(), WithinAbs (0.0, 1.0e-3));

    midi.addEvent (juce::MidiMessage::controllerEvent (1, 92, 0), 0);    // let go
    const auto released = play (midi, 20);
    CHECK_THAT (db (released), WithinAbs (db (dry), 0.5));

    // TOUCH isn't part of a rig (it's where your finger is); the program and HOLD are.
    CHECK (ParamIDs::notInRigs.contains (ParamIDs::havokTouch));
    const auto rig = processor.captureRig();
    CHECK (rig.parameters.count (ParamIDs::havokProgram) == 1);
    CHECK (rig.parameters.count (ParamIDs::havokHold) == 1);
    CHECK (rig.parameters.count (ParamIDs::havokTouch) == 0);
}

TEST_CASE ("HAVOK's panel: the pad plays, the display reads 1.1 to 9.9, PROGRAM MAP keys store and recall", "[havok][ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    std::vector<SignalChain*> chains;
    findAll (*editor, chains);
    REQUIRE (chains.size() == 1);
    std::vector<HavokPanel*> panels;
    findAll (*editor, panels);
    REQUIRE (panels.size() == 1);
    auto& panel = *panels[0];
    std::vector<Faceplate*> faceplates;
    findAll (*editor, faceplates);
    REQUIRE (faceplates.size() == 1);

    // HAVOK is the 14th block, last in the chain; its panel covers the rack's display and controls.
    CHECK_FALSE (panel.isVisible());
    chains[0]->onSelect (13);
    CHECK (panel.isVisible());
    CHECK (faceplates[0]->getLook() == Faceplate::Look::havok);
    const auto& rack = *faceplates[0];
    CHECK (panel.getBounds().contains (rack.getDisplayArea() + rack.getPosition()));
    CHECK (panel.getBounds().contains (rack.getControlArea() + rack.getPosition()));
    CHECK (panel.getPadBounds().getWidth() == panel.getPadBounds().getHeight());   // square, as the original's

    auto& parameters = processor.getParameters();
    const auto raw = [&] (const char* id) { return parameters.getRawParameterValue (id)->load(); };
    CHECK (panel.getDisplayText() == "1");

    // Touching: X and Y follow the finger and the display shows where (1.1 bottom left, 9.9 top right).
    panel.touchPad ({ 0.0f, 0.0f });
    CHECK (raw (ParamIDs::havokTouch) > 0.5f);
    CHECK (panel.getDisplayText() == "1.1");
    panel.touchPad ({ 1.0f, 1.0f });
    CHECK (panel.getDisplayText() == "9.9");
    panel.touchPad ({ 0.3f, 0.85f });
    CHECK_THAT (raw (ParamIDs::havokX), WithinAbs (0.3, 0.02));
    CHECK_THAT (raw (ParamIDs::havokY), WithinAbs (0.85, 0.02));
    CHECK (panel.getDisplayText() == "3.8");   // the manual's own example
    panel.releasePad();
    CHECK (raw (ParamIDs::havokTouch) < 0.5f);
    CHECK (panel.getDisplayText() == "1");

    // Store program 21 with HOLD at 3.8 on key 4, go elsewhere, recall it.
    set (processor, ParamIDs::havokProgram, 20.0f);
    set (processor, ParamIDs::havokHold, 1.0f);
    panel.storeMap (4);
    set (processor, ParamIDs::havokProgram, 5.0f);
    set (processor, ParamIDs::havokHold, 0.0f);
    set (processor, ParamIDs::havokX, 0.9f);
    panel.recallMap (4);
    CHECK (juce::roundToInt (raw (ParamIDs::havokProgram)) == 20);
    CHECK (raw (ParamIDs::havokHold) > 0.5f);
    CHECK_THAT (raw (ParamIDs::havokX), WithinAbs (0.3, 0.02));

    // Every program draws.
    for (int p = 0; p < 60; ++p)
    {
        set (processor, ParamIDs::havokProgram, (float) p);
        CHECK (panel.createComponentSnapshot (panel.getLocalBounds()).isValid());
    }
}
