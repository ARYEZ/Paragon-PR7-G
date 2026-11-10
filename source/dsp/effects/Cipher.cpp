#include "Cipher.h"

namespace effects
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;

        float softClip (float x) noexcept { return x / std::sqrt (1.0f + x * x); }   // smooth, never above 1

        // A state-variable filter's coefficients (Andrew Simper's trapezoidal
        // form): low, band and high outputs from one pair of states, stable
        // while its frequency moves every few samples.
        struct Svf
        {
            float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.4f;
            void set (float g, float q) noexcept
            {
                k = 1.0f / juce::jmax (0.05f, q);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            struct Out { float low, band, high; };
            Out run (float x, float* s) const noexcept
            {
                const auto v3 = x - s[1];
                const auto v1 = a1 * s[0] + a2 * v3;
                const auto v2 = s[1] + a2 * s[0] + a3 * v3;
                s[0] = 2.0f * v1 - s[0];
                s[1] = 2.0f * v2 - s[1];
                return { v2, v1, x - k * v1 - v2 };
            }
        };

        float prewarp (float hz, double sampleRate) noexcept
        {
            return std::tan (pi * juce::jlimit (5.0f, (float) (0.45 * sampleRate), hz) / (float) sampleRate);
        }

        // The Formant filter's vowels: the first three formants of A, E, I,
        // O, U (typical adult values from speech science; the levels are ours).
        constexpr float vowels[5][3] = { { 730.0f, 1090.0f, 2440.0f }, { 530.0f, 1840.0f, 2480.0f }, { 270.0f, 2290.0f, 3010.0f },
                                         { 570.0f, 840.0f, 2410.0f }, { 300.0f, 870.0f, 2240.0f } };
        constexpr float formantLevels[3] = { 1.0f, 0.63f, 0.32f };

        // The drive types' curves, shared by the master drive and the Distort effect.
        float driveShape (int type, float in) noexcept
        {
            switch (type)
            {
                case 0:  return std::tanh (in + 0.15f) - std::tanh (0.15f);                                        // Overdrive: soft, a little uneven
                case 1:  return in >= 0.0f ? 1.0f - std::exp (-in) : -0.7f * (1.0f - std::exp (in / 0.7f));       // Tube: uneven halves
                case 2:  return (in >= 0.0f ? 1.0f : -1.0f) * (1.0f - std::exp (-4.0f * std::abs (in)));         // Fuzz: square-ish
                case 3:  return juce::jlimit (-1.0f, 1.0f, in);                                                  // Hard Clip
                case 4:  return std::sin (in * pi * 0.5f);                                                      // Foldback: folds back past full
                default: return 2.0f * std::abs (std::tanh (in)) - 0.5f;                                         // Rectify: an octave up
            }
        }

        // The four compressors, linked across both sides: the master's, and
        // the Compress effect's in a slot.
        struct Compressor
        {
            double sampleRate = 48000.0;
            float envelopeDb = -120.0f, limiterGain = 1.0f;
            std::array<float, 3> bandDb { -120.0f, -120.0f, -120.0f };
            juce::dsp::LinkwitzRileyFilter<float> split1, split2, allpass1;   // 200 Hz, 2.5 kHz, and the low band's matching all-pass
            juce::AudioBuffer<float> bands;

            void prepare (double rate, int maxBlockSize)
            {
                sampleRate = rate;
                bands.setSize (6, maxBlockSize);
                const juce::dsp::ProcessSpec spec { rate, (juce::uint32) maxBlockSize, 2 };
                for (auto* f : { &split1, &split2, &allpass1 })
                    f->prepare (spec);
                split1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
                split2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
                allpass1.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
                split1.setCutoffFrequency (200.0f);
                split2.setCutoffFrequency (2500.0f);
                allpass1.setCutoffFrequency (2500.0f);
                reset();
            }

            void reset()
            {
                for (auto* f : { &split1, &split2, &allpass1 })
                    f->reset();
                envelopeDb = -120.0f;
                bandDb = { -120.0f, -120.0f, -120.0f };
                limiterGain = 1.0f;
            }

            void process (float* const* chans, int numChannels, int numSamples, Cipher::Comp comp, float amount01)
            {
                using Comp = Cipher::Comp;
                const auto amount = juce::jlimit (0.0f, 1.0f, amount01);
                const auto coeff = [this] (float ms) { return std::exp (-1.0f / (0.001f * ms * (float) sampleRate)); };
                const auto computeGainDb = [] (float levelDb, float thresholdDb, float ratio, float kneeDb)
                {
                    const auto over = levelDb - thresholdDb;
                    if (over <= -kneeDb * 0.5f)
                        return 0.0f;
                    if (over < kneeDb * 0.5f)
                    {
                        const auto x = over + kneeDb * 0.5f;
                        return (1.0f / ratio - 1.0f) * x * x / (2.0f * kneeDb);
                    }
                    return (1.0f / ratio - 1.0f) * over;
                };
                const auto last = numChannels - 1;
                numSamples = juce::jmin (numSamples, bands.getNumSamples());

                if (comp == Comp::squash || comp == Comp::maximizer)
                {
                    // Squash: fast and hard, no look-ahead. Maximizer: a slower,
                    // smoother compressor then a limiter that never lets a peak
                    // past -0.5 dBFS. (The original Maximizer looks 3 ms ahead;
                    // ours doesn't, so it adds no latency.)
                    const auto squash = comp == Comp::squash;
                    const auto thresholdDb = squash ? -36.0f * amount : -30.0f * amount;
                    const auto ratio = squash ? 4.0f + 16.0f * amount : 4.0f;
                    const auto makeUpDb = squash ? -thresholdDb * (1.0f - 1.0f / ratio) * 0.6f : 12.0f * amount;
                    const auto attack = coeff (squash ? 0.3f : 10.0f), release = coeff (squash ? 80.0f : 150.0f);
                    const auto ceiling = juce::Decibels::decibelsToGain (-0.5f), limiterRelease = coeff (50.0f);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto peak = juce::jmax (std::abs (chans[0][i]), std::abs (chans[last][i]));
                        const auto levelDb = juce::Decibels::gainToDecibels (peak, -120.0f);
                        auto& env = envelopeDb;
                        env = levelDb > env ? levelDb + (env - levelDb) * attack : levelDb + (env - levelDb) * release;
                        auto gain = juce::Decibels::decibelsToGain (computeGainDb (env, thresholdDb, ratio, 6.0f) + makeUpDb);
                        if (! squash)
                        {
                            const auto out = peak * gain;
                            const auto wanted = out > ceiling ? ceiling / out : 1.0f;
                            limiterGain = wanted < limiterGain ? wanted : wanted + (limiterGain - wanted) * limiterRelease;
                            gain *= limiterGain;
                        }
                        for (int c = 0; c < numChannels; ++c)
                            chans[c][i] *= gain;
                    }
                    return;
                }

                // Multiband: three bands (crossovers at 200 Hz and 2.5 kHz,
                // our choice), each compressed hard. Overkill: the same bands
                // brought up as well as down to one level, so the quiet parts,
                // noise and all, come right up (the manual's "filthy artefacts").
                const auto overkill = comp == Comp::overkill;
                const auto attack = coeff (overkill ? 1.0f : 2.0f), release = coeff (overkill ? 30.0f : 100.0f);
                const auto thresholdDb = overkill ? -6.0f - 30.0f * amount : -40.0f * amount;
                const auto ratio = overkill ? 10.0f : 6.0f;
                const auto makeUpDb = overkill ? 0.0f : -thresholdDb * (1.0f - 1.0f / ratio) * 0.5f;
                for (int c = 0; c < numChannels; ++c)
                    for (int i = 0; i < numSamples; ++i)
                    {
                        float lowBand = 0.0f, rest = 0.0f, mid = 0.0f, high = 0.0f;
                        split1.processSample (c, chans[c][i], lowBand, rest);
                        split2.processSample (c, rest, mid, high);
                        bands.setSample (c * 3, i, allpass1.processSample (c, lowBand));
                        bands.setSample (c * 3 + 1, i, mid);
                        bands.setSample (c * 3 + 2, i, high);
                    }
                for (int i = 0; i < numSamples; ++i)
                {
                    float out[2] {};
                    for (int b = 0; b < 3; ++b)
                    {
                        auto peak = 0.0f;
                        for (int c = 0; c < numChannels; ++c)
                            peak = juce::jmax (peak, std::abs (bands.getSample (c * 3 + b, i)));
                        const auto levelDb = juce::Decibels::gainToDecibels (peak, -120.0f);
                        auto& env = bandDb[(size_t) b];
                        env = levelDb > env ? levelDb + (env - levelDb) * attack : levelDb + (env - levelDb) * release;
                        const auto gainDb = overkill ? juce::jlimit (-40.0f, 30.0f, (thresholdDb - env) * (1.0f - 1.0f / ratio))   // up and down
                                                     : computeGainDb (env, thresholdDb, ratio, 6.0f) + makeUpDb;
                        const auto gain = juce::Decibels::decibelsToGain (gainDb);
                        for (int c = 0; c < numChannels; ++c)
                            out[c] += bands.getSample (c * 3 + b, i) * gain;
                    }
                    for (int c = 0; c < numChannels; ++c)
                        chans[c][i] = overkill ? softClip (out[c]) : out[c];
                }
            }
        };
    }

    //==========================================================================
    double Cipher::speedBeats (int speed) noexcept
    {
        // Matches speedNames: one pass of the envelope, in quarter notes.
        constexpr double beats[] = { 32.0, 16.0, 8.0, 4.0, 3.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0,
                                     0.375, 0.25, 1.0 / 6.0, 0.125 };
        return beats[juce::jlimit (0, (int) std::size (beats) - 1, speed)];
    }

    Cipher::Labels Cipher::labelsFor (Type type) noexcept
    {
        switch (type)
        {
            case Type::ladderLp: case Type::ladderBp: case Type::ladderHp:
            case Type::digitalLp: case Type::digitalBp: case Type::digitalHp:
                return { "CUTOFF", "RESONANCE", "", true };
            case Type::oneOhOneLp: case Type::oneOhOneBp: case Type::oneOhOneHp:
                return { "CUTOFF", "RESONANCE", "DRIVE", false };
            case Type::model20Lp: case Type::model20Bp: case Type::model20Hp:
                return { "CUTOFF", "RESONANCE", "DRIVE", true };
            case Type::notch:      return { "CUTOFF", "WIDTH", "", false };
            case Type::dualFilter: return { "CUTOFF", "RESONANCE", "BANDWIDTH", false };
            case Type::formant:    return { "VOWEL", "Q", "SHIFT", false };
            case Type::eqBell: case Type::eqLowShelf: case Type::eqHighShelf:
                return { "FREQUENCY", "GAIN", "Q", false };
            case Type::gain:       return { "GAIN", "", "", false };
            case Type::pan:        return { "POSITION", "WIDTH", "", false, "STEREO", &panModeNames };
            case Type::comb:       return { "SPEED", "RESONANCE", "BRIGHTNESS", false };
            case Type::phaser:     return { "FREQUENCY", "FEEDBACK", "SPREAD", false, "STEREO", &phaserModeNames };
            case Type::pitchShift: return { "SHIFT", "GRAIN FREQ", "JITTER", false };
            case Type::freqShift:  return { "SHIFT", "", "", false };
            case Type::ringMod:    return { "FREQUENCY", "AMOUNT", "CHARACTER", false };
            case Type::fm:         return { "FREQUENCY", "CHARACTER", "AMOUNT", false };
            case Type::distort:    return { "DRIVE", "HP", "LP", false, "STEREO", &driveModeNames };
            case Type::decimate:   return { "FREQUENCY", "DIRT", "", false };
            case Type::delay:      return { "TIME", "FEEDBACK", "TONE", false, "KIND", &delayModeNames };
            case Type::reverb:     return { "TIME", "TONE", "HPF", false, "STEREO", &reverbModeNames };
            case Type::reflect:    return { "SIZE", "ABSORB", "FEEDBACK", false };
            case Type::vocoder:    return { "FREQUENCY", "FORMANT", "RESONANCE", false, "CHARACTER" };
            case Type::loop:       return { "FREQUENCY", "TEMPO", "SMOOTH", false, "STEREO", &loopModeNames };
            case Type::stretch:    return { "SPEED", "TEMPO", "GRAIN RATE", false };
            case Type::varispeed:  return { "SPEED", "TEMPO", "", false, "STEREO", &varispeedModeNames };
            case Type::slow:       return { "SPEED", "TEMPO", "", false };
            case Type::reverse:    return { "SPEED", "TEMPO", "", false };
            case Type::gate:       return { "THRESHOLD", "HOLD", "RELEASE", false, "HPF" };
            case Type::compress:   return { "COMPRESS", "", "", false, "STEREO", &compModeNames };
            case Type::off:        break;
        }
        return { "", "", "", false };
    }

    int Cipher::modeOf (Type type, int option) noexcept
    {
        const auto* modes = labelsFor (type).modes;
        if (modes == nullptr)
            return 0;
        const auto count = modes->size();
        return ((option - 3) % count + count) % count;
    }

    int Cipher::optionFor (Type type, int mode) noexcept
    {
        const auto* modes = labelsFor (type).modes;
        return modes == nullptr ? 3 : (mode + 3) % modes->size();
    }

    float Cipher::shiftHz (float main01) noexcept
    {
        const auto x = (juce::jlimit (0.0f, 1.0f, main01) - 0.5f) * 2.0f;
        return (x < 0.0f ? -1.0f : 1.0f) * x * x * 2000.0f;
    }

    float Cipher::gainUtilityDb (float main01) noexcept
    {
        // Centre is 0 dB; up to +12 dB at the top; below the centre the level
        // falls in a straight line to silence (so an envelope can duck or chop
        // the sound right down). Our choice.
        main01 = juce::jlimit (0.0f, 1.0f, main01);
        if (main01 >= 0.5f)
            return (main01 - 0.5f) * 24.0f;
        return juce::Decibels::gainToDecibels (2.0f * main01, -100.0f);
    }

    juce::String Cipher::mainText (Type type, float main01, int option)
    {
        const auto hz = [] (float f) { return f < 1000.0f ? juce::String (f < 10.0f ? f : (float) juce::roundToInt (f), f < 10.0f ? 1 : 0) + " Hz"
                                                          : juce::String (f / 1000.0f, 2) + " kHz"; };
        const auto ms = [] (float t) { return t < 1000.0f ? juce::String (juce::roundToInt (t)) + " ms" : juce::String (t / 1000.0f, 2) + " s"; };
        const auto mode = modeOf (type, option);
        if (type == Type::formant)
        {
            const char* names[] = { "A", "E", "I", "O", "U" };
            return names[juce::jlimit (0, 4, juce::roundToInt (main01 * 4.0f))];
        }
        if (type == Type::gain)
        {
            const auto db = gainUtilityDb (main01);
            return db <= -99.0f ? juce::String ("-inf") : juce::String (db, 1) + " dB";
        }
        if (type == Type::pan)
        {
            const auto p = juce::roundToInt ((main01 - 0.5f) * 200.0f);
            return p == 0 ? juce::String ("C") : juce::String (std::abs (p)) + (p < 0 ? "L" : "R");
        }
        if (type == Type::pitchShift)
        {
            const auto st = semitones (main01);
            return (st > 0.0f ? "+" : "") + juce::String (st, 1) + " st";
        }
        if (type == Type::freqShift)
        {
            const auto f = shiftHz (main01);
            return (f > 0.0f ? "+" : "") + juce::String (juce::roundToInt (f)) + " Hz";
        }
        if (type == Type::delay)
            return mode == 0 ? speedNames[16 - noteStep (main01, 14)] : ms (freeDelayMs (main01));
        if (type == Type::reverb)
            return juce::String (reverbSeconds (main01), 1) + " s";
        if (type == Type::loop)
            return mode == 1 ? noteNames[noteStep (main01, 7)] : hz (loopHz (main01));
        if (type == Type::stretch)
            return juce::String (juce::roundToInt ((0.05f + 0.95f * main01) * 100.0f)) + "%";
        if (type == Type::varispeed)
        {
            if (mode == 1)
                return main01 < 0.5f ? "-" + juce::String (juce::roundToInt ((0.5f - main01) * 200.0f)) + "%"
                                     : juce::String (juce::roundToInt ((main01 - 0.5f) * 200.0f)) + "%";
            return juce::String (juce::roundToInt ((0.05f + 0.95f * main01) * 100.0f)) + "%";
        }
        if (type == Type::slow || type == Type::reverse)
            return slowNames[noteStep (main01, 5)];
        if (type == Type::distort || type == Type::compress)
            return juce::String (juce::roundToInt (main01 * 100.0f)) + "%";
        if (type == Type::decimate)
            return hz (100.0f * std::pow (480.0f, juce::jlimit (0.0f, 1.0f, main01)));
        if (type == Type::reflect)
            return juce::String (1.0f + 59.0f * main01 * main01, 1) + " m";
        if (type == Type::vocoder)
            return hz (30.0f * std::pow (33.0f, juce::jlimit (0.0f, 1.0f, main01)));
        if (type == Type::gate)
            return juce::String (juce::roundToInt (gateThresholdDb (main01))) + " dB";
        if (type == Type::off)
            return "-";
        return hz (cutoffHz (main01));
    }

    juce::String Cipher::p1Text (Type type, float p)
    {
        if (type == Type::eqBell || type == Type::eqLowShelf || type == Type::eqHighShelf)
            return juce::String (eqGainDb (p), 1) + " dB";
        if (type == Type::pan)
            return juce::String (juce::roundToInt (p * 200.0f)) + "%";
        if (type == Type::pitchShift)
            return juce::String (juce::roundToInt (2.0f + p * 98.0f)) + " Hz";
        if (type == Type::distort)
            return juce::String (juce::roundToInt (20.0f * std::pow (100.0f, p))) + " Hz";
        if (type == Type::loop || type == Type::stretch || type == Type::varispeed || type == Type::slow || type == Type::reverse)
            return noteNames[noteStep (p, 6)];
        if (type == Type::vocoder)
        {
            const auto st = juce::roundToInt ((p - 0.5f) * 24.0f);
            return (st > 0 ? "+" : "") + juce::String (st) + " st";
        }
        if (type == Type::gate)
            return juce::String (juce::roundToInt (p * 500.0f)) + " ms";
        return juce::String (juce::roundToInt (p * 100.0f)) + "%";
    }

    juce::String Cipher::p2Text (Type type, float p)
    {
        if (type == Type::formant)
        {
            const auto hz = juce::roundToInt ((p - 0.5f) * 1000.0f);
            return (hz > 0 ? "+" : "") + juce::String (hz) + " Hz";
        }
        if (type == Type::dualFilter)
            return juce::String (p * 3.0f, 1) + " oct";
        if (type == Type::eqBell || type == Type::eqLowShelf || type == Type::eqHighShelf)
            return juce::String (eqQ (p), 2);
        if (type == Type::distort)
        {
            const auto f = 500.0f * std::pow (40.0f, p);
            return f < 1000.0f ? juce::String (juce::roundToInt (f)) + " Hz" : juce::String (f / 1000.0f, 1) + " kHz";
        }
        if (type == Type::reverb)
            return juce::String (juce::roundToInt (20.0f * std::pow (100.0f, p))) + " Hz";
        if (type == Type::stretch)
            return juce::String (juce::roundToInt (4.0f * std::pow (25.0f, p))) + " Hz";
        if (type == Type::gate)
        {
            const auto t = 5.0f * std::pow (400.0f, p);
            return t < 1000.0f ? juce::String (juce::roundToInt (t)) + " ms" : juce::String (t / 1000.0f, 1) + " s";
        }
        if (type == Type::delay)
            return p < 0.49f ? "DARK " + juce::String (juce::roundToInt ((0.5f - p) * 200.0f)) + "%"
                 : p > 0.51f ? "BRIGHT " + juce::String (juce::roundToInt ((p - 0.5f) * 200.0f)) + "%" : juce::String ("FLAT");
        return juce::String (juce::roundToInt (p * 100.0f)) + "%";
    }

    juce::String Cipher::stereoText (Type type, float stereo)
    {
        if (type == Type::delay)
            return delayStereoNames[delayStereoKind (stereo)];
        if (type == Type::vocoder)
        {
            const auto c = (stereo + 1.0f) * 0.5f;
            return c < 0.2f ? juce::String ("SAW") : c < 0.6f ? juce::String ("SQUARE") : juce::String ("PULSE");
        }
        if (type == Type::gate)
            return stereo <= -0.99f ? juce::String ("OFF") : juce::String (juce::roundToInt (20.0f * std::pow (100.0f, (stereo + 1.0f) * 0.5f))) + " Hz";
        return juce::String (juce::roundToInt (stereo * 100.0f)) + "%";
    }

    //==========================================================================
    float Cipher::envelopeAt (const Envelope& points, float x) noexcept
    {
        if (points.empty())
            return 0.5f;
        if (x <= points.front().x)
            return points.front().y;
        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            const auto& a = points[i];
            const auto& b = points[i + 1];
            if (x <= b.x)
            {
                const auto span = b.x - a.x;
                if (span <= 1.0e-6f)
                    return b.y;   // a vertical step
                auto t = (x - a.x) / span;
                // CURVE bends the line: t^(2^-3c), so 0 is straight and
                // +-1 bow it well up or down (our law).
                if (std::abs (a.curve) > 1.0e-3f)
                    t = std::pow (t, std::pow (2.0f, -3.0f * a.curve));
                return a.y + (b.y - a.y) * t;
            }
        }
        return points.back().y;
    }

    Cipher::Envelope Cipher::tidy (Envelope points)
    {
        for (auto& p : points)
        {
            p.x = juce::jlimit (0.0f, 1.0f, p.x);
            p.y = juce::jlimit (0.0f, 1.0f, p.y);
            p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
        }
        std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
        if ((int) points.size() > maxPoints)
            points.resize ((size_t) maxPoints);
        if (points.empty())
            points.push_back ({ 0.0f, 0.5f, 0.0f });
        return points;
    }

    Cipher::Envelope Cipher::shape (int index)
    {
        switch (index)
        {
            case 1:  return { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
            case 2:  return { { 0.0f, 1.0f }, { 1.0f, 0.0f } };
            case 3:  return { { 0.0f, 0.0f }, { 0.5f, 1.0f }, { 1.0f, 0.0f } };
            case 4:  return { { 0.0f, 0.0f, 0.6f }, { 0.5f, 1.0f, -0.6f }, { 1.0f, 0.0f } };
            case 5:  return { { 0.0f, 1.0f }, { 0.5f, 1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f } };
            case 6:  return { { 0.0f, 0.25f }, { 0.25f, 0.25f }, { 0.25f, 0.75f }, { 0.5f, 0.75f }, { 0.5f, 0.0f }, { 0.75f, 0.0f },
                              { 0.75f, 1.0f }, { 1.0f, 1.0f } };
            case 7:  return { { 0.0f, 1.0f, -0.8f }, { 0.35f, 0.1f }, { 1.0f, 0.0f } };
            default: return { { 0.0f, 0.5f }, { 1.0f, 0.5f } };
        }
    }

    juce::String Cipher::toText (const Envelope& points)
    {
        juce::StringArray parts;
        for (const auto& p : points)
            parts.add (juce::String (p.x, 4) + " " + juce::String (p.y, 4) + " " + juce::String (p.curve, 4));
        return parts.joinIntoString (";");
    }

    Cipher::Envelope Cipher::fromText (const juce::String& text)
    {
        Envelope points;
        for (const auto& part : juce::StringArray::fromTokens (text, ";", {}))
        {
            const auto numbers = juce::StringArray::fromTokens (part.trim(), " ", {});
            if (numbers.size() >= 2)
                points.push_back ({ numbers[0].getFloatValue(), numbers[1].getFloatValue(), numbers.size() > 2 ? numbers[2].getFloatValue() : 0.0f });
        }
        return tidy (points);
    }

    //==========================================================================
    struct Cipher::SlotState
    {
        double phase = 0.0;
        float glided = -1.0f;   // the main knob as moved, after GLIDE (-1: not started)

        // Per channel: the filter states and this tick's coefficients.
        struct Channel
        {
            float ladder[4] {};
            float svf[3][2] {};
            float pole = 0.0f;
            // Coefficients
            float g = 0.0f, G = 0.0f, k = 0.0f;   // the ladder and the one-pole
            Svf s[3];
            float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;   // the EQ's mix of the SVF's outputs
            float driveGain = 1.0f, makeUp = 1.0f, level = 1.0f;
            float gains[3] { 1.0f, 1.0f, 1.0f };
        };
        std::array<Channel, 2> ch;

        void reset()
        {
            for (auto& c : ch)
            {
                std::fill (std::begin (c.ladder), std::end (c.ladder), 0.0f);
                for (auto& s : c.svf)
                    s[0] = s[1] = 0.0f;
                c.pole = 0.0f;
            }
            glided = -1.0f;
        }
    };

    struct Cipher::MasterState
    {
        juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };   // 4x for the drive
        float toneState[2] {}, dcIn[2] {}, dcOut[2] {};
        Compressor compressor;
        juce::dsp::LinkwitzRileyFilter<float> bassDry, bassWet;           // CLEAN BASS
    };

    //==========================================================================
    // A slot's time effects: one 4-second memory per side that every
    // buffer-based effect uses (Comb, FM, Delay, Reflect, Pitch Shift, Loop,
    // Stretch, Varispeed, Slow, Reverse), and each effect's own parts.
    struct Cipher::TimeState
    {
        static constexpr double memorySeconds = 4.0;
        static constexpr int numLines = 8, numDiffusers = 6, numBands = 16;

        double sampleRate = 48000.0;
        int size = 1, write = 0;
        std::array<std::vector<float>, 2> memory;

        // The capture clock (Loop, Stretch, Varispeed, Slow, Reverse: how often
        // a new part of the sound is taken).
        long long lastGrid = -1;
        double counter = 0.0;
        bool captured = false;
        Type capturedFor = Type::off;

        // Comb, Phaser, Distort, Decimate, Delay
        float tone[2] {}, tone2[2] {}, hp[2] {}, last[2] {}, held[2] {};
        float allPass[2][12] {};
        double holdPhase = 0.0;
        float decimateSvf[2][2] {};
        float delayNow[2] { -1.0f, -1.0f };
        // Pitch Shift: two grains per side
        double grainPhase[2][2] {};
        float grainLength[2][2] {};
        juce::Random random { 2002 };
        // Freq Shift: two all-pass chains per side make a 90-degree pair
        float hilbertA[2][4][4] {}, hilbertB[2][4][4] {}, hilbertDelay[2] {};
        double oscillator = 0.0;
        // Reverb
        std::array<std::vector<float>, numLines> lines;
        std::array<int, numLines> lineWrite {};
        std::array<float, numLines> lineDamp {};
        std::array<std::vector<float>, numDiffusers> diffusers;
        std::array<int, numDiffusers> diffuserWrite {};
        float reverbHp[2] {}, crushHeld[2] {};
        int crushCount = 0;
        // Vocoder
        float vocoderModulator[2][numBands][2] {}, vocoderCarrier[2][numBands][2] {}, vocoderLevel[2][numBands] {};
        // Loop, Stretch, Varispeed, Slow, Reverse
        double loopStart = 0.0, loopPosition = 0.0;
        double readPosition = 0.0, oldReadPosition = 0.0, grainStart[2] {}, grainHead[2] {};
        int fade = 0;
        // Gate
        float gateLevel = 0.0f, gateGain = 1.0f, gateSide[2] {};
        bool gateOpen = true;
        int gateHold = 0;
        Compressor compressor;

        void prepare (double rate, int maxBlockSize)
        {
            sampleRate = rate;
            size = (int) (memorySeconds * rate) + 8;
            for (auto& m : memory)
                m.assign ((size_t) size, 0.0f);
            for (auto& line : lines)
                line.assign ((size_t) (0.16 * rate) + 4, 0.0f);
            for (auto& d : diffusers)
                d.assign ((size_t) (0.12 * rate) + 4, 0.0f);
            compressor.prepare (rate, maxBlockSize);
            reset();
        }

        void reset()
        {
            for (auto& m : memory)
                std::fill (m.begin(), m.end(), 0.0f);
            for (auto& line : lines)
                std::fill (line.begin(), line.end(), 0.0f);
            for (auto& d : diffusers)
                std::fill (d.begin(), d.end(), 0.0f);
            write = 0;
            lastGrid = -1;
            counter = 0.0;
            captured = false;
            capturedFor = Type::off;
            for (int c = 0; c < 2; ++c)
            {
                tone[c] = tone2[c] = hp[c] = last[c] = held[c] = reverbHp[c] = crushHeld[c] = gateSide[c] = hilbertDelay[c] = 0.0f;
                delayNow[c] = -1.0f;
                std::fill (std::begin (allPass[c]), std::end (allPass[c]), 0.0f);
                decimateSvf[c][0] = decimateSvf[c][1] = 0.0f;
                for (int g = 0; g < 2; ++g)
                {
                    grainPhase[c][g] = g * 0.5;
                    grainLength[c][g] = 0.0f;
                }
                for (auto& stage : hilbertA[c]) std::fill (std::begin (stage), std::end (stage), 0.0f);
                for (auto& stage : hilbertB[c]) std::fill (std::begin (stage), std::end (stage), 0.0f);
                for (int b = 0; b < numBands; ++b)
                {
                    vocoderModulator[c][b][0] = vocoderModulator[c][b][1] = 0.0f;
                    vocoderCarrier[c][b][0] = vocoderCarrier[c][b][1] = 0.0f;
                    vocoderLevel[c][b] = 0.0f;
                }
            }
            lineWrite.fill (0);
            lineDamp.fill (0.0f);
            diffuserWrite.fill (0);
            gateLevel = 0.0f;
            gateGain = 1.0f;
            gateOpen = true;
            gateHold = 0;
            fade = 0;
            compressor.reset();
        }

        // The memory, `back` samples before the newest (fractional, linear).
        float read (int c, double back) const noexcept
        {
            auto position = (double) write - 1.0 - back;
            position -= std::floor (position / size) * size;
            const auto i0 = (int) position;
            const auto frac = (float) (position - i0);
            const auto i1 = i0 + 1 < size ? i0 + 1 : 0;
            return memory[(size_t) c][(size_t) i0] + (memory[(size_t) c][(size_t) i1] - memory[(size_t) c][(size_t) i0]) * frac;
        }
        // The memory at an absolute place (as counted by `write`).
        float at (int c, double position) const noexcept
        {
            position -= std::floor (position / size) * size;
            const auto i0 = (int) position;
            const auto frac = (float) (position - i0);
            const auto i1 = i0 + 1 < size ? i0 + 1 : 0;
            return memory[(size_t) c][(size_t) i0] + (memory[(size_t) c][(size_t) i1] - memory[(size_t) c][(size_t) i0]) * frac;
        }
        // Absolute place of the newest sample, counting on past wraps.
        double now = 0.0;
    };

    Cipher::Cipher()
    {
        for (auto& s : slotStates)
            s = std::make_unique<SlotState>();
        for (auto& t : timeStates)
            t = std::make_unique<TimeState>();
        master = std::make_unique<MasterState>();
        for (int i = 0; i < numSlots; ++i)
        {
            envelopes[(size_t) i] = shape (0);
            for (auto& v : tables[(size_t) i])
                v = 0.5f;
            phases[(size_t) i] = 0.0f;
            moved[(size_t) i] = 0.5f;
        }
    }

    Cipher::~Cipher() = default;

    void Cipher::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        fade.prepare (sampleRate, maxBlockSize);
        dry.setSize (2, maxBlockSize);
        low.setSize (2, maxBlockSize);
        master->oversampling.initProcessing ((size_t) maxBlockSize);
        maxBlock = maxBlockSize;
        master->compressor.prepare (sampleRate, maxBlockSize);
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        for (auto* f : { &master->bassDry, &master->bassWet })
            f->prepare (spec);
        master->bassDry.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        master->bassWet.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        for (auto& t : timeStates)
            t->prepare (sampleRate, maxBlockSize);
        reset();
    }

    void Cipher::reset()
    {
        for (auto& s : slotStates)
        {
            s->reset();
            s->phase = 0.0;
        }
        master->oversampling.reset();
        for (auto* f : { &master->bassDry, &master->bassWet })
            f->reset();
        master->compressor.reset();
        for (auto& t : timeStates)
            t->reset();
        for (int c = 0; c < 2; ++c)
            master->toneState[c] = master->dcIn[c] = master->dcOut[c] = 0.0f;
    }

    void Cipher::setEnvelope (int slot, const Envelope& points)
    {
        if (slot < 0 || slot >= numSlots)
            return;
        auto& kept = envelopes[(size_t) slot];
        kept = tidy (points);
        auto& table = tables[(size_t) slot];
        for (int i = 0; i < tableSize; ++i)
            table[(size_t) i].store (envelopeAt (kept, (float) i / (float) (tableSize - 1)));
    }

    //==========================================================================
    void Cipher::process (juce::dsp::AudioBlock<float> block, const Settings& settings)
    {
        fade.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            const auto numSamples = (int) wet.getNumSamples();
            const auto numChannels = (int) juce::jmin ((size_t) 2, wet.getNumChannels());
            const auto& m = settings.master;
            for (int c = 0; c < numChannels; ++c)
                dry.copyFrom (c, 0, wet.getChannelPointer ((size_t) c), numSamples);

            //------------------------------------------------------------------
            // The master drive: 4x oversampled so its harmonics don't fold back
            // down; TONE tilts the result around 800 Hz.
            const auto runDrive = [&]
            {
                if (! m.driveOn)
                    return;
                const auto pre = juce::Decibels::decibelsToGain (driveGainDb (m.drive));
                const auto type = m.driveType;
                // Level matching: roughly the inverse of how much louder each gets (ours).
                const auto makeUp = 1.0f / std::sqrt (pre) * (type == 3 ? 1.1f : 1.0f);
                auto sub = wet.getSubsetChannelBlock (0, (size_t) numChannels);
                auto up = master->oversampling.processSamplesUp (sub);
                for (size_t c = 0; c < up.getNumChannels(); ++c)
                {
                    auto* x = up.getChannelPointer (c);
                    for (size_t i = 0; i < up.getNumSamples(); ++i)
                    {
                        const auto in = x[i] * pre;
                        const auto y = driveShape (type, in);
                        x[i] = y * makeUp;
                    }
                }
                master->oversampling.processSamplesDown (sub);
                const auto tilt = (m.tone - 0.5f) * 2.0f;   // -1 dark to +1 bright: +-9 dB either side of 800 Hz
                const auto lowGain = juce::Decibels::decibelsToGain (-9.0f * tilt), highGain = juce::Decibels::decibelsToGain (9.0f * tilt);
                const auto a = 1.0f - std::exp (-2.0f * pi * 800.0f / (float) sampleRate);
                const auto dcPole = 1.0f - 2.0f * pi * 15.0f / (float) sampleRate;
                for (int c = 0; c < numChannels; ++c)
                {
                    auto* x = wet.getChannelPointer ((size_t) c);
                    auto& state = master->toneState[c];
                    for (int i = 0; i < numSamples; ++i)
                    {
                        // A DC blocker first (Rectify and Tube leave an offset).
                        const auto in = x[i];
                        const auto blocked = in - master->dcIn[c] + dcPole * master->dcOut[c];
                        master->dcIn[c] = in;
                        master->dcOut[c] = blocked;
                        state += a * (blocked - state);
                        x[i] = state * lowGain + (blocked - state) * highGain;
                    }
                }
            };

            if (! m.driveAfter)
                runDrive();
            for (int s = 0; s < numSlots; ++s)
                processSlot (s, wet, settings.slots[(size_t) s], settings);
            if (m.driveAfter)
                runDrive();

            //------------------------------------------------------------------
            // The compressor, linked across both sides.
            if (m.compOn)
            {
                float* chans[2] = { wet.getChannelPointer (0), wet.getChannelPointer ((size_t) (numChannels - 1)) };
                master->compressor.process (chans, numChannels, numSamples, m.comp, m.compress);
            }

            //------------------------------------------------------------------
            // CLEAN BASS: below its frequency the dry sound, above it CIPHER's
            // (Linkwitz-Riley halves, so with nothing in between they add back flat).
            if (m.cleanBassOn)
            {
                master->bassDry.setCutoffFrequency (juce::jlimit (20.0f, 1000.0f, m.cleanBassHz));
                master->bassWet.setCutoffFrequency (juce::jlimit (20.0f, 1000.0f, m.cleanBassHz));
                for (int c = 0; c < numChannels; ++c)
                {
                    auto* x = wet.getChannelPointer ((size_t) c);
                    const auto* d = dry.getReadPointer (c);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto lowPart = master->bassDry.processSample (c, d[i]);
                        const auto highPart = master->bassWet.processSample (c, x[i]);
                        x[i] = m.cleanBassListen ? lowPart : lowPart + highPart;
                    }
                }
            }

            //------------------------------------------------------------------
            // OUTPUT: GAIN, then MIX with the sound that came in.
            const auto gain = juce::Decibels::decibelsToGain (m.gainDb);
            const auto mix = juce::jlimit (0.0f, 1.0f, m.mix);
            float wetGain = mix, dryGain = 1.0f - mix;
            if (m.mixLaw == 1)
            {
                wetGain = std::sin (mix * pi * 0.5f);
                dryGain = std::cos (mix * pi * 0.5f);
            }
            else if (m.mixLaw == 2)
            {
                wetGain = juce::jmin (1.0f, 2.0f * mix);
                dryGain = juce::jmin (1.0f, 2.0f * (1.0f - mix));
            }
            for (int c = 0; c < numChannels; ++c)
            {
                auto* x = wet.getChannelPointer ((size_t) c);
                const auto* d = dry.getReadPointer (c);
                for (int i = 0; i < numSamples; ++i)
                    x[i] = x[i] * gain * wetGain + d[i] * dryGain;
            }
        });
    }

    //==========================================================================
    void Cipher::processSlot (int index, juce::dsp::AudioBlock<float>& block, const Slot& slot, const Settings& settings)
    {
        auto& st = *slotStates[(size_t) index];
        const auto numSamples = (int) block.getNumSamples();
        const auto numChannels = (int) juce::jmin ((size_t) 2, block.getNumChannels());

        // The LFO runs whether or not the slot sounds, so the envelope keeps its place.
        const auto beatsPerSecond = settings.bpm / 60.0;
        const auto cycleHz = slot.sync ? beatsPerSecond / speedBeats (slot.speed) : (double) juce::jlimit (0.01f, 40.0f, slot.rateHz);
        const auto locked = slot.sync && settings.songBeats >= 0.0;   // PHASE LOCK: synced to the song's position while it plays
        const auto phaseAt = [&] (int offset)
        {
            if (locked)
            {
                const auto beats = settings.songBeats + (double) offset / sampleRate * beatsPerSecond;
                const auto cycle = beats / speedBeats (slot.speed);
                return cycle - std::floor (cycle);
            }
            return st.phase;
        };

        const auto type = slot.on ? slot.type : Type::off;
        auto& table = tables[(size_t) index];
        const auto glideCoeff = slot.glideMs > 0.5f ? std::exp (-(double) tick / (0.001 * slot.glideMs * sampleRate)) : 0.0;

        for (int start = 0; start < numSamples; start += tick)
        {
            const auto count = juce::jmin (tick, numSamples - start);

            // The envelope at this point of its cycle moves the main knob.
            const auto phase = (float) phaseAt (start);
            const auto position = phase * (float) (tableSize - 1);
            const auto i0 = juce::jlimit (0, tableSize - 2, (int) position);
            const auto frac = position - (float) i0;
            const auto env = table[(size_t) i0].load() + (table[(size_t) i0 + 1].load() - table[(size_t) i0].load()) * frac;
            float target = slot.main;
            switch (slot.style)
            {
                case Style::both: target += slot.envAmount * (env - 0.5f); break;
                case Style::up:   target += slot.envAmount * env; break;
                case Style::down: target -= slot.envAmount * env; break;
            }
            target = juce::jlimit (0.0f, 1.0f, target);
            if (st.glided < 0.0f || glideCoeff <= 0.0)
                st.glided = target;
            else
                st.glided = (float) (target + (st.glided - target) * glideCoeff);
            phases[(size_t) index] = phase;
            moved[(size_t) index] = st.glided;

            if (! locked)
            {
                st.phase += cycleHz * (double) count / sampleRate;
                st.phase -= std::floor (st.phase);
            }

            if (type == Type::off)
                continue;

            if (isTimeEffect (type))
            {
                // STEREO moves the two sides' main knob apart, except where an
                // effect uses the knob for something else.
                const auto ownStereo = type == Type::delay || type == Type::vocoder || type == Type::gate || type == Type::freqShift
                                    || type == Type::reverb;
                float mains[2];
                for (int c = 0; c < 2; ++c)
                {
                    const auto offset = numChannels > 1 && ! ownStereo ? (c == 0 ? -1.0f : 1.0f) * slot.stereo * 0.1f : 0.0f;
                    mains[c] = juce::jlimit (0.0f, 1.0f, st.glided + offset);
                }
                processTime (index, block, start, count, slot, settings, mains);
                continue;
            }

            //------------------------------------------------------------------
            // This tick's coefficients, for each side (STEREO moves them apart).
            const auto slope = juce::jlimit (0, 3, slot.option);
            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = st.ch[(size_t) c];
                const auto offset = numChannels > 1 ? (c == 0 ? -1.0f : 1.0f) * slot.stereo * 0.1f : 0.0f;
                const auto mainHere = juce::jlimit (0.0f, 1.0f, st.glided + offset);
                const auto hz = cutoffHz (mainHere);
                const auto res = juce::jlimit (0.0f, 1.0f, slot.p1);
                const auto p2 = juce::jlimit (0.0f, 1.0f, slot.p2);
                ch.g = prewarp (hz, sampleRate);
                ch.G = ch.g / (1.0f + ch.g);
                ch.driveGain = 1.0f;
                ch.makeUp = 1.0f;
                switch (type)
                {
                    case Type::ladderLp: case Type::ladderBp: case Type::ladderHp:
                        // A diode ladder's gentle curve: resonance stops short
                        // of whistling, and the bass lost to it is partly made up.
                        ch.k = res * 3.2f;
                        ch.makeUp = 1.0f + 0.5f * ch.k;
                        break;
                    case Type::oneOhOneLp: case Type::oneOhOneBp: case Type::oneOhOneHp:
                        // The classic 24 dB ladder: a resonance that sings (it
                        // whistles at the top), and DRIVE into it.
                        ch.k = res * 4.2f;
                        ch.driveGain = juce::Decibels::decibelsToGain (p2 * p2 * 30.0f);
                        ch.makeUp = (1.0f + 0.35f * ch.k) / std::sqrt (ch.driveGain);
                        break;
                    case Type::digitalLp: case Type::digitalBp: case Type::digitalHp:
                    {
                        // Clean and steep: Butterworth stages, a soft resonance on the last.
                        const auto q = 0.707f + res * 2.3f;
                        if (slope == 3)
                        {
                            ch.s[0].set (ch.g, 0.541f);
                            ch.s[1].set (ch.g, 1.307f + res * 2.3f);
                        }
                        else
                            ch.s[0].set (ch.g, q);
                        break;
                    }
                    case Type::model20Lp: case Type::model20Bp: case Type::model20Hp:
                    {
                        // A screaming two-pole with its resonance clipped (so it
                        // growls rather than runs away); the steeper settings add
                        // a second resonant stage lower down, the manual's
                        // "multiple resonant points down the filter curve".
                        const auto q = 0.6f + res * res * 14.0f;
                        ch.s[0].set (ch.g, q);
                        ch.s[1].set (prewarp (hz * 0.62f, sampleRate), 0.6f + res * res * 5.0f);
                        ch.driveGain = juce::Decibels::decibelsToGain (p2 * p2 * 30.0f);
                        ch.makeUp = 1.0f / std::sqrt (ch.driveGain);
                        break;
                    }
                    case Type::notch:
                        ch.s[0].set (ch.g, 8.0f * std::pow (0.05f, res));   // WIDTH: narrow (Q 8) to wide (Q 0.4)
                        break;
                    case Type::dualFilter:
                    {
                        const auto spread = std::pow (2.0f, p2 * 3.0f);   // BANDWIDTH: each side up to 3 octaves from the centre
                        const auto q = 0.707f + res * 8.0f;
                        ch.s[0].set (prewarp (hz / spread, sampleRate), q);
                        ch.s[1].set (prewarp (hz * spread, sampleRate), q);
                        break;
                    }
                    case Type::formant:
                    {
                        const auto pos = mainHere * 4.0f;
                        const auto v0 = juce::jlimit (0, 3, (int) pos);
                        const auto f = pos - (float) v0;
                        const auto shift = (p2 - 0.5f) * 1000.0f;   // SHIFT: each formant +-500 Hz
                        const auto q = 4.0f + res * 16.0f;
                        for (int b = 0; b < 3; ++b)
                        {
                            const auto formantHz = vowels[v0][b] + (vowels[v0 + 1][b] - vowels[v0][b]) * f + shift;
                            ch.s[b].set (prewarp (juce::jmax (80.0f, formantHz), sampleRate), q);
                            ch.gains[b] = formantLevels[b];
                        }
                        break;
                    }
                    case Type::eqBell: case Type::eqLowShelf: case Type::eqHighShelf:
                    {
                        const auto A = juce::Decibels::decibelsToGain (eqGainDb (res) * 0.5f);
                        const auto q = eqQ (p2);
                        if (type == Type::eqBell)
                        {
                            ch.s[0].set (ch.g, q * A);
                            ch.m0 = 1.0f; ch.m1 = ch.s[0].k * (A * A - 1.0f); ch.m2 = 0.0f;
                        }
                        else if (type == Type::eqLowShelf)
                        {
                            ch.s[0].set (ch.g / std::sqrt (A), q);
                            ch.m0 = 1.0f; ch.m1 = ch.s[0].k * (A - 1.0f); ch.m2 = A * A - 1.0f;
                        }
                        else
                        {
                            ch.s[0].set (ch.g * std::sqrt (A), q);
                            ch.m0 = A * A; ch.m1 = ch.s[0].k * (1.0f - A) * A; ch.m2 = 1.0f - A * A;
                        }
                        break;
                    }
                    case Type::gain:
                        ch.level = juce::Decibels::decibelsToGain (gainUtilityDb (mainHere), -100.0f);
                        break;
                    case Type::pan: case Type::off:
                    case Type::comb: case Type::phaser: case Type::pitchShift: case Type::freqShift: case Type::ringMod: case Type::fm:
                    case Type::distort: case Type::decimate: case Type::delay: case Type::reverb: case Type::reflect: case Type::vocoder:
                    case Type::loop: case Type::stretch: case Type::varispeed: case Type::slow: case Type::reverse: case Type::gate:
                    case Type::compress:
                        break;
                }
            }

            //------------------------------------------------------------------
            const auto slotGain = juce::Decibels::decibelsToGain (slot.gainDb);
            const auto mix = juce::jlimit (0.0f, 1.0f, slot.mix);

            if (type == Type::pan)
            {
                if (numChannels < 2)
                    continue;
                auto* l = block.getChannelPointer (0) + start;
                auto* r = block.getChannelPointer (1) + start;
                const auto p = (st.glided - 0.5f) * 2.0f;   // -1 left to +1 right
                const auto width = juce::jlimit (0.0f, 1.0f, slot.p1) * 2.0f;
                const auto mode = modeOf (Type::pan, slot.option);   // a filter's 24 dB (the default) is PAN
                const auto angle = (p + 1.0f) * pi * 0.25f;
                const auto gl = std::cos (angle) * juce::MathConstants<float>::sqrt2, gr = std::sin (angle) * juce::MathConstants<float>::sqrt2;
                for (int i = 0; i < count; ++i)
                {
                    const auto inL = l[i], inR = r[i];
                    const auto mid = (inL + inR) * 0.5f;
                    const auto side = (inL - inR) * 0.5f * width;   // WIDTH: mid/side (over 100% puts the sides out of phase)
                    auto wl = mid + side, wr = mid - side;
                    if (mode == 0)          // PAN: both sides move across
                    {
                        if (p > 0.0f) { wr += wl * p; wl *= 1.0f - p; }
                        else          { wl += wr * -p; wr *= 1.0f + p; }
                    }
                    else if (mode == 1)     // BALANCE: one side turned down
                    {
                        if (p > 0.0f) wl *= 1.0f - p; else wr *= 1.0f + p;
                    }
                    else                    // ROTATE: only the middle moves; the sides (reverbs) stay put
                    {
                        const auto m2 = (wl + wr) * 0.5f, s2 = (wl - wr) * 0.5f;
                        wl = s2 + m2 * gl;
                        wr = -s2 + m2 * gr;
                    }
                    l[i] = inL + (wl * slotGain - inL) * mix;
                    r[i] = inR + (wr * slotGain - inR) * mix;
                }
                continue;
            }

            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = st.ch[(size_t) c];
                auto* x = block.getChannelPointer ((size_t) c) + start;
                for (int i = 0; i < count; ++i)
                {
                    const auto in = x[i];
                    float y = in;
                    switch (type)
                    {
                        case Type::ladderLp: case Type::ladderBp: case Type::ladderHp:
                        case Type::oneOhOneLp: case Type::oneOhOneBp: case Type::oneOhOneHp:
                        {
                            // Four one-pole stages with feedback around them,
                            // solved without a sample's delay (Zavalishin's
                            // method), the feedback saturating softly.
                            const auto G = ch.G;
                            const auto b = 1.0f / (1.0f + ch.g);
                            const auto S = (G * G * G * ch.ladder[0] + G * G * ch.ladder[1] + G * ch.ladder[2] + ch.ladder[3]) * b;
                            const auto G4 = G * G * G * G;
                            auto u = (in * ch.driveGain - ch.k * S) / (1.0f + ch.k * G4);
                            u = std::tanh (u);
                            float taps[5] { u, 0, 0, 0, 0 };
                            auto stageIn = u;
                            for (int s = 0; s < 4; ++s)
                            {
                                const auto v = (stageIn - ch.ladder[s]) * G;
                                const auto out = v + ch.ladder[s];
                                ch.ladder[s] = out + v;
                                taps[s + 1] = out;
                                stageIn = out;
                            }
                            const auto oneOhOne = type >= Type::oneOhOneLp;
                            const auto steep = oneOhOne ? 3 : slope;   // the 101 is always 24 dB
                            const auto lp = type == Type::ladderLp || type == Type::oneOhOneLp;
                            const auto hp = type == Type::ladderHp || type == Type::oneOhOneHp;
                            if (lp)
                                y = taps[steep + 1];
                            else if (hp)
                            {
                                // High-pass from the same taps (binomial mixes).
                                switch (steep)
                                {
                                    case 0:  y = taps[0] - taps[1]; break;
                                    case 1:  y = taps[0] - 2.0f * taps[1] + taps[2]; break;
                                    case 2:  y = taps[0] - 3.0f * taps[1] + 3.0f * taps[2] - taps[3]; break;
                                    default: y = taps[0] - 4.0f * taps[1] + 6.0f * taps[2] - 4.0f * taps[3] + taps[4]; break;
                                }
                            }
                            else
                                y = steep < 2 ? 2.0f * (taps[1] - taps[2]) : 4.0f * (taps[2] - 2.0f * taps[3] + taps[4]);
                            y *= ch.makeUp;
                            break;
                        }
                        case Type::digitalLp: case Type::digitalBp: case Type::digitalHp:
                        {
                            const auto lp = type == Type::digitalLp, hp = type == Type::digitalHp;
                            auto signal = in;
                            if (slope == 0 || slope == 2)
                            {
                                // A one-pole stage (6 dB) first.
                                const auto v = (signal - ch.pole) * ch.G;
                                const auto out = v + ch.pole;
                                ch.pole = out + v;
                                signal = lp ? out : hp ? signal - out : signal;
                            }
                            if (slope >= 1)
                            {
                                const auto o = ch.s[0].run (signal, ch.svf[0]);
                                signal = lp ? o.low : hp ? o.high : o.band * ch.s[0].k;
                            }
                            if (slope == 3)
                            {
                                const auto o = ch.s[1].run (signal, ch.svf[1]);
                                signal = lp ? o.low : hp ? o.high : o.band * ch.s[1].k;
                            }
                            if (slope == 0 && ! lp && ! hp)
                            {
                                const auto o = ch.s[0].run (in, ch.svf[0]);   // a band-pass needs two poles at least
                                signal = o.band * ch.s[0].k;
                            }
                            y = signal;
                            break;
                        }
                        case Type::model20Lp: case Type::model20Bp: case Type::model20Hp:
                        {
                            const auto lp = type == Type::model20Lp, hp = type == Type::model20Hp;
                            auto signal = softClip (in * ch.driveGain * 0.7f) / 0.7f;
                            if (slope == 0 || slope == 2)
                            {
                                const auto v = (signal - ch.pole) * ch.G;
                                const auto out = v + ch.pole;
                                ch.pole = out + v;
                                signal = lp ? out : hp ? signal - out : signal;
                            }
                            const auto stage = [&] (int n)
                            {
                                const auto o = ch.s[n].run (signal, ch.svf[n]);
                                // The resonance clips: the band state is held under about 1.5.
                                ch.svf[n][0] = 1.5f * softClip (ch.svf[n][0] / 1.5f);
                                signal = lp ? o.low : hp ? o.high : o.band * juce::jmin (1.0f, ch.s[n].k * 2.0f);
                            };
                            if (slope >= 1 || ! (lp || hp))
                                stage (0);
                            if (slope == 3)
                                stage (1);
                            y = signal * ch.makeUp;
                            break;
                        }
                        case Type::notch:
                        {
                            const auto o = ch.s[0].run (in, ch.svf[0]);
                            y = in - ch.s[0].k * o.band;
                            break;
                        }
                        case Type::dualFilter:
                        {
                            const auto a = ch.s[0].run (in, ch.svf[0]);
                            const auto b = ch.s[1].run (a.high, ch.svf[1]);
                            y = b.low;
                            break;
                        }
                        case Type::formant:
                        {
                            y = 0.0f;
                            for (int b = 0; b < 3; ++b)
                                y += ch.s[b].run (in, ch.svf[b]).band * ch.s[b].k * ch.gains[b];
                            y *= 1.6f;
                            break;
                        }
                        case Type::eqBell: case Type::eqLowShelf: case Type::eqHighShelf:
                        {
                            const auto o = ch.s[0].run (in, ch.svf[0]);
                            y = ch.m0 * in + ch.m1 * o.band + ch.m2 * o.low;
                            break;
                        }
                        case Type::gain:
                            y = in * ch.level;
                            break;
                        case Type::pan: case Type::off:
                        case Type::comb: case Type::phaser: case Type::pitchShift: case Type::freqShift: case Type::ringMod: case Type::fm:
                    case Type::distort: case Type::decimate: case Type::delay: case Type::reverb: case Type::reflect: case Type::vocoder:
                    case Type::loop: case Type::stretch: case Type::varispeed: case Type::slow: case Type::reverse: case Type::gate:
                    case Type::compress:
                            break;
                    }
                    if (! std::isfinite (y))
                    {
                        st.reset();
                        y = 0.0f;
                    }
                    x[i] = in + (y * slotGain - in) * mix;
                }
            }
        }
    }
}

