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
                return { "CUTOFF", "RESONANCE", "", true, false };
            case Type::oneOhOneLp: case Type::oneOhOneBp: case Type::oneOhOneHp:
                return { "CUTOFF", "RESONANCE", "DRIVE", false, false };
            case Type::model20Lp: case Type::model20Bp: case Type::model20Hp:
                return { "CUTOFF", "RESONANCE", "DRIVE", true, false };
            case Type::notch:      return { "CUTOFF", "WIDTH", "", false, false };
            case Type::dualFilter: return { "CUTOFF", "RESONANCE", "BANDWIDTH", false, false };
            case Type::formant:    return { "VOWEL", "Q", "SHIFT", false, false };
            case Type::eqBell: case Type::eqLowShelf: case Type::eqHighShelf:
                return { "FREQUENCY", "GAIN", "Q", false, false };
            case Type::gain:       return { "GAIN", "", "", false, false };
            case Type::pan:        return { "POSITION", "WIDTH", "", false, true };
            case Type::off:        break;
        }
        return { "", "", "", false, false };
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

    juce::String Cipher::mainText (Type type, float main01)
    {
        const auto hz = [] (float f) { return f < 1000.0f ? juce::String (juce::roundToInt (f)) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz"; };
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
        return juce::String (juce::roundToInt (p * 100.0f)) + "%";
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
        // The compressors: the detector (dB) and, for the multiband ones, three bands.
        float envelopeDb = -120.0f, slowDb = -120.0f, limiterGain = 1.0f;
        std::array<float, 3> bandDb { -120.0f, -120.0f, -120.0f };
        juce::dsp::LinkwitzRileyFilter<float> split1, split2, allpass1;   // 200 Hz, 2.5 kHz, and the low band's matching all-pass
        juce::dsp::LinkwitzRileyFilter<float> bassDry, bassWet;           // CLEAN BASS
        juce::AudioBuffer<float> bands;
    };

    Cipher::Cipher()
    {
        for (auto& s : slotStates)
            s = std::make_unique<SlotState>();
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
        master->bands.setSize (6, maxBlockSize);
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        for (auto* f : { &master->split1, &master->split2, &master->allpass1, &master->bassDry, &master->bassWet })
            f->prepare (spec);
        master->split1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        master->split2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        master->allpass1.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        master->bassDry.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        master->bassWet.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        master->split1.setCutoffFrequency (200.0f);
        master->split2.setCutoffFrequency (2500.0f);
        master->allpass1.setCutoffFrequency (2500.0f);
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
        for (auto* f : { &master->split1, &master->split2, &master->allpass1, &master->bassDry, &master->bassWet })
            f->reset();
        master->envelopeDb = master->slowDb = -120.0f;
        master->bandDb = { -120.0f, -120.0f, -120.0f };
        master->limiterGain = 1.0f;
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
                        float y = 0.0f;
                        switch (type)
                        {
                            case 0:  y = std::tanh (in + 0.15f) - std::tanh (0.15f); break;                       // Overdrive: soft, a little uneven
                            case 1:  y = in >= 0.0f ? 1.0f - std::exp (-in) : -0.7f * (1.0f - std::exp (in / 0.7f)); break;   // Tube: uneven halves
                            case 2:  y = (in >= 0.0f ? 1.0f : -1.0f) * (1.0f - std::exp (-4.0f * std::abs (in))); break;  // Fuzz: square-ish
                            case 3:  y = juce::jlimit (-1.0f, 1.0f, in); break;                                   // Hard Clip
                            case 4:  y = std::sin (in * pi * 0.5f); break;                                        // Foldback: folds back past full
                            default: y = 2.0f * std::abs (std::tanh (in)) - 0.5f; break;                          // Rectify: an octave up
                        }
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
                const auto amount = juce::jlimit (0.0f, 1.0f, m.compress);
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
                float* chans[2] = { wet.getChannelPointer (0), wet.getChannelPointer ((size_t) (numChannels - 1)) };

                if (m.comp == Comp::squash || m.comp == Comp::maximizer)
                {
                    // Squash: fast and hard, no look-ahead. Maximizer: a slower,
                    // smoother compressor then a limiter that never lets a peak
                    // past -0.5 dBFS. (The original Maximizer looks 3 ms ahead;
                    // ours doesn't, so it adds no latency.)
                    const auto squash = m.comp == Comp::squash;
                    const auto thresholdDb = squash ? -36.0f * amount : -30.0f * amount;
                    const auto ratio = squash ? 4.0f + 16.0f * amount : 4.0f;
                    const auto makeUpDb = squash ? -thresholdDb * (1.0f - 1.0f / ratio) * 0.6f : 12.0f * amount;
                    const auto attack = coeff (squash ? 0.3f : 10.0f), release = coeff (squash ? 80.0f : 150.0f);
                    const auto ceiling = juce::Decibels::decibelsToGain (-0.5f), limiterRelease = coeff (50.0f);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto peak = juce::jmax (std::abs (chans[0][i]), std::abs (chans[1][i]));
                        const auto levelDb = juce::Decibels::gainToDecibels (peak, -120.0f);
                        auto& env = master->envelopeDb;
                        env = levelDb > env ? levelDb + (env - levelDb) * attack : levelDb + (env - levelDb) * release;
                        auto gain = juce::Decibels::decibelsToGain (computeGainDb (env, thresholdDb, ratio, 6.0f) + makeUpDb);
                        if (! squash)
                        {
                            const auto out = peak * gain;
                            const auto wanted = out > ceiling ? ceiling / out : 1.0f;
                            auto& lg = master->limiterGain;
                            lg = wanted < lg ? wanted : wanted + (lg - wanted) * limiterRelease;
                            gain *= lg;
                        }
                        for (int c = 0; c < numChannels; ++c)
                            wet.getChannelPointer ((size_t) c)[i] *= gain;
                    }
                }
                else
                {
                    // Multiband: three bands (crossovers at 200 Hz and 2.5 kHz,
                    // our choice), each compressed hard. Overkill: the same bands
                    // brought up as well as down to one level, so the quiet parts,
                    // noise and all, come right up (the manual's "filthy artefacts").
                    const auto overkill = m.comp == Comp::overkill;
                    const auto attack = coeff (overkill ? 1.0f : 2.0f), release = coeff (overkill ? 30.0f : 100.0f);
                    const auto thresholdDb = overkill ? -6.0f - 30.0f * amount : -40.0f * amount;
                    const auto ratio = overkill ? 10.0f : 6.0f;
                    const auto makeUpDb = overkill ? 0.0f : -thresholdDb * (1.0f - 1.0f / ratio) * 0.5f;
                    auto& bands = master->bands;
                    for (int c = 0; c < numChannels; ++c)
                    {
                        auto* x = wet.getChannelPointer ((size_t) c);
                        for (int i = 0; i < numSamples; ++i)
                        {
                            float lowBand = 0.0f, rest = 0.0f, mid = 0.0f, high = 0.0f;
                            master->split1.processSample (c, x[i], lowBand, rest);
                            master->split2.processSample (c, rest, mid, high);
                            float lowAp = master->allpass1.processSample (c, lowBand);
                            bands.setSample (c * 3, i, lowAp);
                            bands.setSample (c * 3 + 1, i, mid);
                            bands.setSample (c * 3 + 2, i, high);
                        }
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
                            auto& env = master->bandDb[(size_t) b];
                            env = levelDb > env ? levelDb + (env - levelDb) * attack : levelDb + (env - levelDb) * release;
                            float gainDb;
                            if (overkill)
                                gainDb = juce::jlimit (-40.0f, 30.0f, (thresholdDb - env) * (1.0f - 1.0f / ratio));   // up and down
                            else
                                gainDb = computeGainDb (env, thresholdDb, ratio, 6.0f) + makeUpDb;
                            const auto gain = juce::Decibels::decibelsToGain (gainDb);
                            for (int c = 0; c < numChannels; ++c)
                                out[c] += bands.getSample (c * 3 + b, i) * gain;
                        }
                        for (int c = 0; c < numChannels; ++c)
                            wet.getChannelPointer ((size_t) c)[i] = overkill ? softClip (out[c]) : out[c];
                    }
                }
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
                const auto mode = slot.option >= 3 ? 0 : slot.option;   // a filter's 24 dB (the default) is PAN
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
