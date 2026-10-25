#include "Modulation.h"

namespace effects
{
    namespace
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        constexpr float maxDelayMs = 40.0f;

        // A one-pole low-pass coefficient for a cutoff, and a smoothing
        // coefficient for a time constant.
        float onePole (double sampleRate, float hz) { return 1.0f - std::exp (-twoPi * hz / (float) sampleRate); }
        float smoothing (double sampleRate, float seconds) { return 1.0f - std::exp (-1.0f / (seconds * (float) sampleRate)); }

        // A triangle wave from a 0-1 phase, -1 to 1: the Dimension's LFO shape.
        float triangle (float phase) { phase -= std::floor (phase); return 4.0f * std::abs (phase - 0.5f) - 1.0f; }

        // The Vortex, modelled on the schematic of a late-'80s Japanese
        // compact stereo chorus (a 1024-stage bucket-brigade delay chip and its
        // clock driver). Values marked "from the circuit" are worked out from
        // the schematic's parts; "estimated" ones aren't printed on it and are
        // set by ear and by typical designs of the time.
        namespace vortex
        {
            // Pre-emphasis before the delay chip (and the exact opposite after
            // it): the input op-amp's 47k / 10k / 6.8 nF network lifts the
            // treble about 15 dB, from about 410 Hz up to 2.3 kHz (from the
            // circuit). Boosting treble before the noisy chip and cutting it
            // after keeps hiss down, and is a large part of this chorus's
            // clear, glassy sound.
            constexpr float emphasisZeroHz = 410.0f, emphasisPoleHz = 2340.0f;

            // That op-amp has two diodes across its feedback, so a hot signal
            // (its boosted treble first) is softly limited, as the pedal's
            // manual warns. The level at which it starts is estimated: a
            // normal level stays clean, a loud bright one rounds off.
            constexpr float diodeKnee = 2.2f;

            // The filters either side of the chip (three-pole, from the
            // circuit's three 10k resistors and capacitors), cutting at about
            // 9 kHz (estimated from their values): the chorus sound's top end.
            constexpr float filterHz = 9000.0f;

            // The delay. The chip's delay is its 1024 stages divided by twice
            // the clock rate, and the LFO sweeps the clock, not the delay, so
            // the delay sweeps unevenly: further on the long side than the
            // short. The centre delay and how far DEPTH swings the clock are
            // estimated: about 3.9 to 13.3 ms at full DEPTH (widened from
            // 3.4 to 9.1 ms after the owner found the first version weaker
            // than the pedal).
            constexpr float centreMs = 6.0f, maxClockSwing = 0.55f;

            // E.LEVEL: turned fully up, the chorus is louder than the guitar
            // (about +3.5 dB), as the pedal's knob goes past an even mix into
            // a warbling, almost vibrato sound. It matches the guitar at about
            // two thirds.
            constexpr float maxEffectLevel = 1.5f;

            // EQ: the pedal's tone knob, on the chorus sound only: a treble
            // shelf (corner and range estimated).
            constexpr float eqHz = 2000.0f;
        }

        // Carbonite, modelled on the Boss CE-2 (a 1024-stage bucket-brigade
        // chip, an MN3007, and its clock driver). It shares its design with
        // the Vortex's pedal, which came later; what's different is below.
        // "Measured" values come from players' measurements of real pedals,
        // "from the circuit" from published analyses of its schematic, and
        // "estimated" ones from neither.
        namespace carbonite
        {
            // The same pre-emphasis and de-emphasis as the Vortex: 10k and
            // 6.8 nF, about 15 dB of treble lift between 410 Hz and 2.3 kHz
            // (from the circuit).
            constexpr float emphasisZeroHz = 410.0f, emphasisPoleHz = 2340.0f;

            // The filters around the chip pass about 48 Hz to 6.6 kHz (from
            // the circuit): the top end stops well below the Vortex's 9 kHz,
            // which is much of why this chorus sounds warmer and darker.
            constexpr float lowCutHz = 48.0f, filterHz = 6600.0f;