namespace effects
{
    void Cipher::processTime (int index, juce::dsp::AudioBlock<float>& block, int start, int count, const Slot& slot,
                              const Settings& settings, const float* mainPerSide)
    {
        auto& t = *timeStates[(size_t) index];
        const auto type = slot.type;
        const auto numChannels = (int) juce::jmin ((size_t) 2, block.getNumChannels());
        const auto sr = (float) sampleRate;
        const auto p1 = juce::jlimit (0.0f, 1.0f, slot.p1), p2 = juce::jlimit (0.0f, 1.0f, slot.p2);
        const auto mode = modeOf (type, slot.option);
        const auto main = juce::jlimit (0.0f, 1.0f, mainPerSide[0]);   // the main knob as the envelope moves it
        const auto slotGain = juce::Decibels::decibelsToGain (slot.gainDb);
        const auto mix = juce::jlimit (0.0f, 1.0f, slot.mix);
        const auto bpm = juce::jmax (20.0, settings.bpm);
        const auto samplesPerBeat = sampleRate * 60.0 / bpm;
        float* chans[2] = { block.getChannelPointer (0) + start, block.getChannelPointer ((size_t) (numChannels - 1)) + start };

        // A different capturing effect starts afresh.
        if (t.capturedFor != type)
        {
            t.capturedFor = type;
            t.captured = false;
            t.lastGrid = -1;
            t.counter = 0.0;
        }

        // TEMPO: a new part of the sound is taken on this grid (1 bar to 1/32),
        // on the song's beats while it plays.
        const double captureBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        const auto everyBeats = captureBeats[noteStep (p1, 6)];
        const auto captureDue = [&] (int offset)
        {
            if (settings.songBeats >= 0.0)
            {
                const auto beat = settings.songBeats + (double) (start + offset) / samplesPerBeat;
                const auto grid = (long long) std::floor (beat / everyBeats);
                if (grid != t.lastGrid || ! t.captured)
                {
                    t.lastGrid = grid;
                    t.captured = true;
                    return true;
                }
                return false;
            }
            t.counter += 1.0;
            if (! t.captured || t.counter >= everyBeats * samplesPerBeat)
            {
                t.counter = 0.0;
                t.captured = true;
                return true;
            }
            return false;
        };

        // A one-pole low-pass coefficient for a cutoff in Hz.
        const auto onePole = [sr] (float hz) { return 1.0f - std::exp (-2.0f * pi * juce::jlimit (5.0f, sr * 0.45f, hz) / sr); };
        const auto store = [&t] (int c, float x) { t.memory[(size_t) c][(size_t) t.write] = x; };
        const auto advance = [&t]
        {
            t.write = t.write + 1 < t.size ? t.write + 1 : 0;
            t.now += 1.0;
        };
        const auto finish = [&] (int c, int i, float in, float y)
        {
            if (! std::isfinite (y))
            {
                t.reset();
                y = 0.0f;
            }
            chans[c][i] = in + (y * slotGain - in) * mix;
        };

        switch (type)
        {
            //------------------------------------------------------------------
            case Type::comb:
            {
                // A short delay with lots of feedback: SPEED is the delay (as a
                // pitch, 20 Hz to 5 kHz), RESONANCE the feedback, BRIGHTNESS a
                // low-pass in the loop.
                const auto fb = p1 * 0.97f;
                const auto bright = onePole (300.0f * std::pow (60.0f, p2));
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto hz = juce::jlimit (20.0f, 5000.0f, 20.0f * std::pow (250.0f, mainPerSide[c]));
                        const auto in = chans[c][i];
                        const auto delayed = t.read (c, sr / hz - 1.0f);
                        t.tone[c] += bright * (delayed - t.tone[c]);
                        const auto y = in + fb * t.tone[c];
                        store (c, y);
                        finish (c, i, in, y * (1.0f - fb * 0.6f));
                    }
                    advance();
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::phaser:
            {
                // All-pass stages around FREQUENCY, spread apart by SPREAD,
                // mixed with the input for the notches. TURBO: 12 stages; DUO: 4
                // (two notches); INVERSE: TURBO with the stages' sound inverted.
                const auto stages = mode == 1 ? 4 : 12;
                const auto sign = mode == 2 ? -1.0f : 1.0f;
                const auto fb = p1 * 0.85f;
                float coefficients[2][12];
                for (int c = 0; c < numChannels; ++c)
                {
                    const auto centre = juce::jlimit (40.0f, 16000.0f, cutoffHz (mainPerSide[c]));
                    const auto spread = std::pow (2.0f, 0.2f + p2 * 2.8f);
                    for (int st = 0; st < stages; ++st)
                    {
                        const auto place = stages > 1 ? ((float) st / (float) (stages - 1)) * 2.0f - 1.0f : 0.0f;
                        const auto f = juce::jlimit (20.0f, sr * 0.45f, centre * std::pow (spread, place));
                        const auto w = std::tan (pi * f / sr);
                        coefficients[c][st] = (w - 1.0f) / (w + 1.0f);
                    }
                }
                for (int i = 0; i < count; ++i)
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        auto x = in + fb * t.last[c];
                        for (int st = 0; st < stages; ++st)
                        {
                            const auto a = coefficients[c][st];
                            const auto y = a * x + t.allPass[c][st];
                            t.allPass[c][st] = x - a * y;
                            x = y;
                        }
                        t.last[c] = x;
                        finish (c, i, in, 0.5f * (in + sign * x));
                    }
                break;
            }

            //------------------------------------------------------------------
            case Type::pitchShift:
            {
                // A lo-fi granular shifter: two grains per side read the memory
                // at the new speed, each faded in and out (Hann), half a grain
                // apart. GRAIN FREQ: how many grains a second (2-100); JITTER
                // varies each grain's length, side by side.
                const auto grainHz = 2.0f + p1 * 98.0f;
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        store (c, in);
                        const auto ratio = std::pow (2.0f, semitones (mainPerSide[c]) / 12.0f);
                        float y = 0.0f;
                        for (int g = 0; g < 2; ++g)
                        {
                            auto& phase = t.grainPhase[c][g];
                            auto& length = t.grainLength[c][g];
                            if (length <= 0.0f)
                                length = sr / grainHz;
                            const auto ph = (float) phase;
                            // How far back this grain reads: it sweeps across the
                            // grain so the sound plays at `ratio` times its speed.
                            const auto back = 2.0f + (ratio > 1.0f ? (1.0f - ph) * (ratio - 1.0f) : ph * (1.0f - ratio)) * length;
                            y += t.read (c, juce::jmin (back, (float) t.size - 4.0f)) * std::sin (pi * ph);
                            phase += 1.0 / length;
                            if (phase >= 1.0)
                            {
                                phase -= 1.0;
                                length = sr / grainHz * (1.0f + p2 * (t.random.nextFloat() - 0.5f) * 1.4f);
                            }
                        }
                        finish (c, i, in, y * 0.75f);
                    }
                    advance();
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::freqShift:
            {
                // Every frequency moved by the same number of hertz: two all-pass
                // chains make a pair of signals 90 degrees apart (Olli
                // Niemitalo's coefficients), mixed with a pair of sines.
                constexpr float a[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
                constexpr float b[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
                const auto chain = [] (const float* coeff, float (*st)[4], float x)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        // y[n] = c^2 (x[n] + y[n-2]) - x[n-2]
                        const auto y = coeff[k] * coeff[k] * (x + st[k][3]) - st[k][1];
                        st[k][1] = st[k][0]; st[k][0] = x;
                        st[k][3] = st[k][2]; st[k][2] = y;
                        x = y;
                    }
                    return x;
                };
                const auto hz = shiftHz (main);
                for (int i = 0; i < count; ++i)
                {
                    const auto phase = (float) t.oscillator;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        const auto re = chain (a, t.hilbertA[c], t.hilbertDelay[c]);
                        t.hilbertDelay[c] = in;
                        const auto im = chain (b, t.hilbertB[c], in);
                        const auto offsetPhase = phase + (numChannels > 1 && c == 1 ? slot.stereo * pi * 0.5f : 0.0f);
                        finish (c, i, in, re * std::cos (offsetPhase) + im * std::sin (offsetPhase));
                    }
                    t.oscillator += 2.0 * juce::MathConstants<double>::pi * hz / sampleRate;
                    if (t.oscillator > juce::MathConstants<double>::twoPi) t.oscillator -= juce::MathConstants<double>::twoPi;
                    if (t.oscillator < 0.0) t.oscillator += juce::MathConstants<double>::twoPi;
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::ringMod:
            case Type::fm:
            {
                // Ring Mod: a sine at FREQUENCY multiplies the sound; AMOUNT is how
                // much, CHARACTER squares the sine (less pure). FM: the same
                // oscillator wobbles a tiny delay, so the sound's frequency
                // swings at audio rate (AMOUNT, up to 4 ms); CHARACTER is a pure
                // sine at the centre, a saw to the left, a square to the right.
                const auto ring = type == Type::ringMod;
                const auto hz = juce::jlimit (0.5f, sr * 0.45f, ring ? cutoffHz (main) : 0.5f * std::pow (10000.0f, main));
                const auto amount = ring ? p1 : p2;
                const auto character = ring ? p2 : p1;
                const auto depth = amount * amount * 0.004f * sr;
                for (int i = 0; i < count; ++i)
                {
                    const auto phase = (float) t.oscillator;
                    auto wave = std::sin (phase);
                    if (ring)
                    {
                        const auto k = 1.0f + character * 10.0f;
                        wave = std::tanh (k * wave) / std::tanh (k);
                    }
                    else if (character < 0.5f)
                    {
                        const auto saw = phase / pi - 1.0f;
                        wave += (saw - wave) * (0.5f - character) * 2.0f;
                    }
                    else
                        wave += (std::tanh (8.0f * wave) - wave) * (character - 0.5f) * 2.0f;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        store (c, in);
                        const auto y = ring ? in * ((1.0f - amount) + amount * wave)
                                            : t.read (c, 1.0f + depth * (1.0f + wave));
                        finish (c, i, in, y);
                    }
                    advance();
                    t.oscillator += juce::MathConstants<double>::twoPi * hz / sampleRate;
                    if (t.oscillator >= juce::MathConstants<double>::twoPi) t.oscillator -= juce::MathConstants<double>::twoPi;
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::distort:
            {
                // The master drive's curves as an effect: HP before (20 Hz to
                // 2 kHz), DRIVE (up to 40 dB), the curve run at twice the rate
                // (each sample and a point between) so it folds back less, LP after.
                const auto hpCoeff = onePole (20.0f * std::pow (100.0f, p1));
                const auto lpCoeff = onePole (500.0f * std::pow (40.0f, p2));
                for (int i = 0; i < count; ++i)
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        const auto pre = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 1.0f, mainPerSide[c]) * 40.0f);
                        t.hp[c] += hpCoeff * (in - t.hp[c]);
                        const auto x = (in - t.hp[c]) * pre;
                        const auto mid = 0.5f * (x + t.last[c]);
                        t.last[c] = x;
                        const auto shaped = 0.5f * (driveShape (mode, mid) + driveShape (mode, x));
                        t.tone[c] += lpCoeff * (shaped / std::sqrt (pre) - t.tone[c]);
                        finish (c, i, in, t.tone[c]);
                    }
                break;
            }

