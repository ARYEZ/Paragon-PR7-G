#include "CabStage.h"

namespace
{
    constexpr double fadeSeconds = 0.02;

    // The official NAM plugin turns every IR down by 18 dB so a typical cab
    // IR lands at a sensible level:
    // https://github.com/sdatkinson/NeuralAmpModelerPlugin/issues/100#issuecomment-1455273839
    constexpr float irGainDb = -18.0f;
}

CabStage::CabStage()
{
    formats.registerBasicFormats();   // WAV, AIFF, FLAC and others
}

void CabStage::prepare (double sampleRate, int newMaxBlockSize)
{
    maxBlockSize = juce::jmax (1, newMaxBlockSize);
    convolution.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    wetAmount.reset (sampleRate, fadeSeconds);
    dry.setSize (1, maxBlockSize);
}

CabStage::LoadResult CabStage::load (const juce::File& file)
{
    if (! file.existsAsFile())
        return { "File not found." };

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return { "Not an audio file this plugin can read (use WAV, AIFF or FLAC)." };

    if (reader->lengthInSamples <= 0)
        return { "The file is empty." };
    if ((double) reader->lengthInSamples > maxIrSeconds * reader->sampleRate)
        return { "That's too long for a cabinet IR (over 10 seconds)." };

    // Cabinet IRs are mono. For a stereo file, use the left channel.
    juce::AudioBuffer<float> ir (1, (int) reader->lengthInSamples);
    reader->read (&ir, 0, ir.getNumSamples(), 0, true, false);
    ir.applyGain (juce::Decibels::decibelsToGain (irGainDb));
    auto response = responseDb (ir, reader->sampleRate);

    // Normalise::no also makes JUCE scale the IR by (IR rate / host rate),
    // which keeps the level the same at any host sample rate.
    convolution.loadImpulseResponse (std::move (ir), reader->sampleRate,
                                     juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::no);
    hasImpulseResponse = true;
    return { {}, (double) reader->lengthInSamples / reader->sampleRate, std::move (response) };
}

std::vector<float> CabStage::responseDb (const juce::AudioBuffer<float>& ir, double sampleRate)
{
    // One FFT of the IR (its first 2^15 samples at most: a cab's sound is
    // all in its first few hundred milliseconds), zero-padded to at least
    // 32768 points (about 1.5 Hz apart) so the low end is smooth, not steps.
    const auto length = juce::jmin (ir.getNumSamples(), 1 << 15);
    auto order = 15;
    while ((1 << order) < length)
        ++order;
    const auto size = 1 << order;
    std::vector<float> data ((size_t) size * 2, 0.0f);
    if (length > 0)
        std::copy_n (ir.getReadPointer (0), length, data.begin());
    juce::dsp::FFT (order).performFrequencyOnlyForwardTransform (data.data());

    std::vector<float> curve ((size_t) responsePoints);
    const auto binHz = sampleRate / (double) size;
    const auto lastBin = size / 2;
    const auto sixth = std::pow (2.0, 1.0 / 12.0);   // half of a sixth of an octave, either side
    for (int point = 0; point < responsePoints; ++point)
    {
        const auto hz = responseLowHz * std::pow ((double) responseHighHz / responseLowHz, (double) point / (responsePoints - 1));
        const auto from = juce::jlimit (1, lastBin, (int) std::floor (hz / sixth / binHz));
        const auto to = juce::jlimit (from, lastBin, (int) std::ceil (hz * sixth / binHz));
        double power = 0.0;
        for (int bin = from; bin <= to; ++bin)
            power += (double) data[(size_t) bin] * data[(size_t) bin];
        curve[(size_t) point] = (float) (10.0 * std::log10 (power / (to - from + 1) + 1.0e-20));
    }
    const auto loudest = *std::max_element (curve.begin(), curve.end());
    for (auto& level : curve)
        level -= loudest;
    return curve;
}

void CabStage::process (float* samples, int numSamples, bool enabled) noexcept
{
    wetAmount.setTargetValue (enabled && hasImpulseResponse ? 1.0f : 0.0f);

    if (! wetAmount.isSmoothing() && wetAmount.getTargetValue() < 0.5f)
    {
        // Off, or no IR: skip the convolution and leave the audio untouched.
        // A newly loaded IR still gets installed: JUCE does that inside
        // process(), so it happens as soon as the cab is switched back on.
        return;
    }

    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const auto count = juce::jmin (maxBlockSize, numSamples - start);
        auto* chunk = samples + start;
        auto* drySamples = dry.getWritePointer (0);
        std::copy (chunk, chunk + count, drySamples);

        float* channels[] = { chunk };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) count);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

        for (int i = 0; i < count; ++i)
        {
            const auto mix = wetAmount.getNextValue();
            chunk[i] = drySamples[i] + (chunk[i] - drySamples[i]) * mix;
        }
    }
}
