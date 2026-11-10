// BASTION, the noise gate inspired by the Fortin Zuul+: GATE, HOLD, RELEASE
// and the key, checked on the levels it lets through.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/effects/Bastion.h"

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    int ms (double milliseconds) { return (int) std::lround (milliseconds * 0.001 * sampleRate); }
    float dB (float gain) { return 20.0f * std::log10 (juce::jmax (1.0e-9f, gain)); }

    // Runs BASTION over `length` samples: the key is the clean guitar (a
    // 220 Hz note at -20 dB while `playing` says so), the gated sound is a
    // steady 1.0 (so the output is the gain it applies, sample by sample).
    std::vector<float> run (effects::Bastion& bastion, const effects::Bastion::Settings& settings, int length,
                            const std::function<bool (int)>& playing, float keyLevelDb = -20.0f)
    {
        std::vector<float> gains;
        std::vector<float> key ((size_t) blockSize), sound ((size_t) blockSize);
        const auto amplitude = juce::Decibels::decibelsToGain (keyLevelDb);
        for (int start = 0; start < length; start += blockSize)
        {
            const auto count = juce::jmin (blockSize, length - start);
            for (int i = 0; i < count; ++i)
            {
                const auto n = start + i;
                key[(size_t) i] = playing (n) ? amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * n / sampleRate) : 0.0f;
                sound[(size_t) i] = 1.0f;
            }
            bastion.pushKey (key.data(), count);
            bastion.process (sound.data(), count, settings);
            gains.insert (gains.end(), sound.begin(), sound.begin() + count);
        }
        return gains;
    }

    effects::Bastion prepared()
    {
        effects::Bastion bastion;
        bastion.prepare (sampleRate, blockSize, 8192);
        return bastion;
    }
}

TEST_CASE ("BASTION: open while you play, HOLD then a RELEASE fade when you stop", "[effects][bastion]")
{
    auto bastion = prepared();
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -50.0f;
    settings.holdMs = 50.0f;
    settings.releaseMs = 200.0f;

    const auto stop = ms (300);
    const auto gains = run (bastion, settings, ms (700), [stop] (int n) { return n < stop; });

    // While playing: fully open.
    for (int n = ms (5); n < stop; ++n)
        REQUIRE (gains[(size_t) n] == 1.0f);

    // The detector falls (10 ms time constant) from the note's -20 dB to under
    // GATE - 4 dB (-54 dB): 34 dB, about 39 ms. Then HOLD keeps it open 50 ms more.
    const auto fallMs = 10.0 * std::log (juce::Decibels::decibelsToGain (34.0));
    CHECK (gains[(size_t) (stop + ms (fallMs + 50 - 2))] == 1.0f);
    // Then RELEASE fades it in a straight line in dB to -90 dB over 200 ms:
    // half way through, about -45 dB.
    const auto fadeStart = [&]
    {
        for (int n = stop; n < (int) gains.size(); ++n)
            if (gains[(size_t) n] < 1.0f)
                return n;
        return (int) gains.size();
    }();
    CAPTURE (fadeStart - stop);
    CHECK (fadeStart - stop >= ms (fallMs + 50 - 2));   // the detector's fall, then HOLD
    CHECK (fadeStart - stop <= ms (fallMs + 50 + 2));
    CHECK_THAT (dB (gains[(size_t) (fadeStart + ms (100))]), WithinAbs (-45.0, 1.5));
    CHECK (gains[(size_t) (fadeStart + ms (210))] == 0.0f);   // closed: silence
}

TEST_CASE ("BASTION: HOLD carries it through a short pause; without HOLD it starts to close", "[effects][bastion]")
{
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -50.0f;
    settings.releaseMs = 100.0f;
    // A 100 ms pause between two notes (the detector takes about 39 ms of it
    // to fall under the closing level, see above).
    const auto playing = [] (int n) { return n < ms (200) || n >= ms (300); };

    auto held = prepared();
    settings.holdMs = 80.0f;
    const auto withHold = run (held, settings, ms (400), playing);
    CHECK (*std::min_element (withHold.begin() + ms (5), withHold.end()) == 1.0f);

    auto unheld = prepared();
    settings.holdMs = 0.0f;
    const auto withoutHold = run (unheld, settings, ms (400), playing);
    CHECK (dB (*std::min_element (withoutHold.begin() + ms (5), withoutHold.end())) < -40.0f);
    CHECK (withoutHold.back() == 1.0f);   // open again for the second note
}

