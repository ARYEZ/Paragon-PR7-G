#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

// The cabinet slot: convolves the amp's output with a speaker cabinet
// impulse response (IR).
//
// JUCE's convolution engine loads IRs on its own background thread, resamples
// them to the host rate, and crossfades from the previous IR, so switching
// cabinets doesn't click. IR level matches the official NAM plugin: no
// trimming or normalising, just a fixed -18 dB. With no IR loaded the
// convolution is skipped and audio passes through untouched.
class CabStage
{
public:
    CabStage();

    // Message thread, while audio is stopped.
    void prepare (double sampleRate, int maxBlockSize);

    struct LoadResult
    {
        juce::String error;           // empty on success
        double lengthSeconds = 0.0;
        std::vector<float> responseDb {};   // the IR's frequency response (see below)
    };

    // Background thread. Reads an IR file and hands it to the convolution engine.
    LoadResult load (const juce::File& file);

    // Any thread except the audio thread: back to pass-through.
    void clear() noexcept { hasImpulseResponse = false; }

    // Audio thread. Processes in place.
    void process (float* samples, int numSamples, bool enabled) noexcept;

    static constexpr double maxIrSeconds = 10.0;

    // An IR's frequency response, for the FACEPLATE's curve: `responsePoints`
    // levels in dB, from `responseLowHz` to `responseHighHz` evenly spaced
    // on a log (octave) scale, each averaged over a sixth of an octave so a
    // cab's many fine ripples don't hide its shape. The loudest point is 0 dB.
    static constexpr int responsePoints = 128;
    static constexpr float responseLowHz = 40.0f, responseHighHz = 20000.0f;
    static std::vector<float> responseDb (const juce::AudioBuffer<float>& ir, double sampleRate);

private:
    juce::AudioFormatManager formats;
    juce::dsp::Convolution convolution;   // default: zero latency
    std::atomic<bool> hasImpulseResponse { false };
    juce::SmoothedValue<float> wetAmount;
    juce::AudioBuffer<float> dry;
    int maxBlockSize = 512;
};
