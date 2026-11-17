#pragma once

#include "Circuit.h"
#include "EffectUtils.h"

namespace effects
{
    // AnalogOctave: the OCTAVER's ANALOG switch, modelled on the 1982 compact
    // octave pedal (the brown one), from its factory schematic (as published by
    // Hobby-Hour). How it makes its octaves, as drawn:
    //
    //   buffer -> op-amp, 4.7x (10k over 2.7k)  -> DIRECT
    //          -> tracking: a low-pass at about 700 Hz (22k / 0.01 uF, then
    //             330k / 330k with 2.2 nF and 220 pF), two peak followers (1 uF,
    //             1k in, 11k out) and two comparators setting and clearing a
    //             flip-flop: a square wave at the note's pitch. Flip-flops divide
    //             it by 2 and by 4.
    //          -> OCT 1: the guitar itself, through 1 uF onto a germanium diode
    //             to ground, then an op-amp stage a JFET switches between +0.50x
    //             and -0.57x at half the pitch; then a low-pass at about 320 Hz
    //             (22k / 0.022 uF, then 330k / 330k with 4.7 nF and 470 pF).
    //          -> OCT 2: OCT 1 the same way again, switched at a quarter of the
    //             pitch, through a low-pass at about 150 Hz (0.047 uF; 10 nF and 1 nF).
    //
    // The octaves aren't synthesised: they're the guitar, flipped up and down,
    // so they follow its dynamics. The diode is the trick: with the 1 uF
    // before it, it holds the wave's low peaks just under 0 V, shifting the
    // whole wave up; the switch turns that shift into a square wave an octave
    // down. Like the pedal, it wants single notes: chords, fading notes and
    // high notes confuse its tracking (a flip-flop jumping on the wrong edge).
    //
    // Modelled from the parts: every filter, the switch stages' gains, the
    // clamp (Newton's method on the diode, with the stage's own loading in each
    // switch position), the peak followers and comparators, the dividers.
    // Our reading: ideal switches and flip-flops (they switch in a sample at
    // 4x the rate), and the germanium diodes as a typical small-signal part.
    class AnalogOctave
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        // The guitar in; out, at the base rate: DIRECT (after the 4.7x stage),
        // OCT 1 and OCT 2 (each before its level pot).
        void process (const float* input, float* direct, float* octaveOne, float* octaveTwo, int numSamples) noexcept;

        // The pedal's passive mixer: each pot (100k linear) through 100k into
        // one node. Levels 0 to 1; the result is scaled so DIRECT at full, the
        // octaves down, gives the guitar back at its own level (ours: the pedal
        // itself is about 4 dB louder there).
        static float mix (float direct, float octaveOne, float octaveTwo, float directLevel, float oneLevel, float twoLevel) noexcept;

        static constexpr double inputGain = 1.0 + 10.0 / 2.7;   // IC1a: R7 over R6

    private:
        // One OCT stage: the clamp (C24 / C29 into D10 / D11), the switched
        // op-amp, then its low-pass.
        struct Stage
        {
            double gC = 0.0, history = 0.0, clampVolts = 0.0, clampVoltsPrev = 0.0;
            Circuit lowPass;
            void reset() noexcept { history = clampVolts = clampVoltsPrev = 0.0; lowPass.reset(); }
            double process (double in, bool switchOn) noexcept;
        };

        double rate = 192000.0;
        juce::dsp::Oversampling<float> oversampling { 3, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
        juce::AudioBuffer<float> work;

        double inputG = 0.0, inputIn = 0.0, inputOut = 0.0;   // C3 into R6: 5.9 Hz, the 4.7x stage's bass corner
        Circuit trackFilter, trackDetect;                      // the tracking low-pass, read at the comparators and at the peak followers
        double peakUp = 0.0, peakDown = 0.0, chargeCoefficient = 0.0, releaseCoefficient = 0.0;
        bool square = false, half = false, quarter = false;
        Stage one, two;
    };
}