TEST_CASE ("BASTION: it opens fast enough to keep the pick attack, and has hysteresis", "[effects][bastion]")
{
    auto bastion = prepared();
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -50.0f;
    settings.holdMs = 0.0f;
    settings.releaseMs = 50.0f;

    // Closed, then a note: fully open within a millisecond of it starting.
    const auto start = ms (300);
    const auto gains = run (bastion, settings, ms (400), [start] (int n) { return n >= start; });
    CHECK (gains[(size_t) (start - 1)] == 0.0f);
    CHECK (gains[(size_t) (start + ms (1.5))] == 1.0f);

    // A note dying away just under GATE (2 dB under) keeps it open; 6 dB under, it closes.
    const auto quieter = [&] (float belowDb)
    {
        auto gate = prepared();
        std::vector<float> key ((size_t) blockSize), sound ((size_t) blockSize);
        float last = 1.0f;
        for (int start2 = 0; start2 < ms (500); start2 += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto n = start2 + i;
                const auto level = n < ms (100) ? -20.0f : settings.thresholdDb - belowDb;
                key[(size_t) i] = juce::Decibels::decibelsToGain (level) * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * n / sampleRate);
                sound[(size_t) i] = 1.0f;
            }
            gate.pushKey (key.data(), blockSize);
            gate.process (sound.data(), blockSize, settings);
            last = sound.back();
        }
        return last;
    };
    CHECK (quieter (2.0f) == 1.0f);
    CHECK (quieter (6.0f) == 0.0f);
}

TEST_CASE ("BASTION: KEY Clean listens to the guitar, Self to the sound it cuts", "[effects][bastion]")
{
    // The amp's hiss at -40 dB, well over GATE (-55 dB), and a silent guitar.
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -55.0f;
    settings.holdMs = 20.0f;
    settings.releaseMs = 100.0f;
    const auto hiss = juce::Decibels::decibelsToGain (-40.0f);

    const auto hissOut = [&] (effects::Bastion::Key key)
    {
        auto bastion = prepared();
        settings.key = key;
        juce::Random random (7);
        std::vector<float> silentKey ((size_t) blockSize, 0.0f), sound ((size_t) blockSize);
        double sum = 0.0;
        int counted = 0;
        for (int start = 0; start < ms (500); start += blockSize)
        {
            for (auto& s : sound)
                s = hiss * (random.nextFloat() * 2.0f - 1.0f);
            bastion.pushKey (silentKey.data(), blockSize);
            bastion.process (sound.data(), blockSize, settings);
            if (start >= ms (300))
                for (auto s : sound)
                {
                    sum += s * s;
                    ++counted;
                }
        }
        return (float) std::sqrt (sum / counted);
    };
    CHECK (hissOut (effects::Bastion::Key::clean) == 0.0f);              // the guitar is silent: closed
    CHECK (dB (hissOut (effects::Bastion::Key::self) / hiss) > -6.0f);   // the hiss itself holds it open
}

TEST_CASE ("BASTION: the key is delayed by the amp's latency, so it opens with the amp's sound", "[effects][bastion]")
{
    auto bastion = prepared();
    bastion.setKeyDelay (ms (10));   // say the amp is 10 ms late
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -50.0f;
    settings.releaseMs = 50.0f;
    const auto start = ms (200);
    const auto gains = run (bastion, settings, ms (300), [start] (int n) { return n >= start; });
    CHECK (gains[(size_t) (start + ms (8))] == 0.0f);    // the guitar has started, the amp's sound hasn't
    CHECK (gains[(size_t) (start + ms (12))] == 1.0f);   // now it has: open
}

TEST_CASE ("BASTION: switched off it lets everything through, without a click", "[effects][bastion]")
{
    auto bastion = prepared();
    effects::Bastion::Settings settings;
    settings.on = true;
    settings.thresholdDb = -50.0f;
    settings.releaseMs = 20.0f;
    auto gains = run (bastion, settings, ms (200), [] (int) { return false; });
    REQUIRE (gains.back() == 0.0f);

    settings.on = false;
    gains = run (bastion, settings, ms (10), [] (int) { return false; });
    CHECK (gains.front() < 0.1f);   // a short ramp, not a jump
    CHECK (gains.back() == 1.0f);
}
