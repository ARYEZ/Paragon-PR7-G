#include "PitchBlock.h"

namespace effects
{
    namespace
    {
        constexpr double recentSeconds = 0.5;      // how much playing FREEZE can catch
        constexpr double capturedSeconds = 0.4;    // how much it holds
        constexpr double grainSeconds = 0.12;      // each piece of the pad, faded in and out
        constexpr int overlap = 4;                 // pieces sounding at once
    }

    void PitchBlock::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        for (auto* shifter : { &tuner, &downTwo, &downOne, &upOneShifter })
            shifter->prepare (sampleRate);
        tunedAmount.reset (sampleRate, 0.05);
        tuneRatio.reset (sampleRate, 0.05);
        tunedAmount.setCurrentAndTargetValue (0.0f);
        tuneRatio.setCurrentAndTargetValue (1.0f);
        for (auto* level : { &subTwoLevel, &subOneLevel, &dryLevel, &upLevel })
            level->reset (sampleRate, 0.05);
        octaverBypass.prepare (sampleRate, maxBlockSize);
        analogOctave.prepare (sampleRate, maxBlockSize);
        analogDirect.assign ((size_t) maxBlockSize, 0.0f);
        analogOne.assign ((size_t) maxBlockSize, 0.0f);
        analogTwo.assign ((size_t) maxBlockSize, 0.0f);

        recent.assign ((size_t) (recentSeconds * sampleRate), 0.0f);
        frozen.assign (recent.size(), 0.0f);
        recentWrite = frozenLength = spawnCountdown = 0;
        for (auto& grain : grains)
            grain.active = false;
        wasFrozen = false;
        freezeAmount = 0.0f;
    }

    void PitchBlock::process (juce::dsp::AudioBlock<float> monoBlock, const Settings& settings) noexcept
    {
        auto* samples = monoBlock.getChannelPointer (0);
        const auto numSamples = (int) monoBlock.getNumSamples();

        // TUNING: the whole guitar through the shifter, cross-faded with the
        // untouched guitar so going to or from 0 is smooth.
        const auto tune = juce::jlimit (-12.0f, 12.0f, std::round (settings.tuneSemitones));
        tunedAmount.setTargetValue (tune != 0.0f ? 1.0f : 0.0f);
        if (tune != 0.0f)
        {
            if (tunedAmount.getCurrentValue() <= 0.0f)
                tuner.reset();   // starting fresh, not from what it heard long ago
            tuneRatio.setTargetValue (std::exp2 (tune / 12.0f));
        }
        if (tunedAmount.isSmoothing() || tunedAmount.getCurrentValue() > 0.0f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto amount = tunedAmount.getNextValue();
                const auto shifted = tuner.processSample (samples[i], tuneRatio.getNextValue());
                samples[i] = samples[i] * (1.0f - amount) + shifted * amount;
            }
        }

        // OCTAVER: two down, one down, dry and one up, each at its own level.
        subTwoLevel.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.subTwo));
        subOneLevel.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.subOne));
        dryLevel.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.dry));
        upLevel.setTargetValue (juce::jlimit (0.0f, 1.0f, settings.upOne));
        if (settings.analog != wasAnalog)
        {
            // A different style: start it from silence rather than from what it last held.
            wasAnalog = settings.analog;
            analogOctave.reset();
            for (auto* shifter : { &downTwo, &downOne, &upOneShifter })
                shifter->reset();
        }
        octaverBypass.process (monoBlock, settings.octaverOn, [&] (juce::dsp::AudioBlock<float> block)
        {
            auto* x = block.getChannelPointer (0);
            if (settings.analog)
            {
                // ANALOG: the pedal's three voices, through its passive mixer.
                const auto count = (int) block.getNumSamples();
                analogOctave.process (x, analogDirect.data(), analogOne.data(), analogTwo.data(), count);
                for (int i = 0; i < count; ++i)
                {
                    upLevel.skip (1);
                    x[i] = AnalogOctave::mix (analogDirect[(size_t) i], analogOne[(size_t) i], analogTwo[(size_t) i],
                                              dryLevel.getNextValue(), subOneLevel.getNextValue(), subTwoLevel.getNextValue());
                }
                return;
            }
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const auto in = x[i];
                x[i] = in * dryLevel.getNextValue()
                     + downTwo.processSample (in, 0.25f) * subTwoLevel.getNextValue()
                     + downOne.processSample (in, 0.5f) * subOneLevel.getNextValue()
                     + upOneShifter.processSample (in, 2.0f) * upLevel.getNextValue();
            }
        });

        // FREEZE: keep the last moment of playing; catch it when switched on.
        if (settings.freezeOn && ! wasFrozen)
            captureFreeze();
        wasFrozen = settings.freezeOn;

        const auto fadeSeconds = settings.freezeOn ? settings.freezeAttack : settings.freezeRelease;
        const auto fadeStep = 1.0f / (juce::jmax (0.005f, fadeSeconds) * (float) sampleRate);
        const auto level = juce::jlimit (0.0f, 1.0f, settings.freezeLevel);
        for (int i = 0; i < numSamples; ++i)
        {
            recent[(size_t) recentWrite] = samples[i];
            recentWrite = (recentWrite + 1) % (int) recent.size();

            freezeAmount = settings.freezeOn ? juce::jmin (1.0f, freezeAmount + fadeStep) : juce::jmax (0.0f, freezeAmount - fadeStep);
            if (freezeAmount > 0.0f)
                samples[i] += freezeSample() * freezeAmount * level;
        }
        if (freezeAmount <= 0.0f)
            for (auto& grain : grains)
                grain.active = false;
    }

    void PitchBlock::captureFreeze() noexcept
    {
        // The last moment of playing, in order, oldest first.
        frozenLength = juce::jmin ((int) frozen.size(), (int) (capturedSeconds * sampleRate));
        for (int i = 0; i < frozenLength; ++i)
            frozen[(size_t) i] = recent[(size_t) ((recentWrite - frozenLength + i + (int) recent.size()) % (int) recent.size())];
        for (auto& grain : grains)
            grain.active = false;
        spawnCountdown = 0;
    }

    float PitchBlock::freezeSample() noexcept
    {
        // Overlapping pieces from random places in what was caught, each
        // faded in and out (a Hann window). Four at once add up to a steady
        // level, so the pad holds still rather than pulsing.
        const auto grainLength = juce::jmin (frozenLength, (int) (grainSeconds * sampleRate));
        if (grainLength < 64)
            return 0.0f;

        if (--spawnCountdown <= 0)
        {
            spawnCountdown = grainLength / overlap;
            for (auto& grain : grains)
            {
                if (grain.active)
                    continue;
                grain = { true, random.nextInt (juce::jmax (1, frozenLength - grainLength)), grainLength, 0 };
                break;
            }
        }

        auto sum = 0.0f;
        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;
            const auto phase = (float) grain.played / (float) grain.length;
            sum += frozen[(size_t) (grain.start + grain.played)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase));
            if (++grain.played >= grain.length)
                grain.active = false;
        }
        return sum * (2.0f / (float) overlap);   // Hann windows a quarter apart add up to twice one
    }
}
