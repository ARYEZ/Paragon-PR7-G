#pragma once

#include "EffectUtils.h"

#include <array>
#include <atomic>
#include <memory>

namespace effects
{
    // HAVOK: an X/Y touch pad effect, the last block in the chain, inspired
    // by the original Korg Kaoss Pad (KP1; the owner sent its manual). Our
    // own design. From the manual, used exactly: its 60 programs, what X
    // (left to right) and Y (bottom to top) control on each, the touch,
    // HOLD and PROGRAM MAP behaviour, the 1.1 to 9.9 position readout, and
    // the sample/play programs (51-60) with REC and AUTO REC. The manual
    // gives no sounds or ranges: every frequency, time, depth and curve
    // here is our guess, marked where it's chosen.
    //
    // The pad works as the manual describes: the effect is applied while
    // you touch it (or while HOLD keeps the last position). Let go and the
    // filters, distortion and modulation go back to the dry sound at once,
    // while delays and reverbs stop taking in sound and ring out (so a tap
    // on a snare hit sends only that hit into the echo, as the manual's
    // "tap with snare drum" suggests).
    class Havok
    {
    public:
        static constexpr int numPrograms = 60;

        // The manual's table: name, what X and Y do, and its playing hint.
        struct Program { const char* name; const char* x; const char* y; const char* hint; };
        static const std::array<Program, numPrograms>& programs();
        // "1 LOW PASS FILTER" ... "60 SAMPLE/PLAY2+PAN": saved by number, never reorder.
        static juce::StringArray programNames();

        // Programs 51-60 (indices 50-59) sample a phrase and play it from the pad.
        static constexpr int firstSampler = 50;
        static bool isSampler (int program) noexcept { return program >= firstSampler && program < numPrograms; }
        // 54-56: recording starts by itself when sound comes in.
        static bool isAutoRec (int program) noexcept { return program >= 53 && program <= 55; }
        // The longest phrase (our guess: the manual says only that it depends on the program).
        static constexpr float maxRecordSeconds = 6.0f;

        // Where the sampler is: waiting for REC (its light blinking), waiting
        // for sound (AUTO REC), recording, or holding a phrase to play.
        enum class Rec { ready, waiting, recording, recorded };

        struct Settings
        {
            bool on = false;
            int program = 0;          // 0 to 59 (the display shows 1 to 60)
            float x = 0.5f, y = 0.5f; // 0 to 1: left to right, bottom to top
            bool touch = false;       // a finger on the pad
            bool hold = false;        // HOLD: the effect stays at the last position
        };

        Havok();
        ~Havok();
        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;
        // Stereo in, stereo out (the same signal on both sides is fine).
        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;

        // REC, from the panel (any thread): picked up at the next block.
        void pressRec() noexcept { recPresses.fetch_add (1); }
        Rec getRecState() const noexcept { return (Rec) recState.load(); }
        float getRecordedSeconds() const noexcept { return recordedSeconds.load(); }
        // Where playback is in the phrase, 0 to 1 (-1 while not playing), for the display.
        float getPlayPosition() const noexcept { return playPosition.load(); }

    private:
        struct Engine;
        std::unique_ptr<Engine> engine;
        BypassFade bypass;
        std::atomic<int> recPresses { 0 }, recState { (int) Rec::ready };
        std::atomic<float> recordedSeconds { 0.0f }, playPosition { -1.0f };
        int recPressesSeen = 0;
    };
}
