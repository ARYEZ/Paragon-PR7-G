#pragma once

#include "EffectUtils.h"
#include "CipherSpectral.h"

#include <array>
#include <atomic>
#include <vector>

namespace effects
{
    // CIPHER: an envelope-controlled filter and multi-effect, the block after
    // HAVOK, inspired by Devious Machines' Infiltrator 2 (the owner sent its
    // manual). Our own design, built from the manual's descriptions: ten
    // slots in series, each with an effect, a main knob, two parameter
    // knobs, an envelope that moves the main knob (played by its own LFO,
    // synced to the song or free), ENV MOD STYLE (both ways, only up, only
    // down), GLIDE, STEREO, GAIN and MIX; then the master section: a drive
    // before or after the slots, a compressor (Squash, Maximizer, Multiband,
    // Overkill), CLEAN BASS and the output GAIN and MIX.
    //
    // Stage 1 (0.72.0): the filters (Ladder, Digital, 101 and Model-20, each
    // LP, BP and HP; Notch, Dual Filter and Formant), the EQ (bell and the
    // two shelves) and the utilities Gain and Pan. The manual gives no
    // curves, ranges or circuits: every frequency, curve, resonance law and
    // drive shape here is ours, chosen to do what the manual says each one
    // does. Later stages add the time effects, the sequencer and
    // retriggering, the spectral effects and the macros.
    class Cipher
    {
    public:
        static constexpr int numSlots = 10;

