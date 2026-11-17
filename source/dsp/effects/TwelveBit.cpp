#include "TwelveBit.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        constexpr double pi = 3.14159265358979;

        // Pre-emphasis (IC1b): 47k in, with 4.7 nF + 10k across it; 33k back.
        // De-emphasis (IC1a) is the same network turned round: 33k in, 47k
        // with 4.7 nF + 10k across it back.
        constexpr double emphasisFarads = 4.7e-9, emphasisSeriesOhms = 10.0e3, emphasisOhms = 47.0e3, emphasisBackOhms = 33.0e3;

        // The compander's own lift: 47k with 2.7 nF + 22k across it, into the
        // compressor (R28, C19, R29; and R33, C25, R31 for the feedback), and
        // the same in the expander's feedback (R42, C20, R41).
        constexpr double liftFarads = 2.7e-9, liftSeriesOhms = 22.0e3, liftOhms = 47.0e3;

        // The emitter followers' gain: 10k emitter resistors, about 0.4 mA each (26 mV / 0.4 mA = 65 ohm).
        constexpr double followerGain = 10.0e3 / (10.0e3 + 65.0);

        constexpr double rectifierSeconds = 10.0e3 * 0.68e-6;   // the NE570's 10k into C32 / C33
        constexpr double unityVolts = 1.0;                      // ours: the level the compander leaves alone
        constexpr double maxCompressorGain = 100.0;             // ours: 40 dB
        constexpr double converterVolts = 2.5;                  // ours: half the chip's 5 V
        constexpr double stepVolts = 2.0 * converterVolts / 4096.0;
        constexpr double rail = 3.5;                            // the op-amps on 9 V
        constexpr double potOhms = 50.0e3;
    }

    void TwelveBit::Biquad::setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double sampleRate, double matchHz) noexcept
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

    int TwelveBit::memoryFor (double seconds) noexcept
    {
        return seconds < 0.05 ? 1024 : seconds < 0.2 ? 4096 : 16384;
    }

    double TwelveBit::clockFor (double seconds) noexcept
    {
        const auto s = std::clamp (seconds, minSeconds, maxSeconds);
        return (double) memoryFor (s) / s;
    }

    double TwelveBit::levelGain (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        return k * emphasisBackOhms / (emphasisBackOhms + potOhms * k * (1.0 - k));
    }

    double TwelveBit::feedbackGain (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        return k * liftOhms / (liftOhms + potOhms * k * (1.0 - k));
    }

    void TwelveBit::prepare (double sampleRate, int maxBlockSize)
    {
        oversampling.initProcessing ((size_t) maxBlockSize);
        rate = sampleRate * (double) oversampling.getOversamplingFactor();

        // Couplings: C2 into R7 (3.4 Hz) at the input; C26 into R33 (3.4 Hz) on the way back.
        const auto couplingSeconds = 1.0e-6 * 47.0e3;
        inputCoupling.setAnalog (0.0, couplingSeconds, 0.0, 1.0, couplingSeconds, 0.0, rate, 10.0);
        feedbackCoupling = inputCoupling;

        // Pre-emphasis: (33k / 47k) (1 + s C (47k + 10k)) / (1 + s C 10k); de-emphasis the reverse.
        const auto slow = emphasisFarads * (emphasisOhms + emphasisSeriesOhms), fast = emphasisFarads * emphasisSeriesOhms;
        preEmphasis.setAnalog (emphasisBackOhms / emphasisOhms, emphasisBackOhms / emphasisOhms * slow, 0.0, 1.0, fast, 0.0, rate, 2000.0);
        deEmphasis.setAnalog (emphasisOhms / emphasisBackOhms, emphasisOhms / emphasisBackOhms * fast, 0.0, 1.0, slow, 0.0, rate, 2000.0);

        const auto liftSlow = liftFarads * (liftOhms + liftSeriesOhms), liftFast = liftFarads * liftSeriesOhms;
        compressorLift.setAnalog (1.0, liftSlow, 0.0, 1.0, liftFast, 0.0, rate, 2000.0);
        expanderCut.setAnalog (1.0, liftFast, 0.0, 1.0, liftSlow, 0.0, rate, 2000.0);

        // The Sallen-Key stages: 10k + 10k, one capacitor back to the
        // follower's emitter, one to ground. With a follower gain K just under
        // one: K / (1 + s (2 R Cg + R Cf (1 - K)) + s^2 R^2 Cf Cg).
        const auto sallenKey = [this] (Biquad& stage, double backFarads, double groundFarads)
        {
            const auto r = 10.0e3, k = followerGain;
            const auto hz = 1.0 / (2.0 * pi * r * std::sqrt (backFarads * groundFarads));
            stage.setAnalog (k, 0.0, 0.0, 1.0, 2.0 * r * groundFarads + r * backFarads * (1.0 - k), r * r * backFarads * groundFarads, rate, hz);
        };
        sallenKey (antiAlias[0], 0.0047e-6, 0.0027e-6);    // Q3: C9, C8
        sallenKey (antiAlias[1], 0.0068e-6, 470.0e-12);    // Q4: C15, C14
        sallenKey (antiAlias[2], 0.018e-6, 220.0e-12);     // Q6: C17, C16
        sallenKey (reconstruction[0], 0.0027e-6, 0.0012e-6);   // Q10: C38, C37
        sallenKey (reconstruction[1], 0.0068e-6, 150.0e-12);   // Q9: C35, C36

        rectifierCoefficient = 1.0 - std::exp (-1.0 / (rectifierSeconds * rate));
        clockGlide = 1.0 - std::exp (-1.0 / (0.03 * rate));   // ours: 30 ms, so a jump in TIME glides instead of clicking
        setTime (0.3);
        clock = clockTarget;
        reset();
    }

    void TwelveBit::reset() noexcept
    {
        oversampling.reset();
        for (auto* f : { &inputCoupling, &preEmphasis, &antiAlias[0], &antiAlias[1], &antiAlias[2], &compressorLift,
                         &reconstruction[0], &reconstruction[1], &expanderCut, &deEmphasis, &feedbackCoupling })
            f->reset();
        compressorLevel = expanderLevel = fedBack = lastCompressed = held = phase = 0.0;
        std::fill (memory.begin(), memory.end(), (std::int16_t) 0);
        writeIndex = 0;
        clock = clockTarget;
        clockSettled = false;
    }

    void TwelveBit::setTime (double seconds) noexcept
    {
        const auto s = std::clamp (seconds, minSeconds, maxSeconds);
        const auto newLength = memoryFor (s);
        clockTarget = clockFor (s);
        if (newLength != length || ! clockSettled)
        {
            clockSettled = true;
            // The MODE switch: a different length of memory, at once.
            length = newLength;
            clock = clockTarget;
        }
    }

    void TwelveBit::setFeedback (double knob) noexcept
    {
        feedbackKnob = feedbackGain (knob);
    }

    void TwelveBit::process (float* samples, int numSamples) noexcept
    {
        float* channels[] = { samples };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);
        auto up = oversampling.processSamplesUp (block);
        auto* x = up.getChannelPointer (0);
        const auto count = (int) up.getNumSamples();
        const auto mask = (int) memory.size() - 1;

        for (int i = 0; i < count; ++i)
        {
            // The way in: emphasis, the 7 kHz filter, the feedback, the compressor.
            auto v = opAmpClip (preEmphasis.process (inputCoupling.process ((double) x[i])), rail);
            for (auto& stage : antiAlias)
                v = stage.process (v);
            const auto lifted = compressorLift.process (v + fedBack);
            const auto compressed = opAmpClip (lifted * unityVolts / std::max (compressorLevel, unityVolts / maxCompressorGain), rail);
            compressorLevel += rectifierCoefficient * (std::abs (compressed) - compressorLevel);

            // The clock: on each tick the converter takes the compressor's
            // output (where it was at the tick, between this sample and the
            // last) and the memory swaps it for the one from `length` ticks ago.
            clock += clockGlide * (clockTarget - clock);
            const auto step = clock / rate;
            phase += step;
            auto converterOut = held;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                const auto late = std::min (phase / step, 1.0);   // how far back in this sample the tick was
                const auto taken = compressed - late * (compressed - lastCompressed);
                const auto code = (int) std::floor (std::clamp (taken, -converterVolts, converterVolts - stepVolts) / stepVolts + 0.5);
                const auto old = memory[(size_t) ((writeIndex - length) & mask)];
                memory[(size_t) writeIndex] = (std::int16_t) std::clamp (code, -2048, 2047);
                writeIndex = (writeIndex + 1) & mask;
                const auto next = (double) old * stepVolts;
                converterOut = held * (1.0 - late) + next * late;   // the step lands part-way through this sample
                held = next;
            }
            lastCompressed = compressed;

            // The way out: the held output, filtered, expanded; then F.BACK and the de-emphasis.
            auto c = converterOut;
            for (auto& stage : reconstruction)
                c = stage.process (c);
            expanderLevel += rectifierCoefficient * (std::abs (c) - expanderLevel);
            const auto expanded = c * std::max (expanderLevel, unityVolts / maxCompressorGain) / unityVolts;
            const auto delayed = opAmpClip (expanderCut.process (expanded), rail);
            fedBack = feedbackCoupling.process (delayed * feedbackKnob);
            x[i] = (float) opAmpClip (deEmphasis.process (delayed), rail);
        }

        oversampling.processSamplesDown (block);
        if (! std::isfinite (samples[0]))
            reset();
    }
}
