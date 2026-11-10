#pragma once

#include "EffectUtils.h"

#include <array>
#include <memory>
#include <vector>

namespace effects
{
    // CIPHER's spectral effects (stage 4), one per slot: the sound cut into
    // overlapping frames (a Hann window, four frames to a window), each
    // turned into frequencies (FFT), changed, and turned back. The manual
    // names fourteen; every one's workings are our design to do what it
    // says. A frame's frequencies are tracked between frames (a phase
    // vocoder), so moved or held frequencies stay smooth.
    //
    // Latency: the frame size (512 to 4096 samples, the slot's FFT SIZE),
    // which CIPHER adds up for its slots and reports to the host.
    class CipherSpectral
    {
    public:
        enum class Effect { shift, rotate, twist, mirror, bandpass, stretch, scatter, cascade, smear, reverb, compress, corrupt, lofi, spread };
        static constexpr int numSizes = 4;
        static int sizeFor (int index) noexcept { return 512 << juce::jlimit (0, numSizes - 1, index); }   // 512, 1024, 2048, 4096

        struct Controls
        {
            Effect effect = Effect::bandpass;
            int size = 2048;
            float main[2] { 0.5f, 0.5f };   // per side (STEREO moves them apart)
            float p1 = 0.5f, p2 = 0.5f;
            double samplesPerBeat = 24000.0;
            double songBeats = -1.0;        // at this call's first sample; -1 when not playing
        };

        CipherSpectral();
        ~CipherSpectral();

        void prepare (double sampleRate);
        void reset();

        // Runs `count` samples of both sides in place; `dryOut` gets the input
        // delayed by the latency (for the slot's MIX).
        void process (float* const* chans, float* const* dryOut, int numChannels, int count, const Controls&);

        int getLatency() const noexcept { return size; }

    private:
        struct Channel;
        std::array<std::unique_ptr<Channel>, 2> channels;
        std::array<std::unique_ptr<juce::dsp::FFT>, numSizes> ffts;
        double sampleRate = 48000.0;
        int size = 2048, hop = 512, historyFrames = 0, frameCount = 0;
        long long lastGrid = -1;
        double freeCounter = 0.0;
        juce::Random random { 4242 };

        void setSize (int newSize);
        void processFrame (int c, const Controls&, bool capture);
    };
}
