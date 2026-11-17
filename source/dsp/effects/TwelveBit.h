#pragma once

#include "Circuit.h"
#include "EffectUtils.h"

#include <vector>
#include <cstdint>

namespace effects
{
    // TwelveBit: DELAY's 12-BIT type, modelled on the 1983 compact digital
    // delay (the first digital delay in a compact pedal; reissued unchanged as
    // its successor), from its factory circuit diagram (as published by
    // Hobby-Hour). Its path, as drawn:
    //
    //   buffer -> pre-emphasis (+15 dB shelf, 594 Hz to 3.4 kHz)
    //          -> 6-pole low-pass (three transistor Sallen-Key stages, "fc = 7 kHz")
    //          -> + FEEDBACK -> 2:1 compressor (NE570, with its own treble lift on the way in)
    //          -> 12-bit converter, 16384 samples of memory (three 64k-bit RAMs)
    //          -> held output -> 4-pole low-pass (two Sallen-Key stages)
    //          -> 1:2 expander (and the treble lift taken off again) -> FEEDBACK and E.LEVEL
    //          -> de-emphasis (the shelf taken off again), mixed with the direct sound
    //
    // The delay time comes from the clock, not from where it reads: the memory
    // is always 1024 samples (S, 12.5-50 ms), 4096 (M, 50-200 ms) or 16384 (L,
    // 200-800 ms) long, and the TIME knob speeds the clock up or slows it down
    // between 20.5 and 82 kHz. So at the long end of each range it samples at
    // only 20.5 kHz, and what's above about 10 kHz folds back down as grit;
    // and turning TIME bends the repeats' pitch, as on the pedal.
    //
    // The compander (a 2:1 compressor before the converter, a 1:2 expander
    // after) is how 12 bits sound quieter than 12 bits: quiet passages go
    // into the converter louder and come out turned down again, its steps with
    // them. Both sides follow the level with the NE570's averaging rectifier
    // (10k into 0.68 uF: 6.8 ms), so the steps' noise rises and falls with the
    // playing, and fast changes get through a little uneven.
    //
    // Modelled from the parts: the emphasis shelves, both filters (with the
    // emitter followers' own gain just under one, which tames their peaks a
    // little), the compander's networks and timing, the converter's 12-bit
    // steps and clipping, the memory and clock, the pots' loading. Our reading:
    // where the compander has unity gain and where the converter clips (1 V and
    // +/-2.5 V: the chip runs on 5 V), how far the compressor can turn up (40 dB),
    // that the repeats come back the same way up as the guitar, and that the
    // TIME knob's travel follows its "A" taper evenly in time. The controller
    // chip's own workings aren't published; the memory size and ranges above
    // are worked out from its RAMs and the specification (12.5-800 ms).
    class TwelveBit
    {
    public:
        static constexpr double minSeconds = 0.0125, maxSeconds = 0.8;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        // The echo time (clamped to 12.5-800 ms; picks the S, M or L range) and
        // F.BACK (0 to 1). Call once per block.
        void setTime (double seconds) noexcept;
        void setFeedback (double knob) noexcept;

        // In place: what goes into the pedal (volts) in, the delayed sound out,
        // as it reaches the mixer with E.LEVEL all the way up.
        void process (float* samples, int numSamples) noexcept;

        // How many samples of memory a time uses, and the clock it then runs at.
        static int memoryFor (double seconds) noexcept;
        static double clockFor (double seconds) noexcept;
        // E.LEVEL and F.BACK: 50k linear pots, loaded by the 33k and 47k after their wipers.
        static double levelGain (double knob) noexcept;
        static double feedbackGain (double knob) noexcept;

    private:
        struct Biquad
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
            // The bilinear transform of (nb0 + nb1 s + nb2 s^2) / (na0 + na1 s + na2 s^2), exact at matchHz.
            void setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double rate, double matchHz) noexcept;
            double process (double x) noexcept
            {
                const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return y;
            }
            void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
        };

        double rate = 192000.0;          // the oversampled rate everything here runs at
        juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };

        Biquad inputCoupling, preEmphasis, antiAlias[3], compressorLift, reconstruction[2], expanderCut, deEmphasis, feedbackCoupling;

        // The compander: each side's rectifier, and the feedback from the last sample.
        double rectifierCoefficient = 0.0, compressorLevel = 0.0, expanderLevel = 0.0, fedBack = 0.0;
        double lastCompressed = 0.0;

        // The converter and memory: the clock's phase, the held output, the steps.
        std::vector<std::int16_t> memory = std::vector<std::int16_t> (16384, 0);   // on the heap: 32 KB is too much for a plugin object that hosts (and tests) may keep on the stack
        int writeIndex = 0, length = 16384;
        double clock = 20480.0, clockTarget = 20480.0, clockGlide = 0.0, phase = 0.0;
        double held = 0.0;
        double feedbackKnob = 0.0;
        bool clockSettled = false;   // false after a reset: the next TIME jumps there instead of gliding
    };
}
