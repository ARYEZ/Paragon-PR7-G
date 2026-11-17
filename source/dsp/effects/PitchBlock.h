#pragma once

#include "AnalogOctave.h"
#include "PitchShifter.h"

namespace effects
{
    // The PITCH block, first in the chain, before anything else touches the
    // guitar (as these pedals sit at the start of a real board):
    //
    //   TUNING   virtual retuning: the whole guitar shifted by -12 to +12
    //            semitones, as if every string were tuned down (or capoed
    //            up). -2 plays standard-tuned riffs a whole step down (D
    //            standard), -5 in B. Every string moves together: it can't
    //            drop one string alone, as drop D does. 0 is off: the guitar
    //            passes untouched.
    //   OCTAVER  a POG-style octave pedal: the guitar two octaves down, one
    //            octave down, as it is, and one octave up, each with its own
    //            level, so you can blend organ-like stacks. The shifters
    //            work on the whole sound, so chords track too (with a little
    //            shimmer on dense chords). ANALOG (PR7-G 0.89.0) is
    //            instead modelled on the 1982 compact octave pedal: one and two
    //            octaves down by flip-flops, from single notes (see
    //            AnalogOctave.h); 2 DOWN, 1 DOWN and DRY are its OCT 2, OCT 1
    //            and DIRECT, and 1 UP isn't used.
    //   FREEZE   holds what you just played as an endless pad: switch it on
    //            and the last moment of playing is caught and sustained,
    //            smoothly, while you play over it. ATTACK swells it in,
    //            RELEASE lets it fade when switched off.
    class PitchBlock
    {
    public:
        struct Settings
        {
            float tuneSemitones = 0.0f;   // -12 to +12; 0 is off
            bool octaverOn = false;
            float subTwo = 0.0f;          // levels, 0 to 1: two octaves down...
            float subOne = 0.5f;          // ...one octave down...
            float dry = 1.0f;             // ...the guitar as it is...
            float upOne = 0.0f;           // ...and one octave up
            bool analog = false;          // ANALOG: false the shifters, true the brown pedal
            bool freezeOn = false;
            float freezeLevel = 0.7f;     // 0 to 1
            float freezeAttack = 0.3f;    // seconds to swell in
            float freezeRelease = 1.5f;   // seconds to fade out
        };

        void prepare (double sampleRate, int maxBlockSize);
        void process (juce::dsp::AudioBlock<float> monoBlock, const Settings& settings) noexcept;

        // How far the frozen pad is faded in, 0 to 1 (for tests).
        float getFreezeAmount() const noexcept { return freezeAmount; }

    private:
        void captureFreeze() noexcept;
        float freezeSample() noexcept;

        double sampleRate = 48000.0;

        // TUNING: a shifter on the whole guitar, faded in and out so moving
        // between 0 (untouched) and a tuning doesn't click.
        PitchShifter tuner;
        juce::SmoothedValue<float> tunedAmount, tuneRatio;

        // OCTAVER
        PitchShifter downTwo, downOne, upOneShifter;
        BypassFade octaverBypass;
        AnalogOctave analogOctave;
        std::vector<float> analogDirect, analogOne, analogTwo;
        bool wasAnalog = false;
        juce::SmoothedValue<float> subTwoLevel, subOneLevel, dryLevel, upLevel;

        // FREEZE: the last moment of playing, always kept; when FREEZE goes
        // on it's copied and played back as overlapping, smoothly windowed
        // grains from random places in it, so it sustains without a loop's
        // repeating click or pulse.
        std::vector<float> recent, frozen;
        int recentWrite = 0, frozenLength = 0;
        struct Grain
        {
            bool active = false;
            int start = 0, length = 0, played = 0;
        };
        std::array<Grain, 8> grains;
        int spawnCountdown = 0;
        bool wasFrozen = false;
        float freezeAmount = 0.0f;   // faded in over ATTACK, out over RELEASE
        juce::Random random { 0xf2ee2e };
    };
}
