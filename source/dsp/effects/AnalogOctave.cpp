#include "AnalogOctave.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        constexpr double pi = 3.14159265358979;
        constexpr double rail = 3.5;                                        // TL022s on 9 V
        constexpr double geSaturation = 2.0e-7, geThermal = 1.3 * 25.85e-3;   // D10, D11 (ours: a typical germanium diode)

        // The switch stage (IC5): 47k into each input, 27k back, 100k from (+)
        // to ground, the JFET across that. On: (+) at ground, -27/47. Off: (+)
        // at 100/147 of the input, so 100/147 x (1 + 27/47) - 27/47.
        constexpr double gainOn = -27.0 / 47.0;
        constexpr double gainOff = 100.0 / 147.0 * (1.0 + 27.0 / 47.0) - 27.0 / 47.0;
        // What the clamp's node sees: on, 47k to each (grounded) input; off,
        // 147k to ground and 47k to (-), which sits at 100/147 of the node.
        constexpr double loadOn = 1.0 / 47.0e3 + 1.0 / 47.0e3;
        constexpr double loadOff = 1.0 / 147.0e3 + (1.0 - 100.0 / 147.0) / 47.0e3;

        // One RC then a Sallen-Key pair, as each low-pass is drawn: in -> R1 ->
        // (C1 to ground) -> R2 -> (C2 to the output) -> R3 -> (C3 to ground) -> follower.
        void buildLowPass (Circuit& c, double r1, double c1, double r2, double c2, double r3, double c3, double rate, int tap) noexcept
        {
            c = {};
            const auto in = c.node(), a = c.node(), b = c.node(), plus = c.node(), out = c.node();
            c.resistor (in, a, r1);
            c.capacitor (a, 0, c1);
            c.resistor (a, b, r2);
            c.capacitor (b, out, c2);
            c.resistor (b, plus, r3);
            c.capacitor (plus, 0, c3);
            c.opAmp (plus, out, out);
            c.setInput (in);
            c.setOutput (out);
            if (tap > 0)
            {
                // The tracking: after the follower, R10 33k -> (C20 330 pF) -> R32 33k -> (C18 0.01 uF // R31 68k).
                // Tap 1: the comparators' node (after R10); tap 2: the peak followers' (after R32).
                const auto s1 = c.node(), s2 = c.node();
                c.resistor (out, s1, 33.0e3);
                c.capacitor (s1, 0, 330.0e-12);
                c.resistor (s1, s2, 33.0e3);
                c.capacitor (s2, 0, 0.01e-6);
                c.resistor (s2, 0, 68.0e3);
                c.setOutput (tap == 1 ? s1 : s2);
            }
            c.prepare (rate);
        }
    }

    float AnalogOctave::mix (float direct, float octaveOne, float octaveTwo, float directLevel, float oneLevel, float twoLevel) noexcept
    {
        // Each wiper (k of its voltage, k (1 - k) 100k behind it) through 100k into one node.
        const auto weight = [] (float k) { k = juce::jlimit (0.0f, 1.0f, k); return 1.0f / (1.0f + k * (1.0f - k)); };
        const auto wd = weight (directLevel), w1 = weight (oneLevel), w2 = weight (twoLevel);
        const auto node = (wd * directLevel * direct + w1 * oneLevel * octaveOne + w2 * twoLevel * octaveTwo) / (wd + w1 + w2);
        return node * 3.0f / (float) inputGain;   // DIRECT full alone: the guitar's own level
    }

    double AnalogOctave::Stage::process (double in, bool switchOn) noexcept
    {
        // The clamp: 1 uF from `in`, the diode (anode to ground) and the
        // stage's load on the node. Trapezoidal: the capacitor passes
        // gC (in - v) - history.
        const auto load = switchOn ? loadOn : loadOff;
        auto v = 2.0 * clampVolts - clampVoltsPrev;
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto e = std::exp (std::clamp (-v / geThermal, -40.0, 40.0));
            const auto f = gC * (in - v) - history + geSaturation * (e - 1.0) - load * v;
            const auto d = -gC - geSaturation / geThermal * e - load;
            const auto step = std::clamp (-f / d, -0.1, 0.1);
            v += step;
            if (std::abs (step) < 1.0e-7)
                break;
        }
        const auto amps = gC * (in - v) - history;
        history = gC * (in - v) + amps;
        clampVoltsPrev = clampVolts;
        clampVolts = v;
        const auto switched = opAmpClip (v * (switchOn ? gainOn : gainOff), rail);
        return lowPass.process (switched);
    }

    void AnalogOctave::prepare (double sampleRate, int maxBlockSize)
    {
        oversampling.initProcessing ((size_t) maxBlockSize);
        work.setSize (3, maxBlockSize);
        rate = sampleRate * (double) oversampling.getOversamplingFactor();
        inputG = std::tan (pi * (1.0 / (2.0 * pi * 2.7e3 * 10.0e-6)) / rate);

        buildLowPass (trackFilter, 22.0e3, 0.01e-6, 330.0e3, 2.2e-9, 330.0e3, 220.0e-12, rate, 1);
        buildLowPass (trackDetect, 22.0e3, 0.01e-6, 330.0e3, 2.2e-9, 330.0e3, 220.0e-12, rate, 2);   // the same network, read one node on
        buildLowPass (one.lowPass, 22.0e3, 0.022e-6, 330.0e3, 4.7e-9, 330.0e3, 470.0e-12, rate, 0);
        buildLowPass (two.lowPass, 22.0e3, 0.047e-6, 330.0e3, 0.01e-6, 330.0e3, 0.001e-6, rate, 0);
        one.gC = two.gC = 2.0 * 1.0e-6 * rate;

        chargeCoefficient = 1.0 - std::exp (-1.0 / (1.0e3 * 1.0e-6 * rate));      // R34 / R37 1k into C22 / C23 1 uF
        releaseCoefficient = 1.0 - std::exp (-1.0 / (11.0e3 * 1.0e-6 * rate));    // back out through R34 + R33 (11k)
        reset();
    }

    void AnalogOctave::reset() noexcept
    {
        oversampling.reset();
        trackFilter.reset();
        trackDetect.reset();
        one.reset();
        two.reset();
        inputIn = inputOut = peakUp = peakDown = 0.0;
        square = half = quarter = false;
    }

    void AnalogOctave::process (const float* input, float* direct, float* octaveOne, float* octaveTwo, int numSamples) noexcept
    {
        work.clear();
        work.copyFrom (0, 0, input, numSamples);
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 3, (size_t) numSamples);
        auto up = oversampling.processSamplesUp (block);
        auto* d = up.getChannelPointer (0);
        auto* o1 = up.getChannelPointer (1);
        auto* o2 = up.getChannelPointer (2);

        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            // IC1a: 4.7x above 5.9 Hz (C3 in series with R6), clipping at its rails.
            inputOut = (d[i] - inputIn) / (1.0 + inputG) + inputOut * (1.0 - inputG) / (1.0 + inputG);
            inputIn = d[i];
            const auto guitar = opAmpClip (inputOut * (inputGain - 1.0) + d[i], rail);

            // The tracking: the comparators watch the filtered note; the peak
            // followers (half-wave, from one node further on) set where they trip.
            const auto heard = opAmpClip (trackFilter.process (guitar), rail);
            const auto later = trackDetect.process (guitar);
            const auto upward = std::max (later, 0.0), downward = std::min (later, 0.0);
            peakUp += (upward - peakUp) * (upward > peakUp ? chargeCoefficient : releaseCoefficient);
            peakDown += (downward - peakDown) * (downward < peakDown ? chargeCoefficient : releaseCoefficient);
            const auto wasSquare = square;
            if (heard > peakUp && peakUp > 1.0e-4)
                square = false;    // IC2b clears the flip-flop
            else if (heard < peakDown && peakDown < -1.0e-4)
                square = true;     // IC3b sets it
            if (wasSquare && ! square)
            {
                const auto wasHalf = half;
                half = ! half;     // IC7b: divide by 2
                if (wasHalf && ! half)
                    quarter = ! quarter;   // IC6: divide by 2 again
            }

            d[i] = (float) guitar;
            const auto first = one.process (guitar, half);
            o1[i] = (float) first;
            o2[i] = (float) two.process (first, quarter);
        }

        oversampling.processSamplesDown (block);
        std::copy (work.getReadPointer (0), work.getReadPointer (0) + numSamples, direct);
        std::copy (work.getReadPointer (1), work.getReadPointer (1) + numSamples, octaveOne);
        std::copy (work.getReadPointer (2), work.getReadPointer (2) + numSamples, octaveTwo);
    }
}