            // How hard a loud signal can drive the chip before it rounds off
            // (estimated, as the Vortex's).
            constexpr float headroom = 2.2f;

            // The clock, measured on a real pedal: about 150 kHz with DEPTH
            // down (1024 stages / (2 x 150 kHz) = 3.4 ms); 128 to 185 kHz at
            // half DEPTH; 113 to 233 kHz at full DEPTH (4.5 down to 2.2 ms).
            // The swing is lopsided, further up than down, which this curve
            // follows. Pedals vary by tens of percent (the clock's capacitor
            // is a loose-tolerance part); this is one pedal's.
            constexpr float centreClockKHz = 150.0f, clockSwing = 0.4f, clockBend = 0.153f;
        }

        // Biquad coefficients (bilinear transform), written into an existing
        // filter so nothing is allocated.
        template <typename Biquad>
        void setFirstOrderShelf (Biquad& f, double sampleRate, float zeroHz, float poleHz)
        {
            // H(s) = (1 + s / wz) / (1 + s / wp)
            const auto k = 2.0f * (float) sampleRate;
            const auto wz = twoPi * zeroHz, wp = twoPi * poleHz;
            const auto norm = 1.0f + k / wp;
            f.b0 = (1.0f + k / wz) / norm;
            f.b1 = (1.0f - k / wz) / norm;
            f.b2 = 0.0f;
            f.a1 = (1.0f - k / wp) / norm;
            f.a2 = 0.0f;
        }

        template <typename Biquad>
        void setFirstOrderLowPass (Biquad& f, double sampleRate, float hz)
        {
            const auto c = 1.0f / std::tan (juce::MathConstants<float>::pi * hz / (float) sampleRate);
            f.b0 = f.b1 = 1.0f / (1.0f + c);
            f.b2 = 0.0f;
            f.a1 = (1.0f - c) / (1.0f + c);
            f.a2 = 0.0f;
        }

        template <typename Biquad>
        void setFirstOrderHighPass (Biquad& f, double sampleRate, float hz)
        {
            const auto c = 1.0f / std::tan (juce::MathConstants<float>::pi * hz / (float) sampleRate);
            f.b0 = c / (1.0f + c);
            f.b1 = -f.b0;
            f.b2 = 0.0f;
            f.a1 = (1.0f - c) / (1.0f + c);
            f.a2 = 0.0f;
        }

        template <typename Biquad>
        void setLowPass (Biquad& f, double sampleRate, float hz, float q)
        {
            const auto w = twoPi * hz / (float) sampleRate;
            const auto alpha = std::sin (w) / (2.0f * q), cosW = std::cos (w);
            const auto a0 = 1.0f + alpha;
            f.b0 = f.b2 = (1.0f - cosW) / 2.0f / a0;
            f.b1 = (1.0f - cosW) / a0;
            f.a1 = -2.0f * cosW / a0;
            f.a2 = (1.0f - alpha) / a0;
        }

