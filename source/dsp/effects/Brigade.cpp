#include "Brigade.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        constexpr double pi = 3.14159265358979;

        // Pre-emphasis (IC1a): 47k in, with 6.8 nF + 10k across it; 47k back.
        // De-emphasis (IC1b) is the same network turned round.
        constexpr double emphasisFarads = 6.8e-9, emphasisSeriesOhms = 10.0e3, emphasisOhms = 47.0e3;

        constexpr double stages = 4096.0;
        constexpr double rectifierSeconds = 10.0e3 * 0.22e-6;   // NE570's 10k into C10 / C12
        constexpr double unityVolts = 1.0;                      // ours
        constexpr double maxCompressorGain = 100.0;             // ours: 40 dB
        constexpr double rail = 3.5;
        constexpr double potOhms = 50.0e3;

        // The chip: 1% distortion (MN3005, per Hobby-Hour) at 0 dBm (0.775 V
        // rms), the service notes' test level. h tanh (x / h) has a third
        // harmonic x^2 / (12 h^2): h = 0.775 sqrt 2 / sqrt (12 x 0.01).
        constexpr double chipHeadroom = 0.775 * 1.41421356 / 0.34641016;

        // INTENSITY at full feeds back exactly what came out (ours: the edge of
        // self-oscillation, where a real DM-2 is said to sit).
        constexpr double fullFeedback = 1.0;
    }

    void Brigade::Biquad::setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double sampleRate, double matchHz) noexcept
    {
        const auto w = 2.0 * pi * matchHz;
        const auto k = w / std::tan (w / (2.0 * sampleRate)), k2 = k * k;
        const auto a0 = na0 + na1 * k + na2 * k2;
        b0 = (nb0 + nb1 * k + nb2 * k2) / a0;
        b1 = (2.0 * nb0 - 2.0 * nb2 * k2) / a0;
        b2 = (nb0 - nb1 * k + nb2 * k2) / a0;
        a1 = (2.0 * na0 - 2.0 * na2 * k2) / a0;
        a2 = (na0 - na1 * k + na2 * k2) / a0;
    }

    double Brigade::clockFor (double seconds) noexcept
    {
        return stages / (2.0 * std::clamp (seconds, minSeconds, maxSeconds));
    }

    double Brigade::levelGain (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        return k * 47.0e3 / (47.0e3 + potOhms * k * (1.0 - k));
    }

    double Brigade::feedbackGain (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        return fullFeedback * k * 22.0e3 / (22.0e3 + potOhms * k * (1.0 - k));
    }

    double Brigade::chip (double volts) noexcept
    {
        return chipHeadroom * std::tanh (volts / chipHeadroom);
    }

    void Brigade::prepare (double sampleRate, int maxBlockSize)
    {
        oversampling.initProcessing ((size_t) maxBlockSize);
        rate = sampleRate * (double) oversampling.getOversamplingFactor();

        const auto slow = emphasisFarads * (emphasisOhms + emphasisSeriesOhms), fast = emphasisFarads * emphasisSeriesOhms;
        preEmphasis.setAnalog (1.0, slow, 0.0, 1.0, fast, 0.0, rate, 1000.0);
        deEmphasis.setAnalog (1.0, fast, 0.0, 1.0, slow, 0.0, rate, 1000.0);

        // C15 1 uF into the chip's 10k // 100k bias (17 Hz); C8 0.1 uF into R13 22k on the way back (72 Hz).
        const auto chipSeconds = 1.0e-6 * (10.0e3 * 100.0e3 / 110.0e3), backSeconds = 0.1e-6 * 22.0e3;
        chipCoupling.setAnalog (0.0, chipSeconds, 0.0, 1.0, chipSeconds, 0.0, rate, 10.0);
        feedbackCoupling.setAnalog (0.0, backSeconds, 0.0, 1.0, backSeconds, 0.0, rate, 100.0);

        // Butterworth: 3rd order = a pole and a pair at Q 1; 5th = a pole and pairs at Q 0.618 and 1.618.
        const auto w = 1.0 / (2.0 * pi * filterHz);
        const auto pole = [&] (Biquad& b) { b.setAnalog (1.0, 0.0, 0.0, 1.0, w, 0.0, rate, filterHz); };
        const auto pair = [&] (Biquad& b, double q) { b.setAnalog (1.0, 0.0, 0.0, 1.0, w / q, w * w, rate, filterHz); };
        pole (antiAlias[0]);
        pair (antiAlias[1], 1.0);
        pole (reconstruction[0]);
        pair (reconstruction[1], 0.618034);
        pair (reconstruction[2], 1.618034);

        rectifierCoefficient = 1.0 - std::exp (-1.0 / (rectifierSeconds * rate));
        clockGlide = 1.0 - std::exp (-1.0 / (0.03 * rate));
        setTime (0.3);
        reset();
    }

    void Brigade::reset() noexcept
    {
        oversampling.reset();
        for (auto* f : { &preEmphasis, &deEmphasis, &chipCoupling, &feedbackCoupling, &antiAlias[0], &antiAlias[1],
                         &reconstruction[0], &reconstruction[1], &reconstruction[2] })
            f->reset();
        compressorLevel = expanderLevel = fedBack = lastCompressed = held = phase = 0.0;
        std::fill (memory.begin(), memory.end(), 0.0);
        writeIndex = 0;
        clock = clockTarget;
        clockSettled = false;
    }

    void Brigade::setTime (double seconds) noexcept
    {
        clockTarget = clockFor (seconds);
        if (! clockSettled)
        {
            clock = clockTarget;
            clockSettled = true;
        }
    }

    void Brigade::setFeedback (double knob) noexcept
    {
        feedbackAmount = feedbackGain (knob);
    }

    void Brigade::process (float* samples, int numSamples) noexcept
    {
        float* channels[] = { samples };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);
        auto up = oversampling.processSamplesUp (block);
        auto* x = up.getChannelPointer (0);
        const auto count = (int) up.getNumSamples();
        const auto length = (int) memory.size();

        for (int i = 0; i < count; ++i)
        {
            // The way in: pre-emphasis, plus what INTENSITY brings back, into the compressor.
            const auto in = opAmpClip (preEmphasis.process ((double) x[i]), rail) + fedBack;
            const auto compressed = opAmpClip (in * unityVolts / std::max (compressorLevel, unityVolts / maxCompressorGain), rail);
            compressorLevel += rectifierCoefficient * (std::abs (compressed) - compressorLevel);
            auto v = compressed;
            for (auto& stage : antiAlias)
                v = stage.process (v);
            v = chip (chipCoupling.process (v));

            // The chip: on each clock it takes the signal (where it was at the
            // tick) and hands out the one from 2048 clocks ago.
            clock += clockGlide * (clockTarget - clock);
            const auto step = clock / rate;
            phase += step;
            auto chipOut = held;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                const auto late = std::min (phase / step, 1.0);
                const auto taken = v - late * (v - lastCompressed);
                const auto old = memory[(size_t) writeIndex];
                memory[(size_t) writeIndex] = taken;
                writeIndex = (writeIndex + 1) % length;
                chipOut = held * (1.0 - late) + old * late;
                held = old;
            }
            lastCompressed = v;

            // The way out: the filter, the expander; then INTENSITY and the de-emphasis.
            auto c = chipOut;
            for (auto& stage : reconstruction)
                c = stage.process (c);
            expanderLevel += rectifierCoefficient * (std::abs (c) - expanderLevel);
            const auto delayed = opAmpClip (c * std::max (expanderLevel, unityVolts / maxCompressorGain) / unityVolts, rail);
            fedBack = feedbackCoupling.process (delayed * feedbackAmount);
            x[i] = (float) opAmpClip (deEmphasis.process (delayed), rail);
        }

        oversampling.processSamplesDown (block);
        if (! std::isfinite (samples[0]))
            reset();
    }
}