        // Saved by number in rigs and projects: only ever add to the end.
        enum class Type { off, ladderLp, ladderBp, ladderHp, digitalLp, digitalBp, digitalHp, oneOhOneLp, oneOhOneBp, oneOhOneHp,
                          model20Lp, model20Bp, model20Hp, notch, dualFilter, formant, eqBell, eqLowShelf, eqHighShelf, gain, pan,
                          // Stage 2 (0.73.0): the time effects and the Gate and Compress utilities.
                          comb, phaser, pitchShift, freqShift, ringMod, fm, distort, decimate, delay, reverb, reflect, vocoder,
                          loop, stretch, varispeed, slow, reverse, gate, compress,
                          // Stage 4 (0.75.0): the spectral (FFT) effects.
                          fftShift, fftRotate, fftTwist, fftMirror, fftBandpass, fftStretch, fftScatter, fftCascade, fftSmear,
                          fftReverb, fftCompress, fftCorrupt, fftLofi, fftSpread };
        static inline const juce::StringArray typeNames { "Off", "Ladder LP", "Ladder BP", "Ladder HP", "Digital LP", "Digital BP", "Digital HP",
                                                          "101 LP", "101 BP", "101 HP", "Model-20 LP", "Model-20 BP", "Model-20 HP",
                                                          "Notch", "Dual Filter", "Formant", "EQ Bell", "EQ Low Shelf", "EQ High Shelf",
                                                          "Gain", "Pan",
                                                          "Comb", "Phaser", "Pitch Shift", "Freq Shift", "Ring Mod", "FM", "Distort",
                                                          "Decimate", "Delay", "Reverb", "Reflect", "Vocoder", "Loop", "Stretch",
                                                          "Varispeed", "Slow", "Reverse", "Gate", "Compress",
                                                          "FFT Shift", "FFT Rotate", "FFT Twist", "FFT Mirror", "FFT Bandpass",
                                                          "FFT Stretch", "FFT Scatter", "FFT Cascade", "FFT Smear", "FFT Reverb",
                                                          "FFT Compress", "FFT Corrupt", "FFT Lofi", "FFT Spread" };
        static bool isSpectral (Type t) noexcept { return t >= Type::fftShift; }
        // The spectral effects' FFT SIZE (on OPTION; 2048 by default).
        static inline const juce::StringArray fftSizeNames { "512", "1024", "2048", "4096" };
        static bool isTimeEffect (Type t) noexcept { return t >= Type::comb; }
        // OPTION: a filter's steepness (Ladder, Digital and Model-20), or an
        // effect's mode (Pan, Phaser, Distort, Delay, Reverb, Loop, Varispeed,
        // Compress). Saved by number: only add to the end.
        static inline const juce::StringArray optionNames { "6 dB / Pan", "12 dB / Balance", "18 dB / Rotate", "24 dB", "Option 5", "Option 6" };
        static inline const juce::StringArray slopeNames { "6", "12", "18", "24" };
        static inline const juce::StringArray panModeNames { "PAN", "BALANCE", "ROTATE" };
        static inline const juce::StringArray phaserModeNames { "TURBO", "DUO", "INVERSE" };
        static inline const juce::StringArray delayModeNames { "SYNC", "FREE" };
        static inline const juce::StringArray reverbModeNames { "REVERB", "LO-FI", "VOID" };
        static inline const juce::StringArray loopModeNames { "FREE", "SYNC" };
        static inline const juce::StringArray varispeedModeNames { "FORWARD", "FWD-REV", "REVERSE" };
        static inline const juce::StringArray compModeNames { "SQUASH", "MAXIMIZER", "MULTIBAND", "OVERKILL" };
        static inline const juce::StringArray driveModeNames { "OVERDRIVE", "TUBE", "FUZZ", "HARD CLIP", "FOLDBACK", "RECTIFY" };
        // The Delay's STEREO knob picks its kind (the manual's list; the fractions put the left echo at that part of the right's time).
        static inline const juce::StringArray delayStereoNames { "1/4", "1/3", "1/2", "STEREO", "3/4", "WIDE", "PING-PONG" };
        static int delayStereoKind (float stereo) noexcept { return juce::jlimit (0, 6, juce::roundToInt ((stereo + 1.0f) * 3.0f)); }
        // The synced lengths the main knob or a knob steps through.
        static inline const juce::StringArray noteNames { "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
        static int noteStep (float knob01, int steps) noexcept { return juce::jlimit (0, steps - 1, juce::roundToInt (juce::jlimit (0.0f, 1.0f, knob01) * (float) (steps - 1))); }
        static inline const juce::StringArray slowNames { "FULL", "1/2", "1/4", "1/8", "1/16" };
        enum class Style { both, up, down, snap };   // SNAP (0.74.0): both ways, snapped to the scale's notes
        static inline const juce::StringArray styleNames { "Both", "Up", "Down", "Snap" };
        // PITCH SNAP's scales (the manual's list) and their notes, from the root.
        static inline const juce::StringArray scaleNames { "Chromatic", "Natural Minor", "Harmonic Minor", "Major", "Minor Pentatonic" };
        static inline const juce::StringArray rootNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static bool inScale (int scale, int semitoneFromRoot) noexcept;
        // Effects whose main knob is a pitch (the manual's list), and the main knob snapped to the scale.
        static bool canSnap (Type) noexcept;
        static float snapMain (Type, float main01, int scale, int root) noexcept;
        static juce::String noteName (float hz);
        // The second modulator: its source and where it goes (saved by number: only add to the end).
        enum class ModSource { audio, envelope2 };
        static inline const juce::StringArray modSourceNames { "Audio Follower", "Envelope 2" };
        enum class ModDest { main, p1, p2, stereo, gain, mix };
        static inline const juce::StringArray modDestNames { "Main", "Knob 1", "Knob 2", "Stereo", "Gain", "Mix" };
        // The sequencer's step lengths.
        static inline const juce::StringArray seqSpeedNames { "1/4", "1/8", "1/16", "1/32", "1/64" };
        static constexpr int numSteps = 32;
        // The LFO's synced speeds: how long one pass of the envelope takes.
        static inline const juce::StringArray speedNames { "8 bars", "4 bars", "2 bars", "1 bar", "1/2 dotted", "1/2", "1/2 triplet",
                                                           "1/4 dotted", "1/4", "1/4 triplet", "1/8 dotted", "1/8", "1/8 triplet",
                                                           "1/16 dotted", "1/16", "1/16 triplet", "1/32" };
        static double speedBeats (int speed) noexcept;
        // The master drive's types (ours: the manual names only "Overdrive") and compressors (the manual's four).
        static inline const juce::StringArray driveNames { "Overdrive", "Tube", "Fuzz", "Hard Clip", "Foldback", "Rectify" };
        enum class Comp { squash, maximizer, multiband, overkill };
        static inline const juce::StringArray compNames { "Squash", "Maximizer", "Multiband", "Overkill" };
        static inline const juce::StringArray mixLawNames { "-6 dB", "-3 dB", "0 dB" };

        // What each type's knobs are called (empty: the knob does nothing for
        // this type); `stereo` renames the STEREO knob where the effect uses it
        // for something else (the Vocoder's CHARACTER, as the manual says).
        struct Labels
        {
            const char* main; const char* p1; const char* p2; bool hasSlope;
            const char* stereo = "STEREO";
            const juce::StringArray* modes = nullptr;   // the effect's modes, on OPTION
            bool isPan() const noexcept { return modes == &panModeNames; }
        };
        static Labels labelsFor (Type) noexcept;
        // OPTION's default (3, a filter's 24 dB) is every effect's first mode.
        static int modeOf (Type, int option) noexcept;
        static int optionFor (Type, int mode) noexcept;
        static bool isFilter (Type t) noexcept { return t >= Type::ladderLp && t <= Type::formant; }

        //======================================================================
        // The envelope: points from left (x 0) to right (x 1), each 0 (bottom)
        // to 1 (top), with a CURVE for the line to the next point (-1 to 1:
        // 0 straight, positive bowing up, negative down).
        struct Point { float x = 0.0f, y = 0.5f, curve = 0.0f; };
        using Envelope = std::vector<Point>;
        static constexpr int maxPoints = 64;
        static float envelopeAt (const Envelope&, float x) noexcept;
        // Keeps points in order, inside 0-1, at most maxPoints, at least one.
        static Envelope tidy (Envelope);
        // The shapes above the editor (ours, after the manual's pictures).
        static inline const juce::StringArray shapeNames { "Flat", "Ramp Up", "Ramp Down", "Triangle", "Hump", "Square", "Steps", "Pluck" };
        static Envelope shape (int index);
        // As saved in rigs and projects: "x y curve;..." to 4 decimals.
        static juce::String toText (const Envelope&);
        static Envelope fromText (const juce::String&);

        //======================================================================
        struct Slot
        {
            bool on = true;
            Type type = Type::off;
            float main = 0.5f, p1 = 0.0f, p2 = 0.0f;   // 0 to 1
            int option = 3;                             // 24 dB for a filter, PAN for Pan
            float envAmount = 0.5f;                     // -1 to 1: how far the envelope moves the main knob
            Style style = Style::both;
            bool sync = true;
            int speed = 3;                              // 1 bar
            float rateHz = 1.0f;
            float glideMs = 0.0f;
            float stereo = 0.0f;                        // -1 to 1: left and right moved apart
            float gainDb = 0.0f;
            float mix = 1.0f;
            // Stage 3: retriggering and the second modulator.
            bool retrigAudio = false, retrigMidi = false, randomStart = false, envLoop = true;
            float startPos = 0.0f, loopStart = 0.0f, loopEnd = 1.0f;   // 0 to 1 across the envelope
            ModSource modSource = ModSource::audio;
            ModDest modDest = ModDest::main;
            float modAmount = 0.0f;                                     // -1 to 1
        };

        struct Master
        {
            bool driveOn = false;
            int driveType = 0;
            float drive = 0.3f, tone = 0.5f;            // 0 to 1
            bool driveAfter = true;                     // Effects -> Drive (true) or Drive -> Effects
            bool compOn = false;
            Comp comp = Comp::squash;
            float compress = 0.3f;                      // 0 to 1
            bool cleanBassOn = false;
            float cleanBassHz = 120.0f;
            bool cleanBassListen = false;
            float gainDb = 0.0f;
            float mix = 1.0f;
            int mixLaw = 1;                             // -3 dB
            // Stage 3: the sequencer, MIDI, PITCH SNAP's scale, the audio trigger and follower.
            bool seqOn = false, midiOn = false;
            int seqSpeed = 2, seqStart = 0, seqEnd = 15;                // 1/16, steps 1-16
            int scale = 0, root = 0;
            float trigLevelDb = -24.0f, trigHpfHz = 20.0f, trigHysteresisDb = 6.0f, trigHoldOffMs = 40.0f;
            float followAttackMs = 10.0f, followHoldMs = 0.0f, followDecayMs = 200.0f, followGateDb = -60.0f;
        };

        struct Settings
        {
            bool on = false;
            std::array<Slot, numSlots> slots;
            Master master;
            double bpm = 120.0;
            double songBeats = -1.0;   // the song position at the block's start; -1 when not playing
        };

        Cipher();
        ~Cipher();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // Message thread: a slot's envelope (sampled into a table the audio thread reads).
        // Envelopes 0-9 are the slots' envelopes; 10-19 their ENVELOPE 2.
        static constexpr int numEnvelopes = numSlots * 2;
        void setEnvelope (int index, const Envelope&);
        const Envelope& getEnvelope (int index) const { return envelopes[(size_t) juce::jlimit (0, numEnvelopes - 1, index)]; }

        // The sequencer's steps: bit n of row r lit means slot r plays on step n+1 (message thread).
        void setSequenceRow (int row, juce::uint32 steps) noexcept { sequence[(size_t) juce::jlimit (0, numSlots - 1, row)] = steps; }
        juce::uint32 getSequenceRow (int row) const noexcept { return sequence[(size_t) juce::jlimit (0, numSlots - 1, row)].load(); }
        static juce::String sequenceToText (const std::array<juce::uint32, numSlots>&);
        static std::array<juce::uint32, numSlots> sequenceFromText (const juce::String&);

        // MIDI (audio thread, before process): note-ons retrigger, and with MIDI on,
        // the held notes' classes (C to A) switch slots 1-10 on.
        void handleMidi (const juce::MidiBuffer&) noexcept;
        // The delay the spectral effects add, in samples (the slots' FFT sizes added up), for the host.
        int getLatencySamples() const noexcept { return latency.load(); }
        // For the panel: the step playing, the audio follower's level, whether a slot sounds now.
        int getStep() const noexcept { return step.load(); }
        float getFollower() const noexcept { return follower.load(); }
        bool isSlotActive (int slot) const noexcept { return active[(size_t) slot].load(); }

        void process (juce::dsp::AudioBlock<float> stereo, const Settings&);

        // For the panel: where each slot's envelope is (0-1) and the main knob as moved (0-1).
        float getPhase (int slot) const noexcept { return phases[(size_t) slot].load(); }
        float getMovedMain (int slot) const noexcept { return moved[(size_t) slot].load(); }

        // The knobs' real values, shared by the engine and the panel's readouts.
        static float cutoffHz (float main01) noexcept { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, main01)); }
        static float gainUtilityDb (float main01) noexcept;   // the Gain utility's main knob: -60 (silent) to +12 dB, 0 dB at 0.8
        static float eqGainDb (float p01) noexcept { return (juce::jlimit (0.0f, 1.0f, p01) - 0.5f) * 36.0f; }   // +-18 dB
        static float eqQ (float p01) noexcept { return 0.3f * std::pow (40.0f, juce::jlimit (0.0f, 1.0f, p01)); }  // 0.3 to 12
        static float driveGainDb (float drive01) noexcept { return juce::jlimit (0.0f, 1.0f, drive01) * 36.0f; }
        static juce::String mainText (Type, float main01, int option = 3);
        static juce::String p1Text (Type, float p01);
        static juce::String p2Text (Type, float p01);
        static juce::String stereoText (Type, float stereo);
        // The time effects' knobs, shared by the engine and the readouts.
        static float semitones (float main01) noexcept { return std::round ((main01 - 0.5f) * 48.0f * 10.0f) / 10.0f; }   // Pitch Shift: +-24
        static float shiftHz (float main01) noexcept;            // Freq Shift: +-2 kHz, finer near the centre
        static float freeDelayMs (float main01) noexcept { return std::pow (2000.0f, juce::jlimit (0.0f, 1.0f, main01)); }   // 1 ms to 2 s
        static float reverbSeconds (float main01) noexcept { return 0.2f * std::pow (100.0f, juce::jlimit (0.0f, 1.0f, main01)); }   // 0.2 to 20 s
        static float loopHz (float main01) noexcept { return std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, main01)); }   // 1 Hz to 1 kHz
        static float gateThresholdDb (float main01) noexcept { return -80.0f + 80.0f * juce::jlimit (0.0f, 1.0f, main01); }

        static constexpr int tableSize = 512;
        static constexpr int tick = 16;   // samples between control updates (the envelope, filter coefficients)

    private:
        struct SlotState;
        struct MasterState;
        std::array<std::unique_ptr<SlotState>, numSlots> slotStates;
        std::unique_ptr<MasterState> master;

        std::array<Envelope, numEnvelopes> envelopes;   // message thread's copy
        std::array<std::array<std::atomic<float>, tableSize>, numEnvelopes> tables;
        std::array<std::atomic<float>, numSlots> phases, moved;
        std::array<std::atomic<juce::uint32>, numSlots> sequence {};
        std::atomic<int> step { 0 }, latency { 0 };
        std::array<std::unique_ptr<CipherSpectral>, numSlots> spectral;
        // The dry sound delayed by the spectral slots' latency, for MIX and CLEAN BASS.
        static constexpr int maxLatency = 4096 * numSlots;
        std::array<std::vector<float>, 2> dryLine;
        int dryWrite = 0;
        std::atomic<float> follower { 0.0f };
        std::array<std::atomic<bool>, numSlots> active {};
        // The block's triggers (sample offsets), the audio trigger's and follower's state, MIDI.
        std::array<int, 32> audioTriggers {};
        int numAudioTriggers = 0;
        juce::uint32 noteOns = 0;   // bit n: a note of pitch class n started this block
        juce::uint32 heldNotes = 0;   // bit n: a note of pitch class n is held
        std::array<int, 12> heldCount {};
        float triggerLevelDb = -120.0f, triggerHp[2] {}, followerLevel = 0.0f;
        bool triggerArmed = true;
        int holdOffLeft = 0, followHoldLeft = 0;
        std::vector<float> followerTicks;   // the follower's value at each tick of the block
        double freeBeats = 0.0;   // the sequencer's place (in beats) when the song isn't playing
        juce::Random random { 7 };

        double sampleRate = 48000.0;
        BypassFade fade;
        juce::AudioBuffer<float> dry, low;   // the input (for MIX and CLEAN BASS)

        void processSlot (int index, juce::dsp::AudioBlock<float>&, const Slot&, const Settings&);
        // A time effect over [start, start + count), with this tick's main knob per side.
        void processTime (int index, juce::dsp::AudioBlock<float>&, int start, int count, const Slot&, const Settings&,
                          const float* mainPerSide);
        struct TimeState;
        std::array<std::unique_ptr<TimeState>, numSlots> timeStates;
        int maxBlock = 512;
    };
}