        template <typename Biquad>
        void setHighShelf (Biquad& f, double sampleRate, float hz, float gainDb)
        {
            // RBJ cookbook high shelf, slope 1.
            const auto a = std::pow (10.0f, gainDb / 40.0f);
            const auto w = twoPi * hz / (float) sampleRate;
            const auto cosW = std::cos (w), alpha = std::sin (w) / 2.0f * std::sqrt (2.0f);
            const auto sqrtA = 2.0f * std::sqrt (a) * alpha;
            const auto a0 = (a + 1.0f) - (a - 1.0f) * cosW + sqrtA;
            f.b0 = a * ((a + 1.0f) + (a - 1.0f) * cosW + sqrtA) / a0;
            f.b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosW) / a0;
            f.b2 = a * ((a + 1.0f) + (a - 1.0f) * cosW - sqrtA) / a0;
            f.a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosW) / a0;
            f.a2 = ((a + 1.0f) - (a - 1.0f) * cosW - sqrtA) / a0;
        }
    }

    Modulation::Modulation() : delayLine (8192), hornDelay (2048), drumDelay (2048) {}

    void Modulation::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) maxChannels };
        const juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) maxBlockSize, 1 };

        delayLine.setMaximumDelayInSamples ((int) std::ceil (maxDelayMs * 0.001 * sampleRate) + 4);
        delayLine.prepare (spec);
        phaser.prepare (spec);
        phaser.setCentreFrequency (900.0f);
        phaser.setFeedback (0.5f);

        for (auto* rotor : { &hornDelay, &drumDelay })
        {
            rotor->setMaximumDelayInSamples ((int) std::ceil (0.004 * sampleRate) + 4);
            rotor->prepare (monoSpec);
        }
        rotaryCrossover.setCutoffFrequency (800.0f);   // a real Leslie's crossover frequency
        rotaryCrossover.prepare (monoSpec);
        harmonicCrossover.setCutoffFrequency (900.0f);
        harmonicCrossover.prepare (spec);

        depth.reset (sampleRate, 0.05);
        mix.reset (sampleRate, 0.05);
        lfoPhase = hornPhase = drumPhase = 0.0f;
        hornHz = 0.8f;
        drumHz = 0.7f;
        hornTone = lampGlow = 0.0f;
        std::fill (std::begin (dimensionTone), std::end (dimensionTone), 0.0f);
        for (auto& path : vortexPaths)
        {
            path = {};
            setFirstOrderShelf (path.emphasis, sampleRate, vortex::emphasisZeroHz, vortex::emphasisPoleHz);
            setFirstOrderShelf (path.deEmphasis, sampleRate, vortex::emphasisPoleHz, vortex::emphasisZeroHz);   // the exact inverse
            // Three-pole Butterworth either side of the chip: one pole, then a pole pair.
            setFirstOrderLowPass (path.antiAlias1, sampleRate, vortex::filterHz);
            setLowPass (path.antiAlias2, sampleRate, vortex::filterHz, 1.0f);
            path.reconstruct1.copyCoefficients (path.antiAlias1);
            path.reconstruct2.copyCoefficients (path.antiAlias2);
        }
        for (auto& path : carbonitePaths)
        {
            path = {};
            setFirstOrderHighPass (path.inputHighPass, sampleRate, carbonite::lowCutHz);
            setFirstOrderShelf (path.emphasis, sampleRate, carbonite::emphasisZeroHz, carbonite::emphasisPoleHz);
            setFirstOrderShelf (path.deEmphasis, sampleRate, carbonite::emphasisPoleHz, carbonite::emphasisZeroHz);
            setFirstOrderLowPass (path.antiAlias1, sampleRate, carbonite::filterHz);
            setLowPass (path.antiAlias2, sampleRate, carbonite::filterHz, 1.0f);
            path.reconstruct1.copyCoefficients (path.antiAlias1);
            path.reconstruct2.copyCoefficients (path.antiAlias2);
        }
        vortexEq = 1.0e9f;   // forces the EQ's coefficients to be worked out
        updateVortexEq (0.0f);
        for (auto& channel : vibeStages)
            for (auto& stage : channel)
                stage = {};
        bypass.prepare (sampleRate, maxBlockSize);
        aeon.prepare (sampleRate, maxBlockSize);
    }

    float Modulation::advanceLfo (float rateHz) noexcept
    {
        lfoPhase += rateHz / (float) sampleRate;
        lfoPhase -= std::floor (lfoPhase);
        return lfoPhase;
    }

    void Modulation::process (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            depth.setTargetValue (settings.depth);
            mix.setTargetValue (settings.mix);

            // Switched to AEON: start with its delay empty, not holding old echoes.
            if (settings.type != lastType && settings.type == Type::flanger)
                aeon.reset();
            lastType = settings.type;

            switch (settings.type)
            {
                case Type::chorus:
                    processVortex (wet, settings);
                    break;

                case Type::flanger:
                    aeon.process (wet, { settings.manual, settings.depth, settings.rateHz, settings.resonance });
                    depth.skip ((int) wet.getNumSamples());
                    mix.skip ((int) wet.getNumSamples());
                    break;

                case Type::phaser:
                {
                    phaser.setRate (settings.rateHz);
                    phaser.setDepth (settings.depth);
                    phaser.setMix (settings.mix);
                    juce::dsp::ProcessContextReplacing<float> context (wet);
                    phaser.process (context);
                    depth.skip ((int) wet.getNumSamples());
                    mix.skip ((int) wet.getNumSamples());
                    break;
                }

                case Type::tremolo:          processTremolo (wet, settings); break;
                case Type::dimension:        processDimension (wet, settings); break;
                case Type::uniVibe:          processUniVibe (wet, settings); break;
                case Type::rotary:           processRotary (wet, settings); break;
                case Type::vibrato:          processVibrato (wet, settings); break;
                case Type::harmonicTremolo:  processHarmonicTremolo (wet, settings); break;
                case Type::autoPan:          processAutoPan (wet, settings); break;
                case Type::doubler:          processDoubler (wet, settings); break;
                case Type::carbonite:        processCarbonite (wet, settings); break;
            }
        });
    }

    void Modulation::updateVortexEq (float eq) noexcept
    {
        if (std::abs (eq - vortexEq) < 1.0e-4f)
            return;
        vortexEq = eq;
        for (auto& path : vortexPaths)
            setHighShelf (path.eq, sampleRate, vortex::eqHz, eq * vortexEqRangeDb);
    }

    void Modulation::processVortex (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // The pedal's chorus path: pre-emphasis and its diode limiting, the
        // filter into the delay chip, the chip's swept delay, the filter out
        // of it, de-emphasis, then EQ. The dry guitar is never touched: the
        // chorus is mixed in on top of it at E.LEVEL (the MIX knob).
        //
        // MONO (the pedal's OUTPUT A alone): guitar plus chorus, on both sides.
        // STEREO (A and B): the chorus alone on the left and the guitar alone
        // on the right, so the chorus happens between the speakers. As on the
        // pedal, turning E.LEVEL down moves the sound towards the dry side.
        updateVortexEq (settings.eq);
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto numChannels = (int) block.getNumChannels();
        const auto stereo = settings.stereoOutputs && numChannels > 1;

        const auto chorusOf = [&] (int ch, float input, float delaySamples)
        {
            auto& path = vortexPaths[ch];
            auto x = path.emphasis.process (input);
            x = vortex::diodeKnee * std::tanh (x / vortex::diodeKnee);
            x = path.antiAlias2.process (path.antiAlias1.process (x));
            delayLine.pushSample (ch, x);
            x = delayLine.popSample (ch, delaySamples);
            x = path.reconstruct2.process (path.reconstruct1.process (x));
            return path.eq.process (path.deEmphasis.process (x));
        };

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            // One chip and one triangle LFO: both sides sweep together.
            const auto lfo = triangle (advanceLfo (settings.rateHz));
            const auto clock = 1.0f + vortex::maxClockSwing * depth.getNextValue() * lfo;
            const auto delaySamples = vortex::centreMs / clock * msToSamples;
            const auto level = vortex::maxEffectLevel * mix.getNextValue();

            if (stereo)
            {
                // The pedal has one input: feed it both sides (normally the same).
                const auto input = 0.5f * (block.getSample (0, (int) i) + block.getSample (1, (int) i));
                block.setSample (0, (int) i, level * chorusOf (0, input, delaySamples));
                block.setSample (1, (int) i, input);
                continue;
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = block.getChannelPointer ((size_t) ch);
                samples[i] += level * chorusOf (ch, samples[i], delaySamples);
            }
        }
    }

    float Modulation::carboniteDelayMs (float depth, float lfo) noexcept
    {
        const auto clockKHz = carbonite::centreClockKHz * (1.0f + depth * (carbonite::clockSwing * lfo + carbonite::clockBend * lfo * lfo));
        return 1024.0f / (2.0f * clockKHz);
    }

    void Modulation::processCarbonite (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // The pedal: the guitar, plus the chorus path (a low cut, pre-emphasis,
        // the filter into the chip, the chip's swept delay, the filter out of
        // it, de-emphasis), mixed 50/50 at full level. Only RATE and DEPTH;
        // one input and one output, so each side runs through its own copy.
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto numChannels = (int) block.getNumChannels();

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto lfo = triangle (advanceLfo (settings.rateHz));
            const auto delaySamples = carboniteDelayMs (depth.getNextValue(), lfo) * msToSamples;
            mix.skip (1);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& path = carbonitePaths[ch];
                auto* samples = block.getChannelPointer ((size_t) ch);
                auto x = path.emphasis.process (path.inputHighPass.process (samples[i]));
                x = carbonite::headroom * std::tanh (x / carbonite::headroom);
                x = path.antiAlias2.process (path.antiAlias1.process (x));
                delayLine.pushSample (ch, x);
                x = delayLine.popSample (ch, delaySamples);
                x = path.reconstruct2.process (path.reconstruct1.process (x));
                samples[i] += path.deEmphasis.process (x);
            }
        }
    }

    void Modulation::processTremolo (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto lfo = 0.5f + 0.5f * std::sin (twoPi * advanceLfo (settings.rateHz));
            const auto gain = 1.0f - depth.getNextValue() * lfo;
            mix.skip (1);

            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                block.getChannelPointer (ch)[i] *= gain;
        }
    }

    void Modulation::processDimension (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Two chorus voices, swept by triangle LFOs in opposite directions
        // (one rises while the other falls), so their pitch changes largely
        // cancel: lush and wide without the seasick wobble of a single
        // chorus. Each side hears mostly one voice, with a little of the
        // other inverted, as the Dimension D's outputs do. The wet path is
        // darkened like the bucket-brigade delay chips of the originals.
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto numChannels = (int) block.getNumChannels();
        const auto toneCoefficient = onePole (sampleRate, 6500.0f);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto phase = advanceLfo (settings.rateHz);
            const auto depthNow = depth.getNextValue();
            const auto mixNow = mix.getNextValue();
            const auto sweepMs = 0.6f + 2.4f * depthNow;
            const auto delayA = (7.5f + sweepMs * triangle (phase)) * msToSamples;
            const auto delayB = (7.5f + sweepMs * triangle (phase + 0.5f)) * msToSamples;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = block.getChannelPointer ((size_t) ch);
                const auto dry = samples[i];
                delayLine.pushSample (ch, dry);
                const auto voiceA = delayLine.popSample (ch, delayA, false);
                const auto voiceB = delayLine.popSample (ch, delayB, true);

                // Left: voice A with a little of B inverted; right: the other way round.
                auto wet = numChannels == 1 ? 0.5f * (voiceA + voiceB)
                         : ch == 0           ? voiceA - 0.3f * voiceB
                                             : voiceB - 0.3f * voiceA;
                dimensionTone[ch] += (wet - dimensionTone[ch]) * toneCoefficient;
                wet = dimensionTone[ch];

                samples[i] = dry * (1.0f - mixNow) + wet * mixNow;
            }
        }
    }

    void Modulation::processUniVibe (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Four first-order all-pass stages tuned far apart (the original's
        // stages use very different capacitors), all swept together by a
        // lamp. The lamp is what gives the Vibe its throb: it brightens fast
        // and dims slowly, and spends longer dark than bright, so the sweep
        // is lopsided rather than a smooth sine. Wet alone is the original's
        // VIBRATO setting; mixed half and half with the dry signal it's the
        // CHORUS setting, where the phase shifts become moving notches.
        constexpr float stageHz[4] = { 70.0f, 260.0f, 950.0f, 3400.0f };
        const auto numChannels = (int) block.getNumChannels();
        const auto glowUp = smoothing (sampleRate, 0.004f), glowDown = smoothing (sampleRate, 0.03f);
        const auto nyquistGuard = (float) sampleRate * 0.45f;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto depthNow = depth.getNextValue();
            const auto mixNow = mix.getNextValue();

            const auto drive = std::pow (0.5f + 0.5f * std::sin (twoPi * advanceLfo (settings.rateHz)), 1.7f);
            lampGlow += (drive - lampGlow) * (drive > lampGlow ? glowUp : glowDown);
            const auto sweep = std::exp2 (3.0f * depthNow * lampGlow);   // up to three octaves

            float coefficients[4];
            for (int stage = 0; stage < 4; ++stage)
            {
                const auto t = std::tan (juce::MathConstants<float>::pi * juce::jmin (stageHz[stage] * sweep, nyquistGuard) / (float) sampleRate);
                coefficients[stage] = (t - 1.0f) / (t + 1.0f);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = block.getChannelPointer ((size_t) ch);
                const auto dry = samples[i];
                auto wet = dry;
                for (int stage = 0; stage < 4; ++stage)
                {
                    auto& state = vibeStages[ch][stage];
                    const auto out = coefficients[stage] * wet + state.x1 - coefficients[stage] * state.y1;
                    state.x1 = wet;
                    state.y1 = out;
                    wet = out;
                }
                samples[i] = dry * (1.0f - mixNow) + wet * mixNow;
            }
        }
    }

    void Modulation::processRotary (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // A Leslie speaker: the signal splits at 800 Hz into a treble horn and
        // a bass drum, each spinning. As each turns towards and away from the
        // two microphones its pitch shifts (Doppler: a delay that sweeps) and
        // its level rises and falls. RATE sets the horn's speed; the drum
        // turns a little slower. Both have inertia, the heavy drum far more
        // than the horn, so changing RATE gives the classic ramp: the horn
        // spins up first and the drum catches up seconds later.
        const auto numChannels = (int) block.getNumChannels();
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto hornInertia = smoothing (sampleRate, 0.7f), drumInertia = smoothing (sampleRate, 3.0f);
        const auto hornToneCoefficient = onePole (sampleRate, 5500.0f);
        constexpr float micOffset = 0.3f;   // the right mic sits a third of a turn round from the left

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto intensity = 0.35f + 0.65f * depth.getNextValue();
            const auto mixNow = mix.getNextValue();

            hornHz += (settings.rateHz - hornHz) * hornInertia;
            drumHz += (settings.rateHz * 0.86f - drumHz) * drumInertia;
            hornPhase += hornHz / (float) sampleRate;
            hornPhase -= std::floor (hornPhase);
            drumPhase += drumHz / (float) sampleRate;
            drumPhase -= std::floor (drumPhase);

            // Both channels carry the same signal at this point in the chain.
            float input = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                input += block.getChannelPointer ((size_t) ch)[i];
            input /= (float) numChannels;

            float bass = 0.0f, treble = 0.0f;
            rotaryCrossover.processSample (0, input, bass, treble);
            hornTone += (treble - hornTone) * hornToneCoefficient;   // the horn's own mellow top end
            hornDelay.pushSample (0, hornTone);
            drumDelay.pushSample (0, bass);

            float out[maxChannels] {};
            for (int ch = 0; ch < juce::jmin (numChannels, maxChannels); ++ch)
            {
                const auto hornAngle = twoPi * (hornPhase + micOffset * (float) ch);
                const auto drumAngle = twoPi * (drumPhase + micOffset * (float) ch);
                const auto last = ch == numChannels - 1;

                const auto hornSample = hornDelay.popSample (0, (1.2f + 0.35f * intensity * std::sin (hornAngle)) * msToSamples, last);
                const auto drumSample = drumDelay.popSample (0, (1.2f + 0.15f * intensity * std::sin (drumAngle)) * msToSamples, last);

                const auto hornLevel = 1.0f - 0.55f * intensity * (0.5f - 0.5f * std::cos (hornAngle));
                const auto drumLevel = 1.0f - 0.3f * intensity * (0.5f - 0.5f * std::cos (drumAngle));
                out[ch] = hornSample * hornLevel + drumSample * drumLevel;
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& sample = block.getChannelPointer ((size_t) ch)[i];
                sample = sample * (1.0f - mixNow) + out[juce::jmin (ch, maxChannels - 1)] * mixNow;
            }
        }
    }

    void Modulation::processVibrato (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Only the delayed copy, with its delay swept: the pitch itself
        // wavers, with no dry signal to turn it into a chorus.
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto numChannels = (int) block.getNumChannels();

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto lfo = std::sin (twoPi * advanceLfo (settings.rateHz));
            const auto delayMs = 5.0f + 3.5f * depth.getNextValue() * lfo;
            mix.skip (1);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* samples = block.getChannelPointer ((size_t) ch);
                delayLine.pushSample (ch, samples[i]);
                samples[i] = delayLine.popSample (ch, delayMs * msToSamples);
            }
        }
    }

    void Modulation::processHarmonicTremolo (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Splits the sound at 900 Hz and pulses the lows and highs in turn,
        // as the "brownface" Fender amps of the early '60s did: a softer,
        // swirling tremolo with a hint of phaser, rather than an on-off chop.
        const auto numChannels = (int) block.getNumChannels();
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto lfo = 0.5f + 0.5f * std::sin (twoPi * advanceLfo (settings.rateHz));
            const auto depthNow = depth.getNextValue();
            mix.skip (1);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& sample = block.getChannelPointer ((size_t) ch)[i];
                float low = 0.0f, high = 0.0f;
                harmonicCrossover.processSample (ch, sample, low, high);
                sample = low * (1.0f - depthNow * lfo) + high * (1.0f - depthNow * (1.0f - lfo));
            }
        }
    }

    void Modulation::processAutoPan (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Sweeps the sound from side to side. Equal-power panning keeps the
        // loudness steady as it moves; at full depth it reaches each side.
        if (block.getNumChannels() < 2)
        {
            depth.skip ((int) block.getNumSamples());
            mix.skip ((int) block.getNumSamples());
            return;   // a mono track can't pan
        }

        auto* left = block.getChannelPointer (0);
        auto* right = block.getChannelPointer (1);
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto lfo = std::sin (twoPi * advanceLfo (settings.rateHz));
            const auto angle = juce::MathConstants<float>::halfPi * 0.5f * (1.0f + depth.getNextValue() * lfo);
            mix.skip (1);
            left[i] *= juce::MathConstants<float>::sqrt2 * std::cos (angle);
            right[i] *= juce::MathConstants<float>::sqrt2 * std::sin (angle);
        }
    }

    void Modulation::processDoubler (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        // Automatic double tracking: sounds like the part was played three
        // times, the original in the middle and a "take" each side. Each
        // take is the guitar a little late (19 and 27 ms, so they don't line
        // up into a chorus), with its timing and pitch drifting slowly and
        // independently, as no two takes are ever played exactly alike.
        // DEPTH is how loose the takes are; RATE how quickly they drift; MIX
        // how loud they are against the original.
        const auto msToSamples = (float) sampleRate * 0.001f;
        const auto numChannels = (int) block.getNumChannels();
        constexpr float takeMs[2] = { 19.0f, 27.0f };

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            // A slow wander for each take: two sines at unrelated speeds, so it never quite repeats.
            const auto phase = advanceLfo (settings.rateHz * 0.25f);
            const auto depthNow = depth.getNextValue();
            const auto mixNow = mix.getNextValue();
            const auto wanderMs = 0.15f + 1.2f * depthNow;

            const auto dry = block.getChannelPointer (0)[i];
            delayLine.pushSample (0, dry);

            float takes[2];
            for (int take = 0; take < 2; ++take)
            {
                const auto offset = 0.37f * (float) take;
                const auto wander = 0.6f * std::sin (twoPi * (phase + offset)) + 0.4f * std::sin (twoPi * (2.71f * phase + 0.13f + offset));
                takes[take] = delayLine.popSample (0, (takeMs[take] + wanderMs * wander) * msToSamples, take == 1);
            }

            // The original stays in the middle at full level; the takes come up beside it.
            if (numChannels == 1)
            {
                block.getChannelPointer (0)[i] = dry + 0.5f * mixNow * (takes[0] + takes[1]);
            }
            else
            {
                const auto original = block.getChannelPointer (1)[i];   // same as the left: the chain is mono up to here
                block.getChannelPointer (0)[i] = dry + mixNow * takes[0];
                block.getChannelPointer (1)[i] = original + mixNow * takes[1];
            }
        }
    }
}
