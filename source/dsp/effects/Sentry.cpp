#include "Sentry.h"

namespace effects
{
    void Sentry::prepare (double newSampleRate, int maxBlockSize, int maxKeyDelay)
    {
        sampleRate = newSampleRate;
        keyLine.assign ((size_t) juce::jmax (1, maxKeyDelay + maxBlockSize + 1), 0.0f);
        keyBlock.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
        envelopeFall = (float) std::exp (-1.0 / (0.010 * sampleRate));
        keyDelay = juce::jlimit (0, maxKeyDelay, keyDelay);
        reset();
    }

    void Sentry::reset()
    {
        std::fill (keyLine.begin(), keyLine.end(), 0.0f);
        std::fill (keyBlock.begin(), keyBlock.end(), 0.0f);
        keyWrite = 0;
        envelope = 0.0f;
        gain = 1.0f;
        open = true;
        holdLeft = 0;
    }

    void Sentry::setKeyDelay (int samples) noexcept
    {
        keyDelay = juce::jlimit (0, (int) keyLine.size() - 1, samples);
    }

    void Sentry::pushKey (const float* clean, int numSamples) noexcept
    {
        const auto size = (int) keyLine.size();
        numSamples = juce::jmin (numSamples, (int) keyBlock.size());
        for (int i = 0; i < numSamples; ++i)
        {
            keyLine[(size_t) keyWrite] = clean[i];
            auto read = keyWrite - keyDelay;
            if (read < 0)
                read += size;
            keyBlock[(size_t) i] = keyLine[(size_t) read];
            keyWrite = keyWrite + 1 < size ? keyWrite + 1 : 0;
        }
    }

    void Sentry::process (float* samples, int numSamples, const Settings& settings) noexcept
    {
        // Off: open again (quickly but smoothly, so switching it off doesn't click).
        const auto openStep = 1.0f / (float) juce::jmax (1.0, openSeconds * sampleRate);
        if (! settings.on)
        {
            open = true;
            holdLeft = 0;
            for (int i = 0; i < numSamples; ++i)
            {
                gain = juce::jmin (1.0f, gain + openStep);
                samples[i] *= gain;
            }
            return;
        }

        const auto openAt = juce::Decibels::decibelsToGain (settings.thresholdDb);
        const auto closeAt = juce::Decibels::decibelsToGain (settings.thresholdDb - hysteresisDb);
        const auto holdSamples = (int) std::lround (juce::jmax (0.0f, settings.holdMs) * 0.001 * sampleRate);
        // RELEASE fades from 0 dB to closedDb in a straight line in dB (a
        // natural-sounding fade, like a sound dying away), then to silence.
        const auto releaseSamples = juce::jmax (1.0, (double) settings.releaseMs * 0.001 * sampleRate);
        const auto fallPerSample = (float) std::pow (10.0, (double) closedDb / 20.0 / releaseSamples);
        const auto floorGain = juce::Decibels::decibelsToGain (closedDb);
        const auto* key = settings.key == Key::clean ? keyBlock.data() : samples;
        numSamples = juce::jmin (numSamples, (int) keyBlock.size());

        for (int i = 0; i < numSamples; ++i)
        {
            const auto level = std::abs (key[i]);
            envelope = juce::jmax (level, envelope * envelopeFall);

            if (envelope >= openAt)
            {
                open = true;
                holdLeft = holdSamples;
            }
            else if (open && envelope < closeAt)
            {
                if (holdLeft > 0)
                    --holdLeft;
                else
                    open = false;
            }

            if (open)
                gain = juce::jmin (1.0f, gain + openStep);
            else if (gain > floorGain)
                gain *= fallPerSample;
            else
                gain = 0.0f;

            samples[i] *= gain;
        }
    }
}
