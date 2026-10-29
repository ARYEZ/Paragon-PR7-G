#include "Havok.h"

#include "PitchShifter.h"

// Ranges: where Korg's KC KAOSS PAD manual (the owner sent it) gives one for
// the same kind of effect, the pad sweeps it (filter cutoff 5.95 Hz to
// 21.7 kHz, flanger delay up to 50 ms, ring oscillator up to 12 kHz, pitch
// +-24 semitones, reverb time 0.2 to 10 s, LFOs up to 32 Hz). Everything
// else (curves, fixed times, levels) is our own guess.

namespace effects
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        constexpr int tickSamples = 16;   // control-rate updates (filters, LFO speeds), about 3 kHz

        // 0 to 1 onto lo..hi, evenly by ear (logarithmically).
        float sweep (float position, float lo, float hi) { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, position)); }
        float lerp (float a, float b, float t) { return a + (b - a) * t; }

        constexpr float cutoffLowHz = 5.95f, cutoffHighHz = 21714.34f;   // Korg's filter range

        // The topology-preserving state-variable filter (low, band and high pass at once).
        struct Svf
        {
            float g = 0.1f, k = 1.4f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
            void set (float hz, float q, double sampleRate) noexcept
            {
                g = std::tan (pi * juce::jlimit (5.0f, 0.47f * (float) sampleRate, hz) / (float) sampleRate);
                k = 1.0f / juce::jmax (0.5f, q);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            struct Out { float low, band, high; };
            Out run (float x) noexcept
            {
                const auto v3 = x - ic2;
                const auto v1 = a1 * ic1 + a2 * v3;
                const auto v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                return { v2, v1 * k, x - k * v1 - v2 };   // band normalised to unity at its peak
            }
            void clear() noexcept { ic1 = ic2 = 0.0f; }
        };

        // A circular buffer read at any (fractional) delay.
        struct Line
        {
            std::vector<float> data;
            int mask = 0, write = 0;
            void prepare (int minimumSamples)
            {
                int size = 1;
                while (size < minimumSamples + 4)
                    size <<= 1;
                data.assign ((size_t) size, 0.0f);
                mask = size - 1;
                write = 0;
            }
            void clear() noexcept { std::fill (data.begin(), data.end(), 0.0f); }
            void push (float x) noexcept { data[(size_t) write] = x; write = (write + 1) & mask; }
            float at (int delay) const noexcept { return data[(size_t) ((write - 1 - delay) & mask)]; }
            // Cubic (Hermite) between samples: clean when the delay moves (pitch bends).
            float read (float delay) const noexcept
            {
                delay = juce::jlimit (1.0f, (float) (mask - 3), delay);
                const auto whole = (int) delay;
                const auto t = delay - (float) whole;
                const auto ym1 = at (whole - 1), y0 = at (whole), y1 = at (whole + 1), y2 = at (whole + 2);
                const auto c1 = 0.5f * (y1 - ym1);
                const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
                const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
                return ((c3 * t + c2) * t + c1) * t + y0;
            }
        };

        // Soft clipping without aliasing whistle: tanh by its antiderivative
        // (first-order ADAA), so a drive turned right up stays clean on top.
        struct Clipper
        {
            float last = 0.0f, lastF = 0.0f;
            static float logCosh (float x) noexcept
            {
                const auto a = std::abs (x);
                return a + std::log1p (std::exp (-2.0f * a)) - 0.693147f;
            }
            float run (float x) noexcept
            {
                const auto f = logCosh (x);
                const auto d = x - last;
                const auto y = std::abs (d) < 1.0e-4f ? std::tanh (0.5f * (x + last)) : (f - lastF) / d;
                last = x;
                lastF = f;
                return y;
            }
            void clear() noexcept { last = lastF = 0.0f; }
        };

        // A first-order all-pass: one stage of a phaser.
        struct AllPass
        {
            float z = 0.0f;
            float run (float x, float a) noexcept
            {
                const auto y = a * x + z;
                z = x - a * y;
                return y;
            }
        };

        // A vowel filter (the TALK and VOICE filters): three formants, swept
        // through u, o, a, e, i (classic adult formant tables; the sweep is ours).
        struct Vowel
        {
            Svf formants[3];
            static constexpr float table[5][3] = { { 300.0f, 870.0f, 2240.0f },    // u
                                                   { 450.0f, 800.0f, 2830.0f },    // o
                                                   { 730.0f, 1090.0f, 2440.0f },   // a
                                                   { 530.0f, 1840.0f, 2480.0f },   // e
                                                   { 270.0f, 2290.0f, 3010.0f } }; // i
            void set (float position, double sampleRate) noexcept
            {
                const auto p = juce::jlimit (0.0f, 0.999f, position) * 4.0f;
                const auto i = (int) p;
                const auto t = p - (float) i;
                for (int f = 0; f < 3; ++f)
                {
                    const auto hz = table[i][f] * std::pow (table[i + 1][f] / table[i][f], t);
                    formants[f].set (hz, f == 0 ? 6.0f : 9.0f, sampleRate);
                }
            }
            float run (float x) noexcept
            {
                return 4.0f * (formants[0].run (x).band + 0.7f * formants[1].run (x).band + 0.35f * formants[2].run (x).band);
            }
            void clear() noexcept { for (auto& f : formants) f.clear(); }
        };

        // The reverbs: a feedback delay network (eight lines through a
        // Householder matrix, damped in the loop) after four diffusing all-passes.
        struct Fdn
        {
            static constexpr int numLines = 8;
            Line lines[numLines];
            float lengths[numLines] {}, gains[numLines] {}, damp[numLines] {};
            float dampCoefficient = 0.3f;
            struct Diffuser { Line line; int length = 100; };
            Diffuser diffusers[4];
            float diffusion = 0.7f;
            Line preDelay;
            float preDelaySamples = 0.0f;
            double sampleRate = 48000.0;

            void prepare (double rate)
            {
                sampleRate = rate;
                for (auto& line : lines)
                    line.prepare ((int) (0.25 * rate));
                for (auto& d : diffusers)
                    d.line.prepare ((int) (0.03 * rate));
                preDelay.prepare ((int) (0.55 * rate));
            }
            void clear() noexcept
            {
                for (auto& line : lines) line.clear();
                for (auto& d : diffusers) d.line.clear();
                preDelay.clear();
                std::fill (std::begin (damp), std::end (damp), 0.0f);
            }
            // size: the room's scale; seconds: the time to fall 60 dB; brightHz: the loop's damping.
            void set (float size, float seconds, float brightHz, float diffuse, float preMs) noexcept
            {
                static constexpr float baseMs[numLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.3f, 67.1f, 73.3f };
                static constexpr float diffuseMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
                for (int i = 0; i < numLines; ++i)
                {
                    lengths[i] = juce::jmin ((float) lines[i].mask - 4.0f, baseMs[i] * size * 0.001f * (float) sampleRate);
                    gains[i] = std::pow (10.0f, -3.0f * lengths[i] / (juce::jmax (0.1f, seconds) * (float) sampleRate));
                }
                for (int i = 0; i < 4; ++i)
                    diffusers[i].length = juce::jmax (1, (int) (diffuseMs[i] * juce::jmin (1.4f, size) * 0.001f * (float) sampleRate));
                dampCoefficient = 1.0f - std::exp (-2.0f * pi * brightHz / (float) sampleRate);
                diffusion = diffuse;
                preDelaySamples = preMs * 0.001f * (float) sampleRate;
            }
            void run (float in, float& left, float& right) noexcept
            {
                preDelay.push (in);
                auto x = preDelaySamples >= 1.0f ? preDelay.read (preDelaySamples) : in;
                for (auto& d : diffusers)
                {
                    const auto delayed = d.line.at (d.length - 1);
                    const auto w = x - diffusion * delayed;
                    d.line.push (w);
                    x = delayed + diffusion * w;
                }
                float outs[numLines], sum = 0.0f;
                for (int i = 0; i < numLines; ++i)
                {
                    auto v = lines[i].read (lengths[i]) * gains[i];
                    damp[i] += dampCoefficient * (v - damp[i]);
                    outs[i] = damp[i];
                    sum += outs[i];
                }
                const auto householder = sum * (2.0f / (float) numLines);
                left = right = 0.0f;
                for (int i = 0; i < numLines; ++i)
                {
                    lines[i].push (outs[i] - householder + x * ((i & 1) ? -0.5f : 0.5f));
                    ((i & 1) ? right : left) += outs[i] * ((i & 2) ? -1.0f : 1.0f);
                }
                left *= 0.5f;
                right *= 0.5f;
            }
        };

        struct ProgramTable
        {
            std::array<Havok::Program, Havok::numPrograms> programs {{
                { "LOW PASS FILTER", "FREQ", "DI/FX BALANCE, RESONANCE", "Rub slowly" },
                { "LOW PASS FILTER", "FREQ", "RESONANCE", "Rub slowly" },
                { "WAH FILTER", "FREQ", "DI/FX BALANCE, LFO DEPTH", "Rub slowly" },
                { "DIST+VOICE FILTER", "FREQ", "DIST GAIN", "Rub fast with beat" },
                { "DIST+L.P FILTER", "FREQ", "DIST GAIN", "Rub fast with beat" },
                { "DIST+L.P FILTER", "FREQ", "RESONANCE", "Good for voice (Mic)" },
                { "PAN+FILTER", "PAN", "FREQ", "Rub (1.1 <-> 9.9)" },
                { "DIST+FILTER", "FREQ", "FX BALANCE, DIST GAIN", "Rub fast with beat" },
                { "ISOLATOR TYPE FILTER", "FREQ", "NO ASSIGN", "For \"Bass Cut\"" },
                { "DISTORTION", "GAIN", "LEVEL", "For \"HOLD\" use" },
                { "PHASER", "FREQ (MANUAL)", "DI/FX BALANCE, RESONANCE", "Rub slowly" },
                { "PHASER", "LFO SPEED", "FREQ (MANUAL)", "Rub slowly" },
                { "LFO WAH", "LFO SPEED", "FX BALANCE", "Rub fast with beat" },
                { "STEP PHASER", "LFO SPEED", "MOD STEP", "Rub slowly" },
                { "DIST+PHASER", "LFO SPEED", "DIST GAIN", "Rub slowly" },
                { "MANUAL FLANGER", "DELAY TIME", "RESONANCE", "Rub very slowly (L <-> R)" },
                { "MANUAL FLANGER", "DELAY TIME", "RESONANCE", "Rub very slowly (L <-> R)" },
                { "PITCH SHIFT", "Lch PITCH", "Rch PITCH", "Rub" },
                { "TREMOLO", "LFO SPEED", "LFO DEPTH", "Rub" },
                { "STEP PHASER", "LFO SPEED", "RESONANCE", "Rub slowly" },
                { "TAPE ECHO", "TIME", "FX LEVEL, FEEDBACK", "Rub (Bottom <-> Top)" },
                { "DELAY", "TIME", "FX LEVEL", "Tap with snare drum" },
                { "DELAY", "TIME", "FX LEVEL, FEEDBACK", "Rub" },
                { "STEREO TAPE ECHO", "TIME", "FREQ", "Rub or tap" },
                { "MEDIUM DELAY", "FEEDBACK", "FX LEVEL", "Rub fast" },
                { "LONG DELAY", "FEEDBACK", "FX LEVEL", "Rub fast" },
                { "X DELAY", "FEEDBACK", "FX LEVEL", "Rub or tap" },
                { "TALK FILTER+DELAY", "FILTER FREQ", "DELAY LEVEL", "Rub with beat" },
                { "DIST+FILTER+DELAY", "FREQ", "DELAY LEVEL", "Rub with beat" },
                { "DELAY+FILTER FOR DUB", "TIME", "FREQ", "Rub or tap" },
                { "REVERB (HALL)", "LOW CUT", "FX LEVEL", "Tap with snare drum" },
                { "GATE REVERB (HALL)", "LOW CUT", "FX LEVEL", "Tap with snare drum" },
                { "REVERB (PLATE)", "LOW CUT", "FX LEVEL", "Tap with snare drum" },
                { "GATE REVERB (ROOM)", "LOW CUT", "FX LEVEL", "Tap with snare drum" },
                { "DELAY+REVERB", "DELAY LEVEL", "REVERB LEVEL", "For \"HOLD\" use" },
                { "DELAY+REVERB", "DELAY TIME", "FX LEVEL", "For \"HOLD\" use" },
                { "TALK FILTER+REVERB", "FILTER FREQ", "FX LEVEL", "Rub fast with beat" },
                { "DIST+FILTER+REVERB", "FILTER FREQ", "REV LEVEL+RESONANCE", "Tap with snare drum" },
                { "REVERB+PAN", "PAN", "FX LEVEL", "Rub for panning" },
                { "REVERB", "REVERB TIME", "FX LEVEL", "For \"HOLD\" use" },
                { "STEP MODULATION", "MOD DEPTH", "FX LEVEL", "Rub slowly" },
                { "RING MOD+FILTER", "RING FREQ", "FILTER FREQ", "Tap at random" },
                { "RING+TALK MOD", "MOD SPEED", "RING FREQ", "As you like" },
                { "SPECIAL ECHO", "TIME", "FX LEVEL", "Rub at random" },
                { "RING+DELAY", "TIME", "RING+DELAY BALANCE", "Rub at random" },
                { "DELAY+TREMOLO", "TIME+LFO SPEED", "DLY+TREM X-FADE", "Rub in circle" },
                { "3D PAN", "PAN (L/R)", "NEAR/FAR", "Rub for panning" },
                { "VIBRATE", "SPEED+DEPTH", "FX BALANCE", "Rub (Bottom <-> Top)" },
                { "STEP FLANGER", "FREQ (MANUAL)", "FEEDBACK", "Rub slowly" },
                { "STEP PHASER", "LFO SPEED", "FX LEVEL", "Rub slowly" },
                { "SAMPLE/PLAY1", "LOOP TIME", "FX LEVEL", "Tap for playback" },
                { "SAMPLE/PLAY2", "FORWARD/REVERSE", "FX LEVEL", "Rub like scratch" },
                { "SAMPLE/PLAY3", "TIME STRETCH", "FX LEVEL", "Rub for \"TIME STRETCH\"" },
                { "SAMPLE/PLAY1 (AUTO REC)", "LOOP TIME", "FX LEVEL", "Tap for playback" },
                { "SAMPLE/PLAY2 (AUTO REC)", "FORWARD/REVERSE", "FX LEVEL", "Rub like scratch" },
                { "SAMPLE/PLAY3 (AUTO REC)", "TIME STRETCH", "FX LEVEL", "Rub for \"TIME STRETCH\"" },
                { "SAMPLE/PLAY1 (DIRECT MIX)", "LOOP TIME", "FX LEVEL", "Tap for playback" },
                { "SAMPLE/PLAY2 (DIRECT MIX)", "FORWARD/REVERSE", "FX LEVEL", "Rub like scratch" },
                { "SAMPLE/PLAY1+PAN", "PAN", "LOOP TIME", "Tap (L <-> R)" },
                { "SAMPLE/PLAY2+PAN", "PAN", "FORWARD/REVERSE", "Tap (L <-> R)" },
                // Ours, not the KP1's: nu-metal scratching on our own sounds.
                { "SCRATCH BABY", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH CHIRP", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH TRANSFORMER", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH FLARE", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH TEAR", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH CRAB", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH SCRIBBLE", "RATE (1/2 - 1/32)", "FX LEVEL", "Touch on the beat" },
                { "SCRATCH MANUAL", "RECORD (HAND)", "CROSSFADER", "Rub like scratch, low Y cuts" } }};
        };
    }

    const std::array<Havok::Program, Havok::numPrograms>& Havok::programs()
    {
        static const ProgramTable table;
        return table.programs;
    }

    juce::StringArray Havok::programNames()
    {
        juce::StringArray names;
        for (size_t i = 0; i < programs().size(); ++i)
            names.add (juce::String ((int) i + 1) + " " + programs()[i].name);
        return names;
    }

    //==========================================================================
    struct Havok::Engine
    {
        double sampleRate = 48000.0;
        int current = 0;               // the program sounding
        float programGain = 1.0f;      // dips to switch programs without a click
        float programStep = 0.01f;
        float gate = 0.0f, gateUp = 0.1f, gateDown = 0.01f;   // touching (or HOLD): 0 to 1, smoothed
        float x = 0.5f, y = 0.5f, smoothing = 0.1f;           // the pad, smoothed at the control rate
        int tick = 0;
        bool wasActive = false;

        // Building blocks, two of each (left, right).
        Svf filter[2], filter2[2], loopFilter[2], lowCut[2], tone[2];
        Vowel vowel[2];
        Clipper clip[2];
        AllPass stages[2][8];
        float phaserLast[2] {};
        Line delay[2];
        float echo[2] {};
        Fdn reverb;
        PitchShifter shifter[2], shifter2[2];   // two in a row, each half the interval (clean to +-24 semitones)
        juce::Random random { 2001 };

        // Control values, set each tick by the program.
        float mix = 1.0f, drive = 1.0f, makeUp = 1.0f, outGain = 1.0f, level = 0.0f, feedback = 0.0f;
        float pan = 0.5f, rateHz = 1.0f, depth = 0.0f, steps = 8.0f, ringHz = 440.0f, xfade = 0.0f;
        float revLevel = 0.0f, dlyLevel = 0.0f, centreHz = 1000.0f, manual = 0.5f;
        float pitchL = 1.0f, pitchR = 1.0f;
        float timeL = 10000.0f, timeR = 10000.0f;   // wanted delay, samples
        float delayNow[2] { 10000.0f, 10000.0f }, delayGlide = 0.001f;
        float lfo = 0.0f, lfo2 = 0.0f, ring = 0.0f, stepValue = 0.0f;
        float gateFollower = 0.0f, gateOpen = 0.0f, gateHold = 0.0f, gateHoldSamples = 0.0f;   // the gate reverbs
        int reverseCounter = 0, reverseLength = 24000;

        // The sampler.
        std::vector<float> phrase[2];
        int phraseLength = 0, recordWrite = 0;
        Rec rec = Rec::ready;
        double playHead = 0.0, grainAnchor = 0.0;
        float scratchRate = 0.0f, grainPhase = 0.0f;

        // The scratch programs: eight "records" of our own sounds (made in
        // prepare), the hand's place on the record and the crossfader.
        std::vector<float> records[8];
        double recordPosition = 0.0, scratchBeats = 0.0, manualAnchor = 0.0, manualTarget = 0.0;
        float faderGain = 0.0f, faderStep = 0.01f, handLagAuto = 0.1f, handLagManual = 0.01f;
        Svf scratchDcCut;   // a record standing still under the needle is silent

        void prepare (double rate, int)
        {
            sampleRate = rate;
            programStep = 1.0f / (0.008f * (float) rate);    // 8 ms each way
            gateUp = 1.0f - std::exp (-1.0f / (0.002f * (float) rate));
            gateDown = 1.0f - std::exp (-1.0f / (0.008f * (float) rate));
            smoothing = 1.0f - std::exp (-(float) tickSamples / (0.02f * (float) rate));
            delayGlide = 1.0f - std::exp (-1.0f / (0.12f * (float) rate));   // the time slides like tape when rubbed
            for (auto& line : delay)
                line.prepare ((int) (5.1 * rate));
            reverb.prepare (rate);
            for (auto& s : shifter)
                s.prepare (rate);
            for (auto& s : shifter2)
                s.prepare (rate);
            for (auto& p : phrase)
                p.assign ((size_t) (maxRecordSeconds * rate) + 1, 0.0f);
            makeRecords();
            faderStep = 1.0f / (0.0007f * (float) rate);                       // a crossfader cut: sharp, without a click
            handLagAuto = 1.0f - std::exp (-1.0f / (0.0015f * (float) rate));
            handLagManual = 1.0f - std::exp (-1.0f / (0.012f * (float) rate)); // a platter's lag under the hand (ours)
            scratchDcCut.set (40.0f, 0.7f, rate);
            gateHoldSamples = 0.0f;
            clear();
        }

        void clear() noexcept
        {
            for (int c = 0; c < 2; ++c)
            {
                filter[c].clear(); filter2[c].clear(); loopFilter[c].clear(); lowCut[c].clear(); tone[c].clear();
                vowel[c].clear(); clip[c].clear();
                for (auto& s : stages[c]) s.z = 0.0f;
                phaserLast[c] = echo[c] = 0.0f;
                delay[c].clear();
                shifter[c].reset();
                shifter2[c].reset();
                delayNow[c] = timeL;
            }
            reverb.clear();
            lfo = lfo2 = ring = stepValue = 0.0f;
            gateFollower = gateOpen = gateHold = 0.0f;
            reverseCounter = 0;
            recordPosition = manualTarget = cueSamples();
            scratchBeats = 0.0;
            faderGain = 0.0f;
            scratchDcCut.clear();
        }

        void erasePhrase() noexcept
        {
            phraseLength = recordWrite = 0;
            rec = Rec::ready;
            playHead = grainAnchor = 0.0;
        }

        float ms (float milliseconds) const noexcept { return milliseconds * 0.001f * (float) sampleRate; }

        // The program's controls from the pad (each tick): the manual's X and
        // Y assignments, with our ranges.
        void control (int p) noexcept
        {
            const auto sr = sampleRate;
            mix = 1.0f; outGain = 1.0f; level = 0.0f; feedback = 0.0f;
            const auto distortion = [this] (float amount)   // 0 to 1: gain from 0 to 42 dB (Korg's drive range)
            {
                drive = std::pow (10.0f, 42.0f * amount / 20.0f);
                makeUp = 0.2f / std::tanh (0.2f * drive);   // about the dry level once it clips
            };
            const auto lowPass = [&] (float position, float q)
            {
                for (auto& f : filter) f.set (sweep (position, cutoffLowHz, cutoffHighHz), q, sr);
            };
            const auto talk = [&] (float position) { for (auto& v : vowel) v.set (position, sr); };
            const auto reverbType = [&] (int type, float seconds)
            {
                // 0 hall, 1 plate, 2 room (sizes, damping and pre-delays are ours)
                if (type == 0)      reverb.set (1.5f, seconds, 6500.0f, 0.7f, 22.0f);
                else if (type == 1) reverb.set (0.85f, seconds, 10000.0f, 0.75f, 0.0f);
                else                reverb.set (0.5f, seconds, 5000.0f, 0.65f, 4.0f);
            };
            const auto delayTime = [&] (float position) { return ms (sweep (position, 10.0f, 2000.0f)); };
            const auto lfoSpeed = [] (float position) { return sweep (position, 0.05f, 32.0f); };

            switch (p)
            {
                case 0: lowPass (x, lerp (0.7f, 9.0f, y)); mix = lerp (0.35f, 1.0f, y); break;
                case 1: lowPass (x, lerp (0.7f, 14.0f, y)); break;
                case 2:   // a wah: the band-pass sweeps by hand, an LFO (2.5 Hz, ours) joins in up the pad
                    rateHz = 2.5f; depth = y; mix = lerp (0.35f, 1.0f, y);
                    manual = x;   // the hand's position; the LFO is added per sample
                    break;
                case 3: distortion (y); talk (x); break;
                case 4: distortion (y); lowPass (x, 4.0f); break;
                case 5: distortion (0.45f); lowPass (x, lerp (0.7f, 12.0f, y)); break;
                case 6: pan = x; lowPass (y, 3.0f); break;
                case 7: distortion (y); lowPass (x, 5.0f); mix = lerp (0.35f, 1.0f, y); break;
                case 8:   // isolator-steep (24 dB per octave) bass cut, 20 Hz to 6 kHz
                    for (auto& f : filter) f.set (sweep (x, 20.0f, 6000.0f), 0.54f, sr);
                    for (auto& f : filter2) f.set (sweep (x, 20.0f, 6000.0f), 1.31f, sr);
                    break;
                case 9: distortion (x); outGain = juce::Decibels::decibelsToGain (lerp (-18.0f, 9.0f, y)); break;
                case 10: centreHz = sweep (x, 80.0f, 6000.0f); mix = lerp (0.35f, 1.0f, y); feedback = 0.85f * y; break;
                case 11: rateHz = lfoSpeed (x); centreHz = sweep (y, 80.0f, 6000.0f); feedback = 0.5f; break;
                case 12: rateHz = lfoSpeed (x); mix = y; for (auto& f : filter) f.set (1000.0f, 6.0f, sr); break;
                case 13: rateHz = lfoSpeed (x); steps = std::round (lerp (16.0f, 2.0f, y)); feedback = 0.5f; break;
                case 14: rateHz = lfoSpeed (x); distortion (y); feedback = 0.5f; break;
                case 15: case 16:   // by hand: 0.1 to 50 ms (Korg's flanger), positive or negative feedback
                    timeL = timeR = ms (sweep (x, 0.1f, 50.0f));
                    feedback = (p == 15 ? 0.95f : -0.95f) * y;
                    break;
                case 17:   // +-24 semitones on each side (a small dead zone at the centre: unison)
                {
                    const auto semis = [] (float v)
                    {
                        const auto c = (v - 0.5f) * 2.0f;
                        return std::abs (c) < 0.03f ? 0.0f : 24.0f * (c - std::copysign (0.03f, c)) / 0.97f;
                    };
                    pitchL = std::pow (2.0f, semis (x) / 12.0f);
                    pitchR = std::pow (2.0f, semis (y) / 12.0f);
                    break;
                }
                case 18: rateHz = sweep (x, 0.5f, 32.0f); depth = y; break;
                case 19: rateHz = lfoSpeed (x); steps = 8.0f; feedback = 0.9f * y; break;
                case 20:   // tape echo: level and feedback rise together
                    timeL = timeR = delayTime (x); level = y; feedback = lerp (0.15f, 0.8f, y);
                    for (auto& f : loopFilter) f.set (3200.0f, 0.6f, sr);
                    break;
                case 21: timeL = timeR = delayTime (x); level = y; feedback = 0.35f; break;
                case 22: timeL = timeR = delayTime (x); level = y; feedback = 0.85f * y; break;
                case 23:   // stereo: the right side's time three quarters of the left's
                    timeL = delayTime (x); timeR = timeL * 0.75f; level = 0.7f; feedback = 0.5f;
                    for (auto& f : loopFilter) f.set (sweep (y, 200.0f, 12000.0f), 0.7f, sr);
                    break;
                case 24: timeL = timeR = ms (300.0f); feedback = 0.92f * x; level = y; break;
                case 25: timeL = timeR = ms (650.0f); feedback = 0.92f * x; level = y; break;
                case 26: timeL = ms (300.0f); timeR = ms (450.0f); feedback = 0.92f * x; level = y; break;
                case 27: talk (x); timeL = timeR = ms (320.0f); feedback = 0.45f; level = y; break;
                case 28: distortion (0.5f); lowPass (x, 4.0f); timeL = timeR = ms (320.0f); feedback = 0.45f; level = y; break;
                case 29:   // dub: a band-pass in the loop, the repeats running long
                    timeL = timeR = delayTime (x); feedback = 0.78f; level = 0.8f;
                    for (auto& f : loopFilter) f.set (sweep (y, 150.0f, 6000.0f), 2.0f, sr);
                    break;
                case 30: case 31: case 32: case 33:
                {
                    const int type[] = { 0, 0, 1, 2 };
                    const float seconds[] = { 2.8f, 1.6f, 2.2f, 1.1f };
                    reverbType (type[p - 30], seconds[p - 30]);
                    for (auto& f : lowCut) f.set (sweep (x, 20.0f, 2000.0f), 0.7f, sr);
                    level = y;
                    gateHoldSamples = p == 31 ? ms (300.0f) : p == 33 ? ms (180.0f) : 0.0f;
                    break;
                }
                case 34: reverbType (0, 2.5f); timeL = timeR = ms (380.0f); feedback = 0.4f; dlyLevel = x; revLevel = y; break;
                case 35: reverbType (0, 2.5f); timeL = timeR = delayTime (x); feedback = 0.4f; dlyLevel = y; revLevel = 0.6f * y; break;
                case 36: talk (x); reverbType (1, 2.0f); revLevel = y; break;
                case 37: distortion (0.55f); lowPass (x, lerp (1.0f, 10.0f, y)); reverbType (2, 1.5f); revLevel = y; break;
                case 38: reverbType (0, 2.5f); pan = x; revLevel = y; break;
                case 39: reverbType (0, sweep (x, 0.2f, 10.0f)); revLevel = y; break;   // Korg's reverb time range
                case 40: depth = x; level = y; rateHz = 6.0f; break;
                case 41: ringHz = sweep (x, 1.0f, 12000.0f); lowPass (y, 2.0f); break;
                case 42: rateHz = sweep (x, 0.1f, 16.0f); ringHz = sweep (y, 1.0f, 12000.0f); break;
                case 43:
                {
                    const auto wanted = (int) delayTime (x);
                    if (reverseCounter == 0)
                        reverseLength = juce::jlimit ((int) ms (40.0f), (int) ms (2400.0f), wanted);
                    level = y; feedback = 0.35f;
                    break;
                }
                case 44: timeL = timeR = delayTime (x); feedback = 0.45f; ringHz = 330.0f; xfade = y; level = 0.8f; break;
                case 45:   // the tremolo's cycle follows the delay time: one throb per repeat
                    timeL = timeR = ms (sweep (x, 60.0f, 1000.0f));
                    rateHz = (float) sampleRate / timeL; feedback = 0.4f; xfade = y; level = 0.8f;
                    break;
                case 46:
                    pan = x; level = y;   // up the pad nearer: louder, brighter, drier
                    for (auto& f : filter) f.set (sweep (y, 1500.0f, 20000.0f), 0.7f, sr);
                    reverbType (2, 0.9f);
                    break;
                case 47: rateHz = sweep (x, 0.5f, 14.0f); depth = x; mix = y; break;
                case 48: manual = x; rateHz = 2.0f; steps = 8.0f; feedback = 0.92f * y; break;
                case 49: rateHz = lfoSpeed (x); steps = 8.0f; level = y; feedback = 0.5f; break;
                default: break;   // the sampler reads the pad directly
            }
        }

        //======================================================================
        // The scratch programs (61-68, ours). Each "record" holds silence,
        // then the sound from its cue point on, as a DJ marks a sample.
        static constexpr double recordSeconds = 2.0, cueSeconds = 0.15, strokeSeconds = 0.3;
        double cueSamples() const noexcept { return cueSeconds * sampleRate; }

        void makeRecords()
        {
            const auto length = (int) (recordSeconds * sampleRate);
            const auto sr = (float) sampleRate;
            const auto cue = (int) cueSamples();
            juce::Random noise (61);
            for (auto& record : records)
                record.assign ((size_t) length, 0.0f);

            // A band-limited saw (polyBLEP), for the voice and the chord.
            const auto saw = [] (float& phase, float increment)
            {
                phase += increment;
                if (phase >= 1.0f) phase -= 1.0f;
                auto value = 2.0f * phase - 1.0f;
                if (phase < increment)               { const auto t = phase / increment; value -= t + t - t * t - 1.0f; }
                else if (phase > 1.0f - increment)   { const auto t = (phase - 1.0f) / increment; value -= t * t + t + t + 1.0f; }
                return value;
            };
            const auto envelope = [sr] (int n, float attack, float decay)
            {
                const auto t = (float) n / sr;
                return juce::jmin (1.0f, t / attack) * std::exp (-t / decay);
            };

            // AHH: a shouted "ah" (a voice at about 200 Hz through the "a" formants).
            {
                Svf formants[3];
                const float hz[] = { 730.0f, 1090.0f, 2440.0f }, gains[] = { 1.0f, 0.6f, 0.3f };
                for (int f = 0; f < 3; ++f) formants[f].set (hz[f], 7.0f, sampleRate);
                float phase = 0.0f;
                for (int n = 0; n + cue < length; ++n)
                {
                    const auto t = (float) n / sr;
                    const auto pitch = 200.0f * (1.0f + 0.012f * std::sin (2.0f * pi * 5.5f * t));
                    const auto source = saw (phase, pitch / sr) + 0.08f * (noise.nextFloat() * 2.0f - 1.0f);
                    float voice = 0.0f;
                    for (int f = 0; f < 3; ++f) voice += gains[f] * formants[f].run (source).band;
                    records[0][(size_t) (n + cue)] = voice * envelope (n, 0.012f, 0.9f) * (t < 1.2f ? 1.0f : juce::jmax (0.0f, 1.0f - (t - 1.2f) / 0.1f));
                }
            }
            // LASER: a steady tone, the classic sound to scratch (900 Hz, a little edge).
            for (int n = 0; n + cue < length; ++n)
            {
                const auto t = (float) n / sr;
                records[1][(size_t) (n + cue)] = (std::sin (2.0f * pi * 900.0f * t) + 0.12f * std::sin (2.0f * pi * 2700.0f * t))
                                                 * juce::jmin (1.0f, t / 0.002f);
            }
            // NOISE: a breathy burst, brightest in the middle.
            {
                Svf band;
                band.set (2200.0f, 0.8f, sampleRate);
                for (int n = 0; n + cue < length; ++n)
                    records[2][(size_t) (n + cue)] = band.run (noise.nextFloat() * 2.0f - 1.0f).band * envelope (n, 0.002f, 0.7f);
            }
            // The stabs: distorted chords and notes, hit hard. Each: its notes
            // (Hz), how hard it's driven, its top end and how quickly it dies.
            const auto stab = [&] (std::vector<float>& record, std::initializer_list<float> notes, float drive, float topHz, float decay)
            {
                Svf tone;
                tone.set (topHz, 0.7f, sampleRate);
                float phases[4] {};
                for (int n = 0; n + cue < length; ++n)
                {
                    float chord = 0.0f;
                    int k = 0;
                    for (const auto hz : notes)
                        chord += saw (phases[k++], hz / sr);
                    record[(size_t) (n + cue)] = tone.run (std::tanh (drive * chord)).low * envelope (n, 0.003f, decay);
                }
            };
            stab (records[3], { 82.41f, 123.47f, 164.81f }, 2.5f, 3500.0f, 0.6f);    // STAB (CHORD): a low E power chord
            stab (records[4], { 73.42f, 110.0f, 146.83f }, 2.5f, 3500.0f, 0.6f);     // STAB (DROP D): D power chord, as in drop-D tuning
            stab (records[5], { 61.74f, 92.5f, 123.47f }, 3.0f, 3000.0f, 0.6f);      // STAB (LOW B): B power chord, tuned right down
            stab (records[6], { 82.41f, 123.47f, 164.81f }, 3.5f, 5500.0f, 0.15f);   // STAB (SHORT): shorter, brighter, punchier
            stab (records[7], { 73.42f, 146.83f }, 6.0f, 2800.0f, 0.5f);             // GRIT (LOW NOTE): one low D and its octave, driven hard
            // Each as loud as the others (by its average over the first 0.4 s
            // after the cue, so a steady tone isn't louder than a voice),
            // never peaking over 0.9.
            for (auto& record : records)
            {
                double sum = 0.0;
                const auto span = (int) (0.4 * sampleRate);
                for (int n = cue; n < cue + span; ++n)
                    sum += (double) record[(size_t) n] * record[(size_t) n];
                const auto average = (float) std::sqrt (sum / span);
                const auto range = juce::FloatVectorOperations::findMinAndMax (record.data(), (int) record.size());
                const auto peak = juce::jmax (-range.getStart(), range.getEnd(), 1.0e-6f);
                const auto gain = juce::jmin (0.22f / juce::jmax (average, 1.0e-6f), 0.9f / peak);
                juce::FloatVectorOperations::multiply (record.data(), gain, (int) record.size());
            }
        }

        float readRecord (int sound, double position) const noexcept
        {
            const auto& record = records[(size_t) juce::jlimit (0, 7, sound)];
            const auto last = (double) record.size() - 3.0;
            position = juce::jlimit (1.0, last, position);
            const auto i = (int) position;
            const auto t = (float) (position - i);
            const auto ym1 = record[(size_t) i - 1], y0 = record[(size_t) i], y1 = record[(size_t) i + 1], y2 = record[(size_t) i + 2];
            const auto c1 = 0.5f * (y1 - ym1);
            const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * t + c2) * t + c1) * t + y0;
        }

        // The patterns: over one back-and-forth (phase 0 to 1), where the
        // hand has the record (0 at the cue, 1 a stroke on) and whether the
        // crossfader is open. Our versions of the classic moves.
        static void pattern (int move, double phase, float& stroke, bool& open) noexcept
        {
            const auto smooth = [] (double t) { return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * juce::jlimit (0.0, 1.0, t))); };
            const auto half = std::fmod (phase * 2.0, 1.0);   // where in this stroke (forward or back)
            const auto baby = phase < 0.5 ? smooth (phase * 2.0) : smooth (2.0 - phase * 2.0);
            stroke = baby;
            open = true;
            switch (move)
            {
                case 0: break;                                                   // BABY: back and forth, fader open
                case 1: open = half < 0.5; break;                                // CHIRP: each stroke's start, cut in its middle
                case 2: open = std::fmod (phase * 8.0, 1.0) < 0.45; break;       // TRANSFORMER: eight fast cuts over the stroke
                case 3: open = half < 0.42 || half > 0.58; break;                // FLARE: open, clicked shut mid-stroke
                case 4:                                                          // TEAR: out in one, back in two jerks
                    stroke = phase < 0.4 ? smooth (phase / 0.4)
                           : phase < 0.6 ? 1.0f - 0.5f * smooth ((phase - 0.4) / 0.2)
                           : phase < 0.7 ? 0.5f
                                         : 0.5f - 0.5f * smooth ((phase - 0.7) / 0.3);
                    break;
                case 5: open = half < 0.7 && std::fmod (half * 8.0, 1.0) < 0.5; break;   // CRAB: four fingers' quick taps each stroke
                case 6:                                                          // SCRIBBLE: tense, tiny, fast wiggles riding a slow push
                    stroke = 0.35f * smooth (phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0)
                           + 0.08f * (float) std::sin (juce::MathConstants<double>::twoPi * phase * 8.0);
                    break;
                default: break;
            }
        }

        // One sample of a scratch program: your playing passes, the scratch on top.
        void scratch (int p, const Settings& settings, double beatsPerSample, int i, bool active, float inL, float inR,
                      float& outL, float& outR) noexcept
        {
            const auto move = p - firstScratch;
            const auto manual = move == 7;
            const auto stroke = strokeSeconds * sampleRate;
            float target = 0.0f;
            bool open = false;
            if (manual)
            {
                // The hand grabs the record where it is and moves it: across the
                // whole pad is a second of record. Y is the crossfader, at once
                // (the unsmoothed pad, as a fader is quick).
                if (active && ! wasActive)
                    manualAnchor = recordPosition - (double) settings.x * sampleRate;
                if (active)
                    manualTarget = juce::jlimit (cueSamples() * 0.5, recordSeconds * sampleRate - 4.0, manualAnchor + (double) settings.x * sampleRate);
                open = active && settings.y > 0.3f;
                recordPosition += (manualTarget - recordPosition) * handLagManual;
            }
            else
            {
                // In time with the song: on its grid while the DAW plays, else from the touch.
                static constexpr double cycleBeats[] = { 2.0, 1.0, 0.5, 0.25, 0.125 };
                auto cycle = cycleBeats[scratchDivision (x)];
                if (move == 2)
                    cycle *= 2.0;   // the transformer's hand moves slowly under its fast cuts
                if (active && ! wasActive && settings.songBeats < 0.0)
                    scratchBeats = 0.0;
                const auto beats = settings.songBeats >= 0.0 ? settings.songBeats + beatsPerSample * i : scratchBeats;
                scratchBeats += beatsPerSample;
                float shape = 0.0f;
                pattern (move, std::fmod (beats / cycle, 1.0), shape, open);
                open = open && active;
                target = (float) (cueSamples() + shape * stroke);
                recordPosition += ((double) target - recordPosition) * handLagAuto;
            }
            faderGain = open ? juce::jmin (1.0f, faderGain + faderStep) : juce::jmax (0.0f, faderGain - faderStep);
            const auto sound = scratchDcCut.run (readRecord (settings.scratchSound, recordPosition)).high;
            const auto level = manual ? 1.0f : y;
            const auto wet = sound * faderGain * level;
            outL = inL + wet;
            outR = inR + wet;
        }

        static float panLeft (float p) noexcept { return std::cos (0.5f * pi * p) * 1.4142f; }
        static float panRight (float p) noexcept { return std::sin (0.5f * pi * p) * 1.4142f; }

        float stepped (float phase) const noexcept
        {
            // A triangle LFO held in `steps` levels.
            const auto tri = phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
            return std::floor (tri * steps) / juce::jmax (1.0f, steps - 1.0f);
        }

        float phaser (int c, float in, float hz, int numStages) noexcept
        {
            const auto t = std::tan (pi * juce::jlimit (20.0f, 0.45f * (float) sampleRate, hz) / (float) sampleRate);
            const auto a = (t - 1.0f) / (t + 1.0f);
            auto v = in + feedback * phaserLast[c];
            for (int s = 0; s < numStages; ++s)
                v = stages[c][s].run (v, a);
            phaserLast[c] = juce::jlimit (-2.0f, 2.0f, v);
            return 0.5f * (in + v);
        }

        float advance (float& phase, float hz) const noexcept
        {
            phase += hz / (float) sampleRate;
            phase -= std::floor (phase);
            return phase;
        }

        // One sample of programs 1-50: dry in, out. `g` is the touch (0 to 1).
        void runEffect (int p, float g, float inL, float inR, float& outL, float& outR) noexcept
        {
            const float in[2] = { inL, inR };
            float out[2] = { inL, inR };
            const auto insert = [&] (int c, float wet) { out[c] = in[c] + (lerp (in[c], wet, mix) - in[c]) * g; };
            const auto dist = [&] (int c, float v) { return clip[c].run (v * drive) * makeUp; };
            const auto moveDelay = [&]
            {
                delayNow[0] += (timeL - delayNow[0]) * delayGlide;
                delayNow[1] += (timeR - delayNow[1]) * delayGlide;
            };
            const auto echoLine = [&] (int c, float send, bool cross, bool tape)
            {
                const auto from = cross ? 1 - c : c;
                auto fed = send + feedback * echo[from];
                if (tape)
                    fed = std::tanh (loopFilter[c].run (fed).low * 1.2f) / 1.2f;
                delay[c].push (juce::jlimit (-4.0f, 4.0f, fed));
                return delay[c].read (delayNow[c]);
            };

            switch (p)
            {
                case 0: case 1:
                    for (int c = 0; c < 2; ++c) insert (c, filter[c].run (in[c]).low);
                    break;
                case 2:
                {
                    const auto wobble = std::sin (2.0f * pi * advance (lfo, rateHz)) * 0.5f * depth;
                    const auto hz = sweep (juce::jlimit (0.0f, 1.0f, manual + wobble * 0.5f), 350.0f, 2600.0f);
                    for (int c = 0; c < 2; ++c)
                    {
                        if (c == 0 && (tick & 3) == 0)
                            for (auto& f : filter) f.set (hz, 6.0f, sampleRate);
                        insert (c, filter[c].run (in[c]).band * 3.0f);
                    }
                    break;
                }
                case 3: for (int c = 0; c < 2; ++c) insert (c, vowel[c].run (dist (c, in[c]))); break;
                case 4: case 5: case 7:
                    for (int c = 0; c < 2; ++c) insert (c, filter[c].run (dist (c, in[c])).low);
                    break;
                case 6:
                {
                    const auto mono = 0.5f * (in[0] + in[1]);
                    const auto l = filter[0].run (mono).low;
                    insert (0, l * panLeft (pan));
                    insert (1, l * panRight (pan));
                    break;
                }
                case 8:
                    for (int c = 0; c < 2; ++c) insert (c, filter2[c].run (filter[c].run (in[c]).high).high);
                    break;
                case 9: for (int c = 0; c < 2; ++c) insert (c, dist (c, in[c]) * outGain); break;
                case 10:
                    for (int c = 0; c < 2; ++c) insert (c, phaser (c, in[c], centreHz, 8));
                    break;
                case 11: case 14:
                {
                    const auto phase = advance (lfo, rateHz);
                    const auto centre = p == 11 ? centreHz : 700.0f;
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto ph = c == 0 ? phase : std::fmod (phase + 0.25f, 1.0f);   // a little stereo (ours)
                        const auto hz = centre * std::pow (2.0f, 2.5f * std::sin (2.0f * pi * ph));
                        insert (c, phaser (c, p == 14 ? dist (c, in[c]) : in[c], hz, 8));
                    }
                    break;
                }
                case 12:
                {
                    const auto hz = sweep (0.5f + 0.5f * std::sin (2.0f * pi * advance (lfo, rateHz)), 300.0f, 2600.0f);
                    if ((tick & 3) == 0)
                        for (auto& f : filter) f.set (hz, 6.0f, sampleRate);
                    for (int c = 0; c < 2; ++c) insert (c, filter[c].run (in[c]).band * 3.0f);
                    break;
                }
                case 13: case 19: case 49:
                {
                    const auto hz = sweep (stepped (advance (lfo, rateHz)), 150.0f, 5000.0f);
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto wet = phaser (c, in[c], hz, 8);
                        if (p == 49)
                            out[c] = in[c] + (wet - in[c]) * level * g;
                        else
                            insert (c, wet);
                    }
                    break;
                }
                case 15: case 16: case 48:
                {
                    if (p == 48)
                    {
                        // STEP FLANGER: MANUAL sets the centre, the steps move it up to an octave each way.
                        const auto centreMs = sweep (manual, 0.3f, 12.0f);
                        timeL = timeR = ms (centreMs * std::pow (2.0f, 2.0f * stepped (advance (lfo, rateHz)) - 1.0f));
                        delayNow[0] += (timeL - delayNow[0]) * 0.02f;
                        delayNow[1] = delayNow[0];
                    }
                    else
                    {
                        delayNow[0] += (timeL - delayNow[0]) * 0.004f;
                        delayNow[1] = delayNow[0];
                    }
                    for (int c = 0; c < 2; ++c)
                    {
                        delay[c].push (juce::jlimit (-4.0f, 4.0f, in[c] + feedback * echo[c]));
                        echo[c] = delay[c].read (juce::jmax (1.0f, delayNow[c]));
                        insert (c, 0.5f * (in[c] + echo[c]));
                    }
                    break;
                }
                case 17:
                {
                    // Each of the two shifters moves half the interval: the
                    // shared shifter is at its cleanest within an octave.
                    const auto halfL = std::sqrt (pitchL), halfR = std::sqrt (pitchR);
                    insert (0, shifter2[0].processSample (shifter[0].processSample (in[0], halfL), halfL));
                    insert (1, shifter2[1].processSample (shifter[1].processSample (in[1], halfR), halfR));
                    break;
                }
                case 18:
                {
                    const auto gain = 1.0f - depth * (0.5f - 0.5f * std::cos (2.0f * pi * advance (lfo, rateHz)));
                    for (int c = 0; c < 2; ++c) insert (c, in[c] * gain);
                    break;
                }
                case 20: case 21: case 22: case 23: case 24: case 25: case 26:
                case 27: case 28: case 29:
                {
                    moveDelay();
                    float send[2] = { in[0], in[1] };
                    if (p == 27)
                        for (int c = 0; c < 2; ++c) { send[c] = vowel[c].run (in[c]); insert (c, send[c]); }
                    else if (p == 28)
                        for (int c = 0; c < 2; ++c) { send[c] = filter[c].run (dist (c, in[c])).low; insert (c, send[c]); }
                    const auto tape = p == 20 || p == 23 || p == 29;
                    const auto cross = p == 26 || p == 23;
                    float wet[2];
                    for (int c = 0; c < 2; ++c)
                        wet[c] = echoLine (c, send[c] * g, cross, tape);
                    for (int c = 0; c < 2; ++c)
                    {
                        echo[c] = wet[c];
                        out[c] += wet[c] * level;
                    }
                    break;
                }
                case 30: case 31: case 32: case 33: case 36: case 37: case 38: case 39: case 46:
                {
                    float send[2] = { in[0], in[1] };
                    if (p == 36)
                        for (int c = 0; c < 2; ++c) { send[c] = vowel[c].run (in[c]); insert (c, send[c]); }
                    else if (p == 37)
                        for (int c = 0; c < 2; ++c) { send[c] = filter[c].run (dist (c, in[c])).low; insert (c, send[c]); }
                    else if (p == 38)
                    {
                        const auto mono = 0.5f * (in[0] + in[1]);
                        insert (0, mono * panLeft (pan));
                        insert (1, mono * panRight (pan));
                        send[0] = out[0]; send[1] = out[1];
                    }
                    else if (p == 46)
                    {
                        // 3D PAN: further down the pad, further away (quieter, darker, more room).
                        const auto mono = 0.5f * (in[0] + in[1]);
                        const auto near = lerp (0.3f, 1.0f, level);
                        const auto shaped = filter[0].run (mono).low * near;
                        insert (0, shaped * panLeft (pan));
                        insert (1, shaped * panRight (pan));
                        send[0] = send[1] = mono * (1.0f - level);
                    }
                    float l = 0.0f, r = 0.0f;
                    reverb.run (0.5f * (send[0] + send[1]) * g, l, r);
                    if (p <= 33)
                    {
                        l = lowCut[0].run (l).high;
                        r = lowCut[1].run (r).high;
                        if (gateHoldSamples > 0.0f)
                        {
                            // GATE REVERB: open while the sound comes in, held a moment, then shut.
                            const auto inLevel = std::abs (0.5f * (send[0] + send[1]) * g);
                            gateFollower = juce::jmax (inLevel, gateFollower * 0.9995f);
                            gateHold = gateFollower > 0.01f ? gateHoldSamples : juce::jmax (0.0f, gateHold - 1.0f);
                            gateOpen += ((gateHold > 0.0f ? 1.0f : 0.0f) - gateOpen) * (gateHold > 0.0f ? 0.05f : 0.004f);
                            l *= gateOpen;
                            r *= gateOpen;
                        }
                    }
                    const auto wetLevel = p <= 33 ? level : p == 46 ? 0.5f * (1.0f - level) : revLevel;
                    out[0] += l * wetLevel;
                    out[1] += r * wetLevel;
                    break;
                }
                case 34: case 35:
                {
                    moveDelay();
                    float wet[2];
                    for (int c = 0; c < 2; ++c)
                        wet[c] = echoLine (c, in[c] * g, false, false);
                    float l = 0.0f, r = 0.0f;
                    reverb.run (0.5f * (in[0] + in[1]) * g, l, r);
                    for (int c = 0; c < 2; ++c)
                    {
                        echo[c] = wet[c];
                        out[c] += wet[c] * dlyLevel + (c == 0 ? l : r) * revLevel;
                    }
                    break;
                }
                case 40:
                {
                    // STEP MODULATION: the pitch jumps between random steps, about six a second (ours).
                    advance (lfo2, rateHz);
                    if (lfo2 < rateHz / (float) sampleRate)
                        stepValue = random.nextFloat() * 2.0f - 1.0f;
                    ring += (stepValue - ring) * 0.004f;
                    const auto wanted = ms (8.0f + 6.0f * depth * ring);
                    for (int c = 0; c < 2; ++c)
                    {
                        delay[c].push (in[c]);
                        const auto wet = delay[c].read (wanted);
                        out[c] = in[c] + wet * level * g;
                    }
                    break;
                }
                case 41: case 42:
                {
                    const auto carrier = std::sin (2.0f * pi * advance (ring, ringHz));
                    if (p == 42)
                    {
                        const auto pos = 0.5f + 0.5f * std::sin (2.0f * pi * advance (lfo, rateHz));
                        if ((tick & 3) == 0)
                            for (auto& v : vowel) v.set (pos, sampleRate);
                        for (int c = 0; c < 2; ++c) insert (c, vowel[c].run (in[c] * carrier));
                    }
                    else
                    {
                        for (int c = 0; c < 2; ++c) insert (c, filter[c].run (in[c] * carrier).low);
                    }
                    break;
                }
                case 43:
                {
                    // SPECIAL ECHO (our reading): the repeats played backwards. Two
                    // heads read back through the last stretch, faded in turn.
                    const auto length = (float) reverseLength;
                    const auto countA = (float) reverseCounter;
                    const auto countB = std::fmod (countA + 0.5f * length, length);
                    const auto wA = std::pow (std::sin (pi * countA / length), 2.0f);
                    const auto wB = std::pow (std::sin (pi * countB / length), 2.0f);
                    for (int c = 0; c < 2; ++c)
                    {
                        delay[c].push (juce::jlimit (-4.0f, 4.0f, in[c] * g + feedback * echo[c]));
                        echo[c] = wA * delay[c].read (2.0f * countA + 1.0f) + wB * delay[c].read (2.0f * countB + 1.0f);
                        out[c] += echo[c] * level;
                    }
                    if (++reverseCounter >= reverseLength)
                        reverseCounter = 0;
                    break;
                }
                case 44: case 45:
                {
                    // 45 RING+DELAY: Y from all ring modulator (bottom) to all delay (top).
                    // 46 DELAY+TREMOLO: Y from all delay (bottom) to all tremolo (top).
                    moveDelay();
                    const auto carrier = p == 44 ? std::sin (2.0f * pi * advance (ring, ringHz)) : 0.0f;
                    const auto trem = p == 45 ? 1.0f - (0.5f - 0.5f * std::cos (2.0f * pi * advance (lfo, rateHz))) : 1.0f;
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto wet = echoLine (c, in[c] * g, false, false);
                        echo[c] = wet;
                        const auto delayed = in[c] + wet * level;
                        const auto other = in[c] + ((p == 44 ? in[c] * carrier : in[c] * trem) - in[c]) * g;
                        out[c] = p == 44 ? lerp (other, delayed, xfade) : lerp (delayed, other, xfade);
                    }
                    break;
                }
                case 47:
                {
                    // VIBRATE: SPEED and DEPTH rise together (up to 3 ms of sway, ours).
                    const auto sway = ms (0.2f + 3.0f * depth) * (0.5f + 0.5f * std::sin (2.0f * pi * advance (lfo, rateHz)));
                    for (int c = 0; c < 2; ++c)
                    {
                        delay[c].push (in[c]);
                        insert (c, delay[c].read (ms (0.5f) + sway));
                    }
                    break;
                }
                default: break;
            }
            outL = out[0];
            outR = out[1];
        }

        //======================================================================
        // Programs 51-60: REC records a phrase; the pad plays it.
        void sampler (int p, float g, bool active, float inL, float inR, float& outL, float& outR) noexcept
        {
            outL = inL;
            outR = inR;
            const auto capacity = (int) phrase[0].size() - 1;

            if (rec == Rec::waiting && juce::jmax (std::abs (inL), std::abs (inR)) > 0.01f)   // AUTO REC: about -40 dB (ours)
            {
                rec = Rec::recording;
                recordWrite = 0;
            }
            if (rec == Rec::recording)
            {
                phrase[0][(size_t) recordWrite] = inL;
                phrase[1][(size_t) recordWrite] = inR;
                if (++recordWrite >= capacity)
                {
                    phraseLength = recordWrite;
                    rec = Rec::recorded;
                }
                return;   // the sound passes unchanged while recording
            }
            if (rec != Rec::recorded || phraseLength < 2)
                return;

            const auto length = (double) phraseLength;
            const auto loopType = p == 50 || p == 53 || p == 56 || p == 58;
            const auto scratchType = p == 51 || p == 54 || p == 57 || p == 59;
            const auto directMix = p == 56 || p == 57;
            const auto panned = p >= 58;
            const auto playLevel = panned ? 1.0f : y;
            const auto shape = panned ? y : x;

            if (active && ! wasActive)
            {
                // Each touch starts the phrase again (from its end when it plays backwards).
                const auto rate = (shape - 0.5f) * 4.0f;
                playHead = scratchType && rate < 0.0f ? length - 1.0 : 0.0;
                grainAnchor = 0.0;
                grainPhase = 0.0f;
                scratchRate = rate;
            }

            const auto readAt = [&] (int c, double position)
            {
                position = std::fmod (position, length);
                if (position < 0.0)
                    position += length;
                const auto i = (int) position;
                const auto t = (float) (position - i);
                const auto& data = phrase[c];
                return data[(size_t) i] + (data[(size_t) ((i + 1) % phraseLength)] - data[(size_t) i]) * t;
            };

            float l = 0.0f, r = 0.0f;
            const auto fadeSamples = juce::jmax (1.0, (double) ms (3.0f));
            if (loopType)
            {
                // LOOP TIME: from a 30 ms stutter (left) to the whole phrase (right).
                const auto loop = juce::jlimit (2.0, length, length * (double) sweep (shape, 0.03f / juce::jmax (0.05f, (float) (length / sampleRate)), 1.0f));
                if (playHead >= loop)
                    playHead = std::fmod (playHead, loop);
                const auto edge = (float) juce::jmin (1.0, playHead / fadeSamples, (loop - playHead) / fadeSamples);
                l = readAt (0, playHead) * edge;
                r = readAt (1, playHead) * edge;
                playHead += 1.0;
            }
            else if (scratchType)
            {
                // FORWARD / REVERSE: right of centre forward, left backward, up to
                // twice the speed at the edges; the speed follows the finger with a
                // little lag, as a platter does.
                scratchRate += ((shape - 0.5f) * 4.0f - scratchRate) * 0.0015f;
                l = readAt (0, playHead);
                r = readAt (1, playHead);
                playHead += scratchRate;
                if (playHead < 0.0) playHead += length;
                if (playHead >= length) playHead -= length;
            }
            else
            {
                // TIME STRETCH: two overlapping 80 ms grains read at the
                // phrase's own speed while their start moves at 0.35x to 2.8x
                // (left to right; the pitch stays).
                const auto grain = (float) ms (80.0f);
                const auto stretch = std::pow (2.0f, (shape - 0.5f) * 3.0f);
                for (int head = 0; head < 2; ++head)
                {
                    const auto phase = std::fmod (grainPhase + 0.5f * (float) head, 1.0f);
                    const auto start = grainAnchor - (double) (phase * grain * stretch);
                    const auto w = std::sin (pi * phase);
                    l += readAt (0, start + phase * grain) * w * w;
                    r += readAt (1, start + phase * grain) * w * w;
                }
                grainPhase += 1.0f / grain;
                grainPhase -= std::floor (grainPhase);
                grainAnchor = std::fmod (grainAnchor + stretch, length);
                playHead = grainAnchor;
            }

            if (panned)
            {
                const auto mono = 0.5f * (l + r);
                l = mono * panLeft (x);
                r = mono * panRight (x);
            }
            const auto direct = directMix ? 1.0f : 1.0f - g;
            outL = inL * direct + l * playLevel * g;
            outR = inR * direct + r * playLevel * g;
        }
    };

    //==========================================================================
    Havok::Havok() : engine (std::make_unique<Engine>()) {}
    Havok::~Havok() = default;

    void Havok::prepare (double sampleRate, int maxBlockSize)
    {
        engine->prepare (sampleRate, maxBlockSize);
        bypass.prepare (sampleRate, maxBlockSize);
        reset();
    }

    void Havok::reset() noexcept
    {
        engine->clear();
        engine->erasePhrase();
        recState = (int) Rec::ready;
        recordedSeconds = 0.0f;
        playPosition = -1.0f;
        recPressesSeen = recPresses.load();
    }

    void Havok::process (juce::dsp::AudioBlock<float> block, const Settings& settings) noexcept
    {
        auto& e = *engine;
        const auto wanted = juce::jlimit (0, numPrograms - 1, settings.program);

        // REC presses since the last block.
        const auto presses = recPresses.load();
        const auto newPresses = presses - recPressesSeen;
        recPressesSeen = presses;

        bypass.process (block, settings.on, [&] (juce::dsp::AudioBlock<float> wet)
        {
            const auto numSamples = (int) wet.getNumSamples();
            auto* left = wet.getChannelPointer (0);
            auto* right = wet.getNumChannels() > 1 ? wet.getChannelPointer (1) : left;
            const auto active = settings.touch || settings.hold;
            const auto beatsPerSample = juce::jmax (1.0, settings.bpm) / 60.0 / e.sampleRate;

            for (int n = 0; n < newPresses && isSampler (e.current) && wanted == e.current; ++n)
            {
                // REC: start (or, for AUTO REC, wait for sound); again to stop; again to record anew.
                switch (e.rec)
                {
                    case Rec::ready:
                    case Rec::recorded:
                        e.phraseLength = e.recordWrite = 0;
                        e.rec = isAutoRec (e.current) ? Rec::waiting : Rec::recording;
                        break;
                    case Rec::waiting:   e.rec = Rec::ready; break;
                    case Rec::recording:
                        e.phraseLength = e.recordWrite;
                        e.rec = e.phraseLength > (int) e.ms (30.0f) ? Rec::recorded : Rec::ready;
                        break;
                }
            }

            for (int i = 0; i < numSamples; ++i)
            {
                // Switching program: dip, swap (clearing the old program's sound
                // and, as the manual says, any recorded phrase), come back.
                if (wanted != e.current)
                {
                    e.programGain -= e.programStep;
                    if (e.programGain <= 0.0f)
                    {
                        e.programGain = 0.0f;
                        e.current = wanted;
                        e.clear();
                        e.erasePhrase();
                    }
                }
                else if (e.programGain < 1.0f)
                {
                    e.programGain = juce::jmin (1.0f, e.programGain + e.programStep);
                }

                if (e.tick++ % tickSamples == 0)
                {
                    e.x += (settings.x - e.x) * e.smoothing;
                    e.y += (settings.y - e.y) * e.smoothing;
                    if (! isSampler (e.current) && ! isScratch (e.current))
                        e.control (e.current);
                }
                const auto target = active ? 1.0f : 0.0f;
                e.gate += (target - e.gate) * (target > e.gate ? e.gateUp : e.gateDown);

                const auto inL = left[i], inR = right[i];
                float outL = inL, outR = inR;
                if (isScratch (e.current))
                    e.scratch (e.current, settings, beatsPerSample, i, active, inL, inR, outL, outR);
                else if (isSampler (e.current))
                    e.sampler (e.current, e.gate, active, inL, inR, outL, outR);
                else
                    e.runEffect (e.current, e.gate, inL, inR, outL, outR);
                e.wasActive = active;

                left[i] = inL + (outL - inL) * e.programGain;
                if (right != left)
                    right[i] = inR + (outR - inR) * e.programGain;
            }
        });

        recState = (int) e.rec;
        recordedSeconds = (float) (e.rec == Rec::recording ? e.recordWrite : e.phraseLength) / (float) e.sampleRate;
        playPosition = isSampler (e.current) && e.rec == Rec::recorded && (settings.touch || settings.hold) && e.phraseLength > 0
                           ? (float) (e.playHead / (double) e.phraseLength) : -1.0f;
    }
}
