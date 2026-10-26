#pragma once

#include "Cosmonaut.h"
#include "EffectUtils.h"

namespace effects
{
    // Footswitch 2: an overdrive or fuzz pedal, before the amp, in four models.
    //
    // SCREAMER is modelled on the Ibanez TS808 Tube Screamer's circuit:
    //
    //   x --+--------------------------------------------(+)--> tone --> level
    //       |                                              ^
    //       +--> 720 Hz low cut -> gain -> diode clipping -+
    //
    // Its op-amp only amplifies above about 720 Hz, and its diodes clip just
    // that amplified part, which is added back on top of the clean guitar.
    // So the bass stays clean and tight and the mids get pushed: the famous
    // mid "hump" that makes it the classic boost in front of a driven amp.
    // The diodes clip both halves of the wave alike (odd harmonics), gently,
    // as diodes in an op-amp's feedback loop do. Its tone control works
    // after a fixed treble cut at 723 Hz, from dark to slightly bright.
    //
    // CLASSIC is Paragon's original overdrive:
    //
    //   low cut (tightens the bass) -> gain -> asymmetric soft clipping
    //   (8x oversampled) -> tone (high cut) -> level
    //
    // Clipping creates new high harmonics. Without oversampling, the ones
    // above half the sample rate fold back down as harsh, unmusical tones
    // (aliasing). Running the clipper at 8x the rate keeps them out: at full
    // drive a high note is close to a square wave, whose harmonics reach far
    // enough that 4x still let audible fold-back through.
    //
    // COSMONAUT is modelled on the green Russian Big Muff, stage by stage
    // from its circuit (see Cosmonaut.h; PR7-G 0.41.0, replacing the
    // original "Muff Fuzz"). DRIVE is its SUSTAIN: two clipping stages give
    // the long sustain, a small capacitor lets the bass through less
    // clipped, and the passive tone stack always scoops the mids.
    //
    // FACE FUZZ follows the silicon Fuzz Face (Gilmour's earlier sound):
    //
    //   x -> pickup loading (a soft treble cut) -> gain -> asymmetric clip -> treble cut (TONE) -> level
    //
    // One high-gain stage that clips one half of the wave harder than the
    // other: a raspy, round fuzz full of even harmonics, which cleans up
    // when the guitar's volume comes down. The original has no tone knob:
    // TONE here is a gentle treble cut.
    class Drive
    {
    public:
        // Saved by number in rigs and projects: only ever add to the end.
        enum class Model { classic, screamer, muff, face };
        static inline const juce::StringArray modelNames { "Classic", "Banshee", "Cosmonaut", "Face Fuzz" };

        struct Settings
        {
            bool on = false;
            Model model = Model::screamer;
            float drive = 5.0f;      // 0 to 10
            float tone = 5.0f;       // 0 to 10
            float levelDb = 0.0f;
        };

        Drive();
        void prepare (double sampleRate, int maxBlockSize);
        void process (juce::dsp::AudioBlock<float> monoBlock, const Settings& settings) noexcept;

        // Exposed for tests: the waveshaper itself.
        static float shape (float x) noexcept;
        static float diodeClip (float x) noexcept;

    private:
        void processClassic (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processScreamer (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processCosmonaut (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processFace (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;

        double sampleRate = 48000.0;
        juce::dsp::Oversampling<float> oversampling;
        juce::dsp::IIR::Filter<float> lowCut, toneFilter;
        juce::SmoothedValue<float> inputGain, outputGain;
        float lastTone = -1.0f;

        // Screamer: the clean guitar and the clipping path, oversampled together
        // so they stay exactly in step when they're added back together.
        juce::dsp::Oversampling<float> screamerOversampling;
        juce::AudioBuffer<float> screamerPaths;
        juce::dsp::IIR::Filter<float> screamerLowCut;
        float clipLowPassState = 0.0f, toneState = 0.0f, outputCouplingState = 0.0f;
        juce::SmoothedValue<float> screamerGain, screamerTone, screamerLevel;
        // Cosmonaut's circuit, and Face Fuzz's one-pole filters (pickup
        // loading, the transistors' top, the output coupling, tone).
        Cosmonaut cosmonaut;
        std::array<float, 8> fuzzStates {};
        juce::SmoothedValue<float> fuzzGain, fuzzTone, fuzzLevel;
        Model lastModel = Model::screamer;
        BypassFade bypass;
    };
}
