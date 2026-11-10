#pragma once

#include "EffectUtils.h"

namespace effects
{
    // BASTION: the noise gate, inspired by the Fortin Zuul+ (the owner sent
    // its manual). Like the pedal in its "4 cable method", it listens to the
    // clean guitar (its KEY IN: straight from the input, before PITCH, the
    // drive and the amp, so boosts and gain don't change when it opens) and
    // cuts the sound in the amp's effects loop: after the amp, the cab and
    // the graphic EQ, before GLITCH (Post), MOD, DELAY and REVERB, so it
    // silences the amp's hiss and leaves echo and reverb tails ringing.
    //
    // The manual's controls: GATE (how much level opens it), HOLD (keeps it
    // open through brief pauses), RELEASE (the fade from open to fully
    // closed once HOLD is over), and the key: CLEAN (KEY IN, the guitar) or
    // SELF (no key cable: it listens to what it cuts, the "2 cable method").
    // The manual gives no numbers: the ranges, the 0.5 ms opening, the 4 dB
    // hysteresis and the detector's 10 ms fall are our choices.
    class Bastion
    {
    public:
        // Saved by number in rigs and projects: only ever add to the end.
        enum class Key { clean, self };
        static inline const juce::StringArray keyNames { "Clean", "Self" };

        struct Settings
        {
            bool on = false;
            float thresholdDb = -60.0f;   // GATE
            float holdMs = 20.0f;
            float releaseMs = 120.0f;
            Key key = Key::clean;
        };

        static constexpr float openSeconds = 0.0005f;   // how fast it opens: fast enough to keep the pick attack
        static constexpr float hysteresisDb = 4.0f;     // it closes this far below GATE, so it doesn't flutter
        static constexpr float closedDb = -90.0f;       // RELEASE fades to here, then to silence

        // `maxKeyDelay`: the most the key will need delaying (the amp's latency).
        void prepare (double sampleRate, int maxBlockSize, int maxKeyDelay);
        void reset();

        // The amp's latency in samples: the clean key is delayed by this, so
        // it lines up with the amp's sound it gates.
        void setKeyDelay (int samples) noexcept;

        // Keeps the clean guitar for this block (call before the amp changes it).
        void pushKey (const float* clean, int numSamples) noexcept;

        // Gates `samples` (the mono sound after the amp, cab and EQ), using
        // the key pushed for this block.
        void process (float* samples, int numSamples, const Settings&) noexcept;

        bool isOpen() const noexcept { return open; }
        float getGain() const noexcept { return gain; }

    private:
        double sampleRate = 48000.0;
        std::vector<float> keyLine, keyBlock;   // the delay line for the key, and this block's key
        int keyWrite = 0, keyDelay = 0;

        float envelope = 0.0f;   // the detector: peak, falling about 10 ms
        float envelopeFall = 0.0f;
        float gain = 1.0f;       // what's applied, 0 to 1
        bool open = true;
        int holdLeft = 0;        // samples left of HOLD after the level drops
    };
}
