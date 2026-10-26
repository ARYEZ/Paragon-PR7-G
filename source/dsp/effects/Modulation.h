#pragma once

#include "Aeon.h"
#include "EffectUtils.h"

namespace effects
{
    // Footswitch 3: the modulation block, after the graphic EQ. Twelve types:
    //
    //   chorus         "Vortex": modelled on a late-'80s Japanese
    //                  compact stereo chorus pedal (see processVortex)
    //   flanger        AEON, modelled on the Boss BF-2 (see Aeon.h; PR7-G 0.35.0,
    //                  replacing the original generic flanger)
    //   phaser         Hadron, modelled on the MXR Phase 90, with its
    //                  SCRIPT and BLOCK circuits (see processHadron; PR7-G
    //                  0.38.0, replacing the original generic phaser)
    //   tremolo        Razor, modelled on the Boss TR-2, with its WAVE knob
    //                  (see processRazor; PR7-G 0.39.0, replacing the
    //                  original sine tremolo)
    //   dimension      Roland Dimension D / Boss CE-1 style dual chorus
    //   uniVibe        Shin-ei Uni-Vibe: four uneven phase stages, lamp LFO
    //   rotary         Leslie speaker: horn and drum rotors, Doppler, stereo mics
    //   vibrato        pure pitch vibrato
    //   harmonicTremolo  Fender brownface: lows and highs pulse in turn
    //   autoPan        sweeps between the speakers at constant loudness
    //   doubler        two extra "takes" of the part, one each side (ADT)
    //   carbonite      modelled on the Boss CE-2: the late-'70s compact
    //                  chorus, darker and tighter than Vortex (see processCarbonite;
    //                  PR7-G 0.37.0)
    //
    class Modulation
    {
    public:
        // Saved by number in rigs and projects: only ever add to the end.
        enum class Type { chorus, flanger, phaser, tremolo, dimension, uniVibe, rotary, vibrato, harmonicTremolo, autoPan, doubler, carbonite };
        static inline const juce::StringArray typeNames { "Vortex", "AEON", "Hadron", "Razor", "Dimension Chorus", "Uni-Vibe",
                                                          "Rotary", "Vibrato", "Harmonic Tremolo", "Auto-Pan", "Doubler", "Carbonite" };

        // Where the block sits: POST (after the amp and cab) or PRE (after the
        // drive, before the amp, like a pedal). Saved by number: only append.
        enum class Position { post, pre };
        static inline const juce::StringArray positionNames { "Post", "Pre" };

        // Hadron's two circuits: SCRIPT (the original) and BLOCK (with the
        // feedback resistor added later). Saved by number: only append.
        static inline const juce::StringArray hadronCircuitNames { "Script", "Block" };

        // Types for which MIX means something (the others are fully "wet" by nature).
        static bool usesMix (Type type) noexcept
        {
            // (AEON, Hadron and Carbonite have none: their pedals mix dry and wet 50/50.)
            return type == Type::chorus || type == Type::dimension
                || type == Type::uniVibe || type == Type::rotary || type == Type::doubler;
        }

        struct Settings
        {
            bool on = false;
            Type type = Type::chorus;
            float rateHz = 0.8f;
            float depth = 0.5f;   // 0 to 1
            float mix = 0.5f;     // 0 to 1 (ignored where usesMix() is false); E.LEVEL for the Vortex
            float eq = 0.0f;      // Vortex's EQ: -1 (darker) to 1 (brighter), on the chorus sound only
            bool stereoOutputs = false;   // Vortex: chorus left and dry right, as the pedal's A and B jacks
            float manual = 5.0f, resonance = 5.0f;   // AEON's MANUAL and RES, 0 to 10
            bool hadronBlock = false;   // Hadron: the BLOCK circuit (false: SCRIPT)
            float wave = 0.0f;          // Razor's WAVE, 0 (triangle) to 10 (square)
        };