            //------------------------------------------------------------------
            case Type::decimate:
            {
                // A lower sample rate (FREQUENCY, 100 Hz to the full rate): the
                // sound held between samples. DIRT: how much of the aliasing and
                // the steps get through; at 0 the sound is filtered before (so
                // little folds back) and after (smoothing the steps).
                const auto rate = juce::jmin (sr, 100.0f * std::pow (480.0f, juce::jlimit (0.0f, 1.0f, main)));
                Svf filter;
                filter.set (prewarp (juce::jmin (sr * 0.45f, rate * 0.45f * std::pow (2.0f, p1 * 7.0f)), sampleRate), 0.707f);
                const auto bypassFilter = p1 > 0.98f;
                const auto smooth = onePole (juce::jmin (sr * 0.45f, rate * 0.4f * std::pow (2.0f, p1 * 7.0f)));
                for (int i = 0; i < count; ++i)
                {
                    t.holdPhase += rate / sr;
                    const auto take = t.holdPhase >= 1.0;
                    if (take)
                        t.holdPhase -= std::floor (t.holdPhase);
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        const auto filtered = bypassFilter ? in : filter.run (in, t.decimateSvf[c]).low;
                        if (take)
                            t.held[c] = filtered;
                        if (bypassFilter)
                            t.tone[c] = t.held[c];
                        else
                        {
                            t.tone[c] += smooth * (t.held[c] - t.tone[c]);
                            t.tone2[c] += smooth * (t.tone[c] - t.tone2[c]);
                        }
                        finish (c, i, in, bypassFilter ? t.held[c] : t.tone2[c]);
                    }
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::delay:
            {
                // SYNC: TIME steps through 1/32 to 1 bar; FREE: 1 ms to 2 s. The
                // time glides (about 50 ms), so moving it bends the echoes like
                // tape. FEEDBACK: the repeats; TONE: darker to the left, brighter
                // to the right (in the loop). KIND (the STEREO knob): the
                // manual's ping-pong, stereo, wide and fractional delays.
                const auto kind = delayStereoKind (slot.stereo);
                const auto fb = juce::jmin (0.98f, p1);
                const auto dark = p2 < 0.5f ? onePole (20000.0f * std::pow (0.04f, (0.5f - p2) * 2.0f)) : 1.0f;
                const auto bright = p2 > 0.5f ? onePole (20.0f * std::pow (100.0f, (p2 - 0.5f) * 2.0f)) : 0.0f;
                const auto timeSamples = (mode == 0 ? speedBeats (16 - noteStep (main, 14)) * samplesPerBeat
                                                    : freeDelayMs (main) * 0.001 * sampleRate);
                const float fractions[] = { 0.25f, 1.0f / 3.0f, 0.5f, 1.0f, 0.75f, 1.0f, 1.0f };
                float wanted[2] = { (float) timeSamples, (float) timeSamples };
                if (kind == 5)
                {
                    wanted[0] *= 0.97f;   // WIDE: the two sides very slightly apart
                    wanted[1] *= 1.03f;
                }
                else
                    wanted[0] *= fractions[kind];
                const auto glide = 1.0f - std::exp (-1.0f / (0.05f * sr));
                const auto pingPong = kind == 6 && numChannels > 1;
                for (int i = 0; i < count; ++i)
                {
                    float echoes[2] {};
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto& d = t.delayNow[c];
                        const auto target = juce::jlimit (1.0f, (float) t.size - 4.0f, wanted[c]);
                        d = d < 0.0f ? target : d + glide * (target - d);
                        auto echo = t.read (c, d - 1.0f);
                        t.tone[c] += dark * (echo - t.tone[c]);
                        echo = t.tone[c];
                        t.hp[c] += bright * (echo - t.hp[c]);
                        echoes[c] = echo - t.hp[c];
                    }
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        // PING-PONG: the sound goes in on the left, each side feeds the other.
                        const auto feed = pingPong ? (c == 0 ? (chans[0][i] + chans[1][i]) * 0.5f + fb * echoes[1] : fb * echoes[0])
                                                   : in + fb * echoes[c];
                        store (c, std::tanh (feed));
                        finish (c, i, in, in + echoes[c]);
                    }
                    advance();
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::reflect:
            {
                // Early reflections: twelve echoes from walls, their times set by
                // SIZE (1 to 60 m), each quieter and duller than the last
                // (ABSORB); FEEDBACK sends them round again for slapback.
                constexpr float taps[12] = { 0.043f, 0.071f, 0.097f, 0.131f, 0.157f, 0.193f, 0.229f, 0.263f, 0.307f, 0.353f, 0.401f, 0.457f };
                const auto metres = 1.0f + 59.0f * main * main;
                const auto scale = metres / 343.0f * sr;   // seconds across a room this size, in samples, per unit of the tap table
                const auto absorb = onePole (18000.0f * std::pow (0.05f, p1));
                const auto fb = p2 * 0.7f;
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        float sum = 0.0f;
                        for (int k = 0; k < 12; ++k)
                        {
                            const auto side = numChannels > 1 && (k % 2 == c) ? 1.0f : 0.6f;
                            sum += t.read (c, juce::jmin ((float) t.size - 4.0f, taps[k] * scale * (c == 1 ? 1.07f : 1.0f)))
                                   * side * (1.0f - (float) k * 0.06f);
                        }
                        sum *= 0.22f;
                        t.tone[c] += absorb * (sum - t.tone[c]);
                        store (c, in + fb * t.tone[c]);
                        finish (c, i, in, in + t.tone[c]);
                    }
                    advance();
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::reverb:
            {
                // A feedback delay network of eight lines. TIME: how long the tail
                // lasts (0.2 to 20 s); TONE: how fast the highs die (darker to the
                // left); HPF: the lows taken out first (20 Hz to 2 kHz). REVERB: a
                // smooth room or hall. LO-FI: the lines' lengths follow TIME, so
                // turning it re-pitches what's ringing, and the tail is crushed to
                // 8 bits at half the rate. VOID: a long diffuser first (a slow
                // swell in), and darker.
                constexpr float lengthsMs[TimeState::numLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.1f, 67.3f };
                constexpr float diffuserMs[TimeState::numDiffusers] = { 4.7f, 7.3f, 11.9f, 31.3f, 53.9f, 89.1f };
                const auto seconds = reverbSeconds (main);
                const auto lofi = mode == 1, voidType = mode == 2;
                const auto sizeScale = lofi ? 0.5f + main * 1.0f : 1.0f;
                const auto damping = onePole ((voidType ? 3000.0f : 20000.0f) * std::pow (voidType ? 0.07f : 0.06f, 1.0f - p1) + 200.0f);
                const auto hpCoeff = onePole (20.0f * std::pow (100.0f, p2));
                float lengths[TimeState::numLines], gains[TimeState::numLines];
                for (int l = 0; l < TimeState::numLines; ++l)
                {
                    lengths[l] = juce::jmin ((float) t.lines[(size_t) l].size() - 3.0f, lengthsMs[l] * 0.001f * sr * sizeScale);
                    gains[l] = std::pow (10.0f, -3.0f * lengths[l] / (seconds * sr));
                }
                const auto diffusers = voidType ? TimeState::numDiffusers : 3;
                const auto width = juce::jlimit (0.0f, 1.0f, 0.5f + slot.stereo * 0.5f);
                for (int i = 0; i < count; ++i)
                {
                    // The input: both sides together, its lows taken out.
                    auto in = 0.0f;
                    for (int c = 0; c < numChannels; ++c)
                        in += chans[c][i];
                    in /= (float) numChannels;
                    t.reverbHp[0] += hpCoeff * (in - t.reverbHp[0]);
                    auto x = in - t.reverbHp[0];
                    for (int d = 0; d < diffusers; ++d)
                    {
                        auto& line = t.diffusers[(size_t) d];
                        const auto len = juce::jmin ((int) line.size() - 2, (int) (diffuserMs[d] * 0.001f * sr));
                        auto readAt = t.diffuserWrite[(size_t) d] - len;
                        if (readAt < 0) readAt += (int) line.size();
                        const auto delayed = line[(size_t) readAt];
                        const auto g = voidType ? 0.7f : 0.6f;
                        const auto v = x + g * delayed;
                        line[(size_t) t.diffuserWrite[(size_t) d]] = v;
                        x = delayed - g * v;
                        t.diffuserWrite[(size_t) d] = (t.diffuserWrite[(size_t) d] + 1) % (int) line.size();
                    }
                    // The lines' outputs, mixed (Householder) and fed back.
                    float outs[TimeState::numLines];
                    auto sum = 0.0f;
                    for (int l = 0; l < TimeState::numLines; ++l)
                    {
                        auto& line = t.lines[(size_t) l];
                        auto position = (float) t.lineWrite[(size_t) l] - lengths[l];
                        if (position < 0.0f) position += (float) line.size();
                        const auto i0 = (int) position;
                        const auto frac = position - (float) i0;
                        const auto i1 = i0 + 1 < (int) line.size() ? i0 + 1 : 0;
                        auto v = line[(size_t) i0] + (line[(size_t) i1] - line[(size_t) i0]) * frac;
                        t.lineDamp[(size_t) l] += damping * (v - t.lineDamp[(size_t) l]);
                        outs[l] = t.lineDamp[(size_t) l] * gains[l];
                        sum += outs[l];
                    }
                    const auto householder = sum * (2.0f / (float) TimeState::numLines);
                    for (int l = 0; l < TimeState::numLines; ++l)
                    {
                        auto& line = t.lines[(size_t) l];
                        line[(size_t) t.lineWrite[(size_t) l]] = outs[l] - householder + x * 0.35f;
                        t.lineWrite[(size_t) l] = (t.lineWrite[(size_t) l] + 1) % (int) line.size();
                    }
                    float sides[2] = { outs[0] + outs[2] + outs[4] + outs[6], outs[1] + outs[3] + outs[5] + outs[7] };
                    if (lofi)
                    {
                        // Half the rate and 8 bits.
                        if ((t.crushCount++ & 1) == 0)
                            for (int c = 0; c < 2; ++c)
                                t.crushHeld[c] = std::round (sides[c] * 127.0f) / 127.0f;
                        sides[0] = t.crushHeld[0];
                        sides[1] = t.crushHeld[1];
                    }
                    const auto mid = (sides[0] + sides[1]) * 0.5f, side = (sides[0] - sides[1]) * 0.5f * width * 2.0f;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto inC = chans[c][i];
                        const auto wetC = numChannels > 1 ? (c == 0 ? mid + side : mid - side) : mid;
                        finish (c, i, inC, inC + wetC * 0.6f);
                    }
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::vocoder:
            {
                // Sixteen bands: the sound's level in each (the modulator) sets
                // the level of the same band of an oscillator (the carrier) at
                // FREQUENCY (30 Hz to 1 kHz). FORMANT moves the carrier's bands
                // (+-12 semitones) without changing its pitch; RESONANCE sharpens
                // every band; CHARACTER (the STEREO knob) goes from sawtooth
                // through square to a narrow pulse.
                const auto hz = juce::jlimit (20.0f, 2000.0f, 30.0f * std::pow (33.0f, juce::jlimit (0.0f, 1.0f, main)));
                const auto formant = std::pow (2.0f, (p1 - 0.5f) * 2.0f);
                const auto q = 2.0f + p2 * 28.0f;
                const auto character = juce::jlimit (0.0f, 1.0f, (slot.stereo + 1.0f) * 0.5f);
                Svf modulator[TimeState::numBands], carrier[TimeState::numBands];
                for (int b = 0; b < TimeState::numBands; ++b)
                {
                    const auto f = 100.0f * std::pow (80.0f, (float) b / (float) (TimeState::numBands - 1));   // 100 Hz to 8 kHz
                    modulator[b].set (prewarp (f, sampleRate), q);
                    carrier[b].set (prewarp (f * formant, sampleRate), q);
                }
                const auto attack = 1.0f - std::exp (-1.0f / (0.002f * sr)), release = 1.0f - std::exp (-1.0f / (0.03f * sr));
                const auto width = character < 0.5f ? 0.5f : 0.5f - (character - 0.5f) * 0.9f;   // the pulse narrows past SQUARE
                const auto squareMix = juce::jmin (1.0f, character * 2.0f);
                for (int i = 0; i < count; ++i)
                {
                    const auto phase = (float) (t.oscillator / juce::MathConstants<double>::twoPi);
                    const auto saw = 2.0f * phase - 1.0f;
                    const auto pulse = phase < width ? 1.0f : -1.0f;
                    const auto osc = saw + (pulse - saw) * squareMix;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        auto y = 0.0f;
                        for (int b = 0; b < TimeState::numBands; ++b)
                        {
                            const auto band = std::abs (modulator[b].run (in, t.vocoderModulator[c][b]).band * modulator[b].k);
                            auto& level = t.vocoderLevel[c][b];
                            level += (band > level ? attack : release) * (band - level);
                            y += carrier[b].run (osc, t.vocoderCarrier[c][b]).band * carrier[b].k * level;
                        }
                        finish (c, i, in, y * 3.0f);
                    }
                    t.oscillator += juce::MathConstants<double>::twoPi * hz / sampleRate;
                    if (t.oscillator >= juce::MathConstants<double>::twoPi) t.oscillator -= juce::MathConstants<double>::twoPi;
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::loop:
            {
                // Repeats the last part of the sound: FREE, its length is 1/FREQUENCY
                // (1 s down to 1 ms: stutters to buzzing pitches); SYNC, 1 bar down
                // to 1/64. TEMPO: how often a new part is taken; SMOOTH: the
                // crossfade at the loop's join.
                const double syncBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                        store (c, chans[c][i]);
                    advance();
                    const auto length = juce::jlimit (8.0, (double) t.size * 0.45,
                                                      mode == 1 ? syncBeats[noteStep (main, 7)] * samplesPerBeat
                                                                : sampleRate / loopHz (mainPerSide[0]));
                    if (captureDue (i))
                    {
                        t.loopStart = t.now - length;
                        t.loopPosition = 0.0;
                    }
                    if (t.loopPosition >= length)
                        t.loopPosition -= length * std::floor (t.loopPosition / length);
                    const auto fadeLength = juce::jmax (1.0, p2 * length * 0.5);
                    for (int c = 0; c < numChannels; ++c)
                    {
                        const auto in = chans[c][i];
                        auto y = t.at (c, t.loopStart + t.loopPosition);
                        if (t.loopPosition > length - fadeLength)
                        {
                            // Near the end, fade into what led up to the start, so the join is smooth.
                            const auto a = (float) ((length - t.loopPosition) / fadeLength);
                            y = y * a + t.at (c, t.loopStart + t.loopPosition - length) * (1.0f - a);
                        }
                        finish (c, i, in, y);
                    }
                    t.loopPosition += 1.0;
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::stretch:
            {
                // Slows a part of the sound down without changing its pitch:
                // grains (GRAIN RATE, 4-100 a second) play at normal speed from a
                // place that moves on at SPEED (5% to 100%). Two grains, half a
                // grain apart, each faded in and out. TEMPO: how often it jumps
                // to a new part.
                const auto speed = 0.05 + 0.95 * (double) juce::jlimit (0.0f, 1.0f, main);
                const auto grain = (double) sr / (4.0 * std::pow (25.0, (double) p2));
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                        store (c, chans[c][i]);
                    advance();
                    if (captureDue (i))
                        t.readPosition = t.now - grain;
                    float out[2] {};
                    for (int g = 0; g < 2; ++g)
                    {
                        auto phase = t.grainHead[0] + (g == 1 ? 0.5 : 0.0);
                        if (phase >= 1.0)
                            phase -= 1.0;
                        if (phase < 1.0 / grain)   // this grain starts again: from where the read place is now
                            t.grainStart[g] = t.readPosition;
                        const auto window = std::sin (pi * (float) phase);
                        for (int c = 0; c < numChannels; ++c)
                            out[c] += t.at (c, juce::jmin (t.now - 1.0, t.grainStart[g] + phase * grain)) * window;
                    }
                    t.grainHead[0] += 1.0 / grain;
                    if (t.grainHead[0] >= 1.0)
                        t.grainHead[0] -= 1.0;
                    t.readPosition = juce::jmin (t.now - grain, t.readPosition + speed);
                    for (int c = 0; c < numChannels; ++c)
                        finish (c, i, chans[c][i], out[c] * 0.8f);
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::varispeed:
            case Type::slow:
            case Type::reverse:
            {
                // Plays a part of the sound at another speed, which changes its
                // pitch like tape. VARISPEED: SPEED 5% to 100%; FWD-REV: the left
                // half plays backwards (full speed at the far left), the right
                // half forwards; REVERSE: backwards only. SLOW and REVERSE: the
                // main knob steps from full speed down to 1/16. TEMPO: how often
                // a new part is taken (a 5 ms crossfade at each).
                double speed = 1.0;
                if (type == Type::varispeed)
                {
                    if (mode == 1)
                        speed = main < 0.5f ? -(0.5 - main) * 2.0 : (main - 0.5) * 2.0;
                    else
                        speed = (0.05 + 0.95 * (double) main) * (mode == 2 ? -1.0 : 1.0);
                }
                else
                    speed = std::pow (0.5, (double) noteStep (main, 5)) * (type == Type::reverse ? -1.0 : 1.0);
                const auto fadeSamples = juce::jmax (1, (int) (0.005 * sampleRate));
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < numChannels; ++c)
                        store (c, chans[c][i]);
                    advance();
                    if (captureDue (i))
                    {
                        t.oldReadPosition = t.readPosition;
                        t.readPosition = t.now - 1.0;
                        t.fade = fadeSamples;
                    }
                    // Never read past what's been written, or further back than the memory holds.
                    t.readPosition = juce::jlimit (t.now - (double) t.size + 8.0, t.now - 1.0, t.readPosition);
                    const auto fadeIn = t.fade > 0 ? 1.0f - (float) t.fade / (float) fadeSamples : 1.0f;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto y = t.at (c, t.readPosition);
                        if (t.fade > 0)
                            y = y * fadeIn + t.at (c, juce::jlimit (t.now - (double) t.size + 8.0, t.now - 1.0, t.oldReadPosition)) * (1.0f - fadeIn);
                        finish (c, i, chans[c][i], y);
                    }
                    if (t.fade > 0)
                    {
                        --t.fade;
                        t.oldReadPosition += speed;
                    }
                    t.readPosition += speed;
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::gate:
            {
                // Mutes the sound once it falls below THRESHOLD (-80 to 0 dB):
                // open within half a millisecond, HOLD (0-500 ms), then RELEASE
                // (5 ms to 2 s, a fade in dB). HPF (the STEREO knob): the gate
                // listens through a high-pass (20 Hz, off, to 2 kHz), so only
                // the higher sounds open it.
                const auto openAt = juce::Decibels::decibelsToGain (gateThresholdDb (main));
                const auto closeAt = openAt * 0.63f;   // 4 dB of hysteresis
                const auto holdSamples = (int) (p1 * 0.5f * sr);
                const auto releaseSamples = juce::jmax (1.0f, 5.0f * std::pow (400.0f, p2) * 0.001f * sr);
                const auto fall = std::pow (10.0f, -90.0f / 20.0f / releaseSamples);
                const auto openStep = 1.0f / (0.0005f * sr);
                const auto detectorFall = std::exp (-1.0f / (0.01f * sr));
                const auto sidechain = slot.stereo > -0.99f;
                const auto sideCoeff = onePole (20.0f * std::pow (100.0f, (slot.stereo + 1.0f) * 0.5f));
                for (int i = 0; i < count; ++i)
                {
                    auto level = 0.0f;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto x = chans[c][i];
                        if (sidechain)
                        {
                            t.gateSide[c] += sideCoeff * (x - t.gateSide[c]);
                            x -= t.gateSide[c];
                        }
                        level = juce::jmax (level, std::abs (x));
                    }
                    t.gateLevel = juce::jmax (level, t.gateLevel * detectorFall);
                    if (t.gateLevel >= openAt)
                    {
                        t.gateOpen = true;
                        t.gateHold = holdSamples;
                    }
                    else if (t.gateOpen && t.gateLevel < closeAt)
                    {
                        if (t.gateHold > 0)
                            --t.gateHold;
                        else
                            t.gateOpen = false;
                    }
                    if (t.gateOpen)
                        t.gateGain = juce::jmin (1.0f, t.gateGain + openStep);
                    else
                        t.gateGain = t.gateGain > 3.2e-5f ? t.gateGain * fall : 0.0f;
                    for (int c = 0; c < numChannels; ++c)
                        finish (c, i, chans[c][i], chans[c][i] * t.gateGain);
                }
                break;
            }

            //------------------------------------------------------------------
            case Type::compress:
            {
                // The master's compressors as an effect (COMPRESS is the main
                // knob, so an envelope can pump it); MIX gives parallel compression.
                float dryCopy[2][tick];
                for (int c = 0; c < numChannels; ++c)
                    std::copy (chans[c], chans[c] + count, dryCopy[c]);
                t.compressor.process (chans, numChannels, count, (Comp) mode, main);
                for (int c = 0; c < numChannels; ++c)
                    for (int i = 0; i < count; ++i)
                    {
                        const auto wetSample = chans[c][i];
                        chans[c][i] = dryCopy[c][i];
                        finish (c, i, dryCopy[c][i], wetSample);
                    }
                break;
            }

            case Type::off: case Type::ladderLp: case Type::ladderBp: case Type::ladderHp: case Type::digitalLp: case Type::digitalBp:
            case Type::digitalHp: case Type::oneOhOneLp: case Type::oneOhOneBp: case Type::oneOhOneHp: case Type::model20Lp:
            case Type::model20Bp: case Type::model20Hp: case Type::notch: case Type::dualFilter: case Type::formant: case Type::eqBell:
            case Type::eqLowShelf: case Type::eqHighShelf: case Type::gain: case Type::pan:
                break;
        }
        (void) index;
    }
}
