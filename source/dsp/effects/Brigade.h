#pragma once

#include "Circuit.h"
#include "EffectUtils.h"

#include <vector>

namespace effects
{
    // Brigade: DELAY's BRIGADE type, modelled on the 1981 compact analog
    // delay, from its factory schematic and service-manual adjustment notes
    // (as published by Hobby-Hour). Its path, as drawn:
    //
    //   buffer -> pre-emphasis (+15 dB shelf, 410 Hz to 2.3 kHz, as the CE-2's)
    //          -> + INTENSITY -> NE570 compressor -> low-pass -> bucket-brigade
    //          delay chip (MN3005, 4096 stages) -> low-pass -> NE570 expander
    //          -> INTENSITY and ECHO -> de-emphasis, mixed with the direct sound
    //
    // The delay is the clock: the chip moves the signal one stage per half
    // clock, so 4096 stages take 2048 / clock seconds. The service notes set
    // the clock to 6.8 kHz at the longest REPEAT RATE (301 ms, the specified
    // 300 ms); at 20 ms it runs at 102 kHz. So at long delays the chip
    // samples at only 6.8 kHz: the filters around it cut steeply near 3 kHz
    // (the dark repeats), and what gets past still folds back as grit.
    //
    // From the circuit: the emphasis shelves; the compander's 2.2 ms
    // rectifiers (10k into 0.22 uF, both sides); the clock range; the
    // INTENSITY feedback's 0.1 uF into 22k (a 72 Hz cut, so each repeat
    // loses a little more bass); the 50k linear INTENSITY and ECHO pots,
    // loaded by the 22k and 47k after them. From the data sheet: the chip's
    // distortion (1% at 0 dBm, the service notes' test level).
    //
    // Our reading (owner's choice, PR7-G 0.88.0): the filters around the chip.
    // As drawn (3rd order in, 3rd + 2nd order out, 10k with 6.8n / 82n / 330p,
    // 2.2n / 33n / 1n and 39n / 330p) they lift about 9 dB around 2 kHz, which
    // would make the repeats ring; a misprint or misread value is likelier
    // than the real pedal doing that. So they're smooth (Butterworth) filters
    // of the same orders, at 3.2 kHz, where the drawn ones are 3 dB down.
    // Also ours: the compander's unity level (1 V) and ceiling (40 dB), and
    // how strongly INTENSITY feeds back at full (the edge of self-oscillation).
    class Brigade
    {
    public:
        static constexpr double minSeconds = 0.02, maxSeconds = 0.3;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        // The echo time (clamped to 20-300 ms) and INTENSITY (0 to 1). Call once per block.
        void setTime (double seconds) noexcept;
        void setFeedback (double knob) noexcept;

        // In place: the guitar in, the delayed sound out (ECHO all the way up).
        void process (float* samples, int numSamples) noexcept;

        // The chip's clock for a delay time.
        static double clockFor (double seconds) noexcept;
        // ECHO and INTENSITY: 50k linear pots, loaded by the 47k and 22k after them.
        static double levelGain (double knob) noexcept;
        static double feedbackGain (double knob) noexcept;
        // The chip's soft limit (volts in, volts out).
        static double chip (double volts) noexcept;

        static constexpr double filterHz = 3200.0;

    private:
        struct Biquad
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
            void setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double rate, double matchHz) noexcept;
            double process (double x) noexcept
            {
                const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return y;
            }
            void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
        };

        double rate = 192000.0;
        juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };

        // In: 3rd order (a pole and a pair); out: 5th order (a pole and two pairs).
        Biquad preEmphasis, deEmphasis, chipCoupling, feedbackCoupling;
        Biquad antiAlias[2], reconstruction[3];

        double rectifierCoefficient = 0.0, compressorLevel = 0.0, expanderLevel = 0.0, fedBack = 0.0, lastCompressed = 0.0;

        std::vector<double> memory = std::vector<double> (2048, 0.0);   // the chip: 4096 stages, a sample per two
        int writeIndex = 0;
        double clock = 6826.0, clockTarget = 6826.0, clockGlide = 0.0, phase = 0.0, held = 0.0;
        double feedbackAmount = 0.0;
        bool clockSettled = false;
    };
}