        // The Vortex's EQ: how much it cuts or boosts the chorus's
        // treble at either end of the knob (for tests).
        static constexpr float vortexEqRangeDb = 10.0f;

        // Carbonite's delay in ms, for a DEPTH (0 to 1) and a point in the
        // LFO's sweep (-1 to 1) (for tests).
        static float carboniteDelayMs (float depth, float lfo) noexcept;

        // Hadron: where each phase stage's turn sits (its 90-degree point),
        // in Hz, for the LFO's voltage at the transistors' gates (for tests).
        static float hadronStageHz (float lfoVolts) noexcept;
        static constexpr float hadronLfoLowVolts = 3.1f, hadronLfoHighVolts = 3.4f;

        // Razor's volume, 0 to 1, at a point in the LFO's cycle (0 to 1),
        // before the corner-rounding capacitor (for tests).
        static float razorShape (float phase, float wave) noexcept;

        Modulation();
        void prepare (double sampleRate, int maxBlockSize);
        void process (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;

    private:
        void processVortex (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void updateVortexEq (float eq) noexcept;
        void processHadron (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processCarbonite (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processRazor (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processDimension (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processUniVibe (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processRotary (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processVibrato (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processHarmonicTremolo (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processAutoPan (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        void processDoubler (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept;
        float advanceLfo (float rateHz) noexcept;

        double sampleRate = 48000.0;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delayLine;
        float lfoPhase = 0.0f;   // 0 to 1
        juce::SmoothedValue<float> depth, mix;
        BypassFade bypass;
        Type lastType = Type::chorus;

        // AEON, the flanger.
        Aeon aeon;

        // Dimension: the darker tone of the bucket-brigade chips the originals used.
        float dimensionTone[maxChannels] {};

        // Vortex: the pedal's chorus path, per channel. A small biquad
        // with its coefficients worked out here, so changing the EQ never
        // allocates on the audio thread.
        struct Biquad
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;   // normalised: a0 = 1
            float z1 = 0.0f, z2 = 0.0f;
            float process (float x) noexcept
            {
                const auto y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void copyCoefficients (const Biquad& other) noexcept { b0 = other.b0; b1 = other.b1; b2 = other.b2; a1 = other.a1; a2 = other.a2; }
        };
        struct VortexPath
        {
            Biquad emphasis, antiAlias1, antiAlias2, reconstruct1, reconstruct2, deEmphasis, eq;
        };
        VortexPath vortexPaths[maxChannels];
        float vortexEq = 0.0f;

        // One first-order all-pass stage's memory (Hadron, Uni-Vibe).
        struct AllPassState { float x1 = 0.0f, y1 = 0.0f; };

        // Carbonite: the same kind of chorus path, with its own filters.
        struct CarbonitePath
        {
            Biquad inputHighPass, emphasis, antiAlias1, antiAlias2, reconstruct1, reconstruct2, deEmphasis;
        };
        CarbonitePath carbonitePaths[maxChannels];

        // Hadron: four phase stages per channel, and what the BLOCK
        // circuit's feedback resistor carries back from the last one.
        AllPassState hadronStages[maxChannels][4] {};
        float hadronFeedback[maxChannels] {};

        // Razor: the LFO after its corner-rounding capacitor.
        float razorLfo = 0.0f;

        // Uni-Vibe: four first-order all-pass stages per channel, and the lamp's glow.
        AllPassState vibeStages[maxChannels][4] {};
        float lampGlow = 0.0f;

        // Rotary: a crossover into horn (treble) and drum (bass), each with
        // its own delay line for the Doppler shift and its own spinning speed.
        juce::dsp::LinkwitzRileyFilter<float> rotaryCrossover;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> hornDelay, drumDelay;
        float hornPhase = 0.0f, drumPhase = 0.0f, hornHz = 0.8f, drumHz = 0.7f, hornTone = 0.0f;

        // Harmonic tremolo: a crossover per channel.
        juce::dsp::LinkwitzRileyFilter<float> harmonicCrossover;
    };
}
