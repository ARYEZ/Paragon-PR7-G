#include "CipherSpectral.h"

namespace effects
{
    namespace
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        constexpr int maxSize = 4096, maxBins = maxSize / 2 + 1;
        constexpr double historySeconds = 1.5;

        float wrapPi (float x) noexcept
        {
            x = std::fmod (x + juce::MathConstants<float>::pi, twoPi);
            if (x < 0.0f)
                x += twoPi;
            return x - juce::MathConstants<float>::pi;
        }

        float cutoffHz (float main01) noexcept { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, main01)); }
    }

    struct CipherSpectral::Channel
    {
        std::vector<float> input, outAcc, queue, fft, window;
        int write = 0, queueRead = 0;
        std::vector<float> lastPhase, sumPhase, mag, freq, inPhase, outMag, outFreq, held, heldFreq, mask, scratch, reverbMag;
        std::vector<float> historyMag, historyFreq;   // frames x bins, as many frames as fit 1.5 s
        std::vector<int> delays;
        int historyWrite = 0, lofiCountdown = 0, scatterCountdown = 0;
        double readFrame = 0.0;

        Channel()
        {
            input.assign (maxSize, 0.0f);
            outAcc.assign (maxSize * 2, 0.0f);
            queue.assign (maxSize, 0.0f);
            fft.assign (maxSize * 2, 0.0f);
            window.assign (maxSize, 0.0f);
            for (auto* v : { &lastPhase, &sumPhase, &mag, &freq, &inPhase, &outMag, &outFreq, &held, &heldFreq, &mask, &scratch, &reverbMag })
                v->assign (maxBins, 0.0f);
            delays.assign (maxBins, 0);
        }

        void clear()
        {
            for (auto* v : { &input, &outAcc, &queue, &fft, &lastPhase, &sumPhase, &held, &heldFreq, &reverbMag, &historyMag, &historyFreq })
                std::fill (v->begin(), v->end(), 0.0f);
            std::fill (mask.begin(), mask.end(), 1.0f);
            std::fill (delays.begin(), delays.end(), 0);
            write = queueRead = historyWrite = lofiCountdown = scatterCountdown = 0;
            readFrame = 0.0;
        }
    };

    CipherSpectral::CipherSpectral()
    {
        for (auto& c : channels)
            c = std::make_unique<Channel>();
        for (int i = 0; i < numSizes; ++i)
            ffts[(size_t) i] = std::make_unique<juce::dsp::FFT> (9 + i);
    }

    CipherSpectral::~CipherSpectral() = default;

    void CipherSpectral::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        // The history holds 1.5 s whatever the frame size: (rate / hop) frames of (size / 2 + 1) bins.
        const auto floats = (size_t) (historySeconds * sampleRate * 4.0 / 512.0 + 2.0) * (size_t) (512 / 2 + 1);
        for (auto& c : channels)
        {
            c->historyMag.assign (floats, 0.0f);
            c->historyFreq.assign (floats, 0.0f);
        }
        size = 0;
        setSize (2048);
    }

    void CipherSpectral::reset()
    {
        for (auto& c : channels)
            c->clear();
        frameCount = 0;
        lastGrid = -1;
        freeCounter = 0.0;
    }

    void CipherSpectral::setSize (int newSize)
    {
        if (newSize == size)
            return;
        size = newSize;
        hop = size / 4;
        const auto bins = size / 2 + 1;
        historyFrames = juce::jmax (2, (int) (channels[0]->historyMag.size() / (size_t) bins));
        for (auto& c : channels)
        {
            c->clear();
            for (int i = 0; i < size; ++i)
                c->window[(size_t) i] = 0.5f - 0.5f * std::cos (twoPi * (float) i / (float) size);   // Hann (periodic)
        }
        frameCount = 0;
    }

    //==========================================================================
    void CipherSpectral::process (float* const* chans, float* const* dryOut, int numChannels, int count, const Controls& controls)
    {
        setSize (controls.size);
        const double captureBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        const auto everyBeats = captureBeats[juce::jlimit (0, 5, juce::roundToInt (juce::jlimit (0.0f, 1.0f, controls.p1) * 5.0f))];
        for (int i = 0; i < count; ++i)
        {
            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = *channels[(size_t) c];
                dryOut[c][i] = ch.input[(size_t) ch.write];   // the sample from one frame ago
                ch.input[(size_t) ch.write] = chans[c][i];
                chans[c][i] = ch.queue[(size_t) ch.queueRead];
                ++ch.queueRead;
                ch.write = ch.write + 1 < size ? ch.write + 1 : 0;
            }
            if (++frameCount >= hop)
            {
                frameCount = 0;
                // STRETCH's TEMPO: a new part taken on this grid (the song's beats while it plays).
                auto capture = false;
                if (controls.effect == Effect::stretch)
                {
                    if (controls.songBeats >= 0.0)
                    {
                        const auto grid = (long long) std::floor ((controls.songBeats + (double) i / controls.samplesPerBeat) / everyBeats);
                        capture = grid != lastGrid;
                        lastGrid = grid;
                    }
                    else
                    {
                        freeCounter += hop;
                        capture = lastGrid < 0 || freeCounter >= everyBeats * controls.samplesPerBeat;
                        if (capture)
                        {
                            freeCounter = 0.0;
                            lastGrid = 0;
                        }
                    }
                }
                for (int c = 0; c < numChannels; ++c)
                {
                    processFrame (c, controls, capture);
                    channels[(size_t) c]->queueRead = 0;
                }
            }
        }
    }

    void CipherSpectral::processFrame (int c, const Controls& controls, bool capture)
    {
        auto& ch = *channels[(size_t) c];
        auto& fft = *ffts[(size_t) juce::jlimit (0, numSizes - 1, (int) std::log2 (size / 512))];
        const auto bins = size / 2 + 1;
        const auto binHz = (float) sampleRate / (float) size;
        const auto expected = twoPi * (float) hop / (float) size;
        auto* data = ch.fft.data();

        // The last frame of sound, oldest first, windowed, into frequencies.
        for (int i = 0; i < size; ++i)
            data[i] = ch.input[(size_t) ((ch.write + i) % size)] * ch.window[(size_t) i];
        std::fill (data + size, data + 2 * size, 0.0f);
        fft.performRealOnlyForwardTransform (data, true);

        // Each bin's level, phase and true frequency (in bins) from how far its phase moved.
        for (int k = 0; k < bins; ++k)
        {
            const auto re = data[2 * k], im = data[2 * k + 1];
            const auto phase = std::atan2 (im, re);
            ch.mag[(size_t) k] = std::sqrt (re * re + im * im);
            ch.inPhase[(size_t) k] = phase;
            const auto deviation = wrapPi (phase - ch.lastPhase[(size_t) k] - expected * (float) k);
            ch.lastPhase[(size_t) k] = phase;
            ch.freq[(size_t) k] = (float) k + deviation / expected;
        }

        const auto main = juce::jlimit (0.0f, 1.0f, controls.main[c]);
        const auto p1 = juce::jlimit (0.0f, 1.0f, controls.p1), p2 = juce::jlimit (0.0f, 1.0f, controls.p2);
        auto keepPhase = true;   // most effects keep each bin's own phase; the movers resynthesise it
        std::fill (ch.outMag.begin(), ch.outMag.begin() + bins, 0.0f);

        // Puts energy at a frequency (in bins): a note's neighbouring bins all
        // land in the one bin nearest its true frequency, so it comes back as one
        // clean sine. A windowed sine's bins add up to 4/3 of what that one bin
        // needs, hence the 0.75.
        const auto place = [&] (float to, float m)
        {
            const auto target = juce::roundToInt (to);
            if (target < 1 || target >= bins - 1)
                return;
            if (m * 0.75f > ch.outMag[(size_t) target] - 1.0e-12f)
                ch.outFreq[(size_t) target] = to;
            ch.outMag[(size_t) target] += m * 0.75f;
        };
        // Moves each bin's energy to a new frequency (in bins).
        const auto remap = [&] (auto&& mapping)
        {
            keepPhase = false;
            for (int k = 1; k < bins; ++k)
                place (mapping (ch.freq[(size_t) k]), ch.mag[(size_t) k]);
        };
        const auto history = [&] (int framesAgo, int k, bool wantFreq)
        {
            auto frame = ch.historyWrite - framesAgo;
            if (frame < 0)
                frame += historyFrames;
            const auto index = (size_t) frame * (size_t) bins + (size_t) k;
            return wantFreq ? ch.historyFreq[index] : ch.historyMag[index];
        };
        const auto writeHistory = [&] (int k, float m, float f)
        {
            const auto index = (size_t) ch.historyWrite * (size_t) bins + (size_t) k;
            ch.historyMag[index] = m;
            ch.historyFreq[index] = f;
        };

        switch (controls.effect)
        {
            case Effect::shift:
            {
                // SHIFT (the main knob): every frequency moved by +-2 kHz (finer near
                // the centre); SCALE (knob 1): spread out or squeezed, x0.25 to x4.
                const auto x = (main - 0.5f) * 2.0f;
                const auto translate = (x < 0.0f ? -1.0f : 1.0f) * x * x * 2000.0f / binHz;
                const auto scale = std::pow (2.0f, (p1 - 0.5f) * 4.0f);
                remap ([&] (float f) { return f * scale + translate; });
                break;
            }
            case Effect::rotate:
            {
                // The whole spectrum shifted and wrapped round (what falls off the top comes in at the bottom).
                const auto offset = (main - 0.5f) * (float) (bins - 2);
                remap ([&] (float f)
                {
                    auto to = f + offset;
                    const auto span = (float) (bins - 2);
                    to = std::fmod (to - 1.0f, span);
                    if (to < 0.0f)
                        to += span;
                    return to + 1.0f;
                });
                break;
            }
            case Effect::twist:
            {
                // Inside a band (BANDWIDTH 0.1 to 6 octaves around FREQ) the
                // spectrum is turned round by TWIST, wrapping at the band's edges.
                const auto centre = cutoffHz (main) / binHz;
                const auto width = 0.1f + p2 * 5.9f;
                const auto low = centre * std::pow (2.0f, -width * 0.5f), high = centre * std::pow (2.0f, width * 0.5f);
                remap ([&] (float f)
                {
                    if (f < low || f > high || f <= 0.0f)
                        return f;
                    auto place = std::log2 (f / low) / width + p1;
                    place -= std::floor (place);
                    return low * std::pow (2.0f, place * width);
                });
                break;
            }
            case Effect::mirror:
            {
                // Inside a band (BANDWIDTH) around FREQ the spectrum is flipped: low becomes high.
                const auto centre = cutoffHz (main) / binHz;
                const auto width = 0.1f + p1 * 5.9f;
                const auto low = centre * std::pow (2.0f, -width * 0.5f), high = centre * std::pow (2.0f, width * 0.5f);
                remap ([&] (float f) { return f >= low && f <= high && f > 0.0f ? centre * centre / f : f; });
                break;
            }
            case Effect::bandpass:
            {
                // A brick wall: only the band (BANDWIDTH 0.1 to 8 octaves around FREQUENCY) passes.
                const auto centre = cutoffHz (main) / binHz;
                const auto width = 0.1f + p1 * 7.9f;
                const auto low = centre * std::pow (2.0f, -width * 0.5f), high = centre * std::pow (2.0f, width * 0.5f);
                for (int k = 0; k < bins; ++k)
                {
                    const auto f = (float) k;
                    const auto edge = juce::jlimit (0.0f, 1.0f, juce::jmin (f - low + 1.0f, high - f + 1.0f));
                    ch.outMag[(size_t) k] = ch.mag[(size_t) k] * edge;
                }
                break;
            }
            case Effect::stretch:
            {
                // Plays the frames back slower (SPEED 5% to 100%) without changing
                // the pitch; TEMPO jumps to a new part; CHAOS scatters the phases.
                for (int k = 0; k < bins; ++k)
                    writeHistory (k, ch.mag[(size_t) k], ch.freq[(size_t) k]);
                if (capture)
                    ch.readFrame = 0.0;
                const auto behind = juce::jlimit (0, historyFrames - 1, (int) ch.readFrame);
                keepPhase = false;
                for (int k = 1; k < bins; ++k)
                {
                    place (history (behind, k, true), history (behind, k, false));
                    if (p2 > 0.0f)
                        ch.sumPhase[(size_t) k] += p2 * twoPi * (random.nextFloat() - 0.5f);
                }
                // The read place falls behind the newest frame by (1 - SPEED) each frame.
                ch.readFrame = juce::jmin ((double) historyFrames - 1.0, ch.readFrame + (1.0 - (0.05 + 0.95 * (double) main)));
                break;
            }
            case Effect::scatter:
            case Effect::cascade:
            {
                // Each frequency delayed by its own amount. SCATTER: random amounts up
                // to LENGTH (main, up to 1.5 s), new ones RATE times as often;
                // CASCADE: by how far it is from CENTRE (knob 2), so the sound
                // slides up (CENTRE low) or down (high). FEEDBACK (knob 1) repeats.
                const auto length = main * (float) (historyFrames - 1);
                const auto fb = p1 * 0.9f;
                if (controls.effect == Effect::scatter)
                {
                    if (--ch.scatterCountdown <= 0)
                    {
                        ch.scatterCountdown = 1 + juce::roundToInt ((1.0f - p2) * 60.0f);
                        for (int k = 0; k < bins; ++k)
                            ch.delays[(size_t) k] = juce::roundToInt (random.nextFloat() * length);
                    }
                }
                else
                {
                    const auto centre = cutoffHz (p2) / binHz;
                    for (int k = 0; k < bins; ++k)
                        ch.delays[(size_t) k] = k == 0 ? 0 : juce::roundToInt (length * juce::jmin (1.0f, std::abs (std::log2 ((float) k / centre)) / 6.0f));
                }
                keepPhase = false;
                for (int k = 0; k < bins; ++k)
                {
                    const auto d = juce::jlimit (0, historyFrames - 1, ch.delays[(size_t) k]);
                    const auto delayedMag = d == 0 ? 0.0f : history (d, k, false);
                    const auto delayedFreq = d == 0 ? ch.freq[(size_t) k] : history (d, k, true);
                    // What goes into the history: the new sound, plus the delayed sound again (FEEDBACK).
                    const auto in = ch.mag[(size_t) k];
                    writeHistory (k, in + fb * delayedMag, in >= fb * delayedMag ? ch.freq[(size_t) k] : delayedFreq);
                    if (k > 0)
                        place (d == 0 ? ch.freq[(size_t) k] : delayedFreq, d == 0 ? in : delayedMag);
                }
                break;
            }
            case Effect::smear:
            {
                // Each frequency held and fading over LENGTH (up to about 2 s);
                // CHAOS scatters the phases, a diffuse wash.
                const auto frames = 1.0f + main * main * (float) sampleRate * 2.0f / (float) hop;
                const auto fall = std::exp (-1.0f / frames);
                keepPhase = false;
                for (int k = 0; k < bins; ++k)
                {
                    auto& held = ch.held[(size_t) k];
                    if (ch.mag[(size_t) k] >= held * fall)
                    {
                        held = ch.mag[(size_t) k];
                        ch.heldFreq[(size_t) k] = ch.freq[(size_t) k];
                    }
                    else
                        held *= fall;
                    if (k > 0)
                        place (ch.heldFreq[(size_t) k], held);
                    if (p1 > 0.0f)
                        ch.sumPhase[(size_t) k] += p1 * twoPi * (random.nextFloat() - 0.5f);
                }
                break;
            }
            case Effect::reverb:
            {
                // Each frequency rings on and dies away over TIME (0.2 to 20 s), with
                // a random phase each frame (diffuse); TONE: the highs die faster
                // (left) or ring longer (right). The sound itself passes too.
                const auto seconds = 0.2f * std::pow (100.0f, main);
                for (int k = 0; k < bins; ++k)
                {
                    const auto place = (float) k / (float) bins;
                    const auto t = seconds * std::pow (2.0f, (p1 - 0.5f) * 4.0f * place);
                    const auto g = std::pow (10.0f, -3.0f * (float) hop / (t * (float) sampleRate));
                    ch.reverbMag[(size_t) k] = ch.reverbMag[(size_t) k] * g + ch.mag[(size_t) k] * 0.25f;
                    ch.outMag[(size_t) k] = ch.mag[(size_t) k];
                }
                break;
            }
            case Effect::compress:
            {
                // Each frequency pushed towards one target spectrum (AMOUNT): pink
                // (TONE left, falling 3 dB an octave) to white (right, flat), at the
                // frame's overall level: many bands of compression.
                auto power = 0.0f, targetPower = 0.0f;
                for (int k = 1; k < bins; ++k)
                {
                    power += ch.mag[(size_t) k] * ch.mag[(size_t) k];
                    const auto w = std::pow ((float) k * binHz / 1000.0f, -(1.0f - p1) * 0.5f);
                    ch.scratch[(size_t) k] = w;
                    targetPower += w * w;
                }
                const auto scale = targetPower > 0.0f ? std::sqrt (power / targetPower) : 0.0f;
                for (int k = 0; k < bins; ++k)
                {
                    const auto target = k == 0 ? ch.mag[0] : ch.scratch[(size_t) k] * scale;
                    ch.outMag[(size_t) k] = std::pow (ch.mag[(size_t) k] + 1.0e-9f, 1.0f - main) * std::pow (target + 1.0e-9f, main);
                }
                break;
            }
            case Effect::corrupt:
            {
                // AMOUNT left of centre takes out the quietest frequencies, right of
                // centre the loudest (at the centre nothing changes); DECAY: how
                // gently they come and go (low: glitchy).
                // Quiet and loud are judged against the frame's loudest frequency,
                // over a 60 dB range: further from the centre reaches further.
                const auto amount = (main - 0.5f) * 2.0f;
                const auto fraction = std::abs (amount) * 0.95f;
                auto loudest = 1.0e-9f;
                for (int k = 0; k < bins; ++k)
                    loudest = juce::jmax (loudest, ch.mag[(size_t) k]);
                const auto threshold = loudest * juce::Decibels::decibelsToGain (amount < 0.0f ? -60.0f * (1.0f - fraction) : -60.0f * fraction);
                const auto smooth = p1 * 0.95f;
                for (int k = 0; k < bins; ++k)
                {
                    const auto keep = std::abs (amount) < 0.01f ? 1.0f
                                    : amount < 0.0f ? (ch.mag[(size_t) k] >= threshold ? 1.0f : 0.0f)
                                                    : (ch.mag[(size_t) k] <= threshold ? 1.0f : 0.0f);
                    ch.mask[(size_t) k] = ch.mask[(size_t) k] * smooth + keep * (1.0f - smooth);
                    ch.outMag[(size_t) k] = ch.mag[(size_t) k] * ch.mask[(size_t) k];
                }
                break;
            }
            case Effect::lofi:
            {
                // Low-bitrate digital compression: FACTOR takes out most of the
                // frequencies, keeping the loudest (RANDOMISE picks more at random);
                // SLOW holds the choice for up to 40 frames.
                if (--ch.lofiCountdown <= 0)
                {
                    ch.lofiCountdown = 1 + juce::roundToInt (p2 * 40.0f);
                    auto loudest = 0.0f;
                    for (int k = 0; k < bins; ++k)
                        loudest = juce::jmax (loudest, ch.mag[(size_t) k]);
                    for (int k = 0; k < bins; ++k)
                        ch.scratch[(size_t) k] = ch.mag[(size_t) k] * (1.0f - p1) + random.nextFloat() * p1 * loudest;
                    std::vector<float>& order = ch.reverbMag;   // scratch space for the sort (not reverb here)
                    std::copy (ch.scratch.begin(), ch.scratch.begin() + bins, order.begin());
                    // FACTOR: half way keeps a quarter of the frequencies; all the way, just the loudest.
                    const auto kept = juce::jmax (1, (int) ((1.0f - main) * (1.0f - main) * (float) bins));
                    const auto nth = juce::jlimit (0, bins - 1, bins - kept);
                    std::nth_element (order.begin(), order.begin() + nth, order.begin() + bins);
                    const auto threshold = order[(size_t) nth];
                    for (int k = 0; k < bins; ++k)
                        ch.mask[(size_t) k] = ch.scratch[(size_t) k] >= threshold ? 1.0f : 0.0f;
                }
                for (int k = 0; k < bins; ++k)
                    ch.outMag[(size_t) k] = ch.mag[(size_t) k] * ch.mask[(size_t) k];
                break;
            }
            case Effect::spread:
            {
                // Each frequency's energy spread over its neighbours (AMOUNT: up to 40 bins each way).
                const auto radius = juce::roundToInt (main * 40.0f);
                auto running = 0.0f;
                for (int k = 0; k < bins; ++k)
                    ch.scratch[(size_t) k] = ch.mag[(size_t) k] * ch.mag[(size_t) k];
                for (int k = 0; k < juce::jmin (bins, radius); ++k)
                    running += ch.scratch[(size_t) k];
                for (int k = 0; k < bins; ++k)
                {
                    if (k + radius < bins)
                        running += ch.scratch[(size_t) (k + radius)];
                    if (k - radius - 1 >= 0)
                        running -= ch.scratch[(size_t) (k - radius - 1)];
                    ch.outMag[(size_t) k] = std::sqrt (juce::jmax (0.0f, running) / (float) (2 * radius + 1));
                }
                break;
            }
        }

        if (controls.effect == Effect::stretch || controls.effect == Effect::scatter || controls.effect == Effect::cascade)
            ch.historyWrite = (ch.historyWrite + 1) % historyFrames;

        // Back to sound: each bin at its level, with its own phase or one carried on
        // from its frequency (so moved and held frequencies stay smooth).
        for (int k = 0; k < bins; ++k)
        {
            float phase;
            if (keepPhase)
                phase = ch.inPhase[(size_t) k];
            else
            {
                ch.sumPhase[(size_t) k] = wrapPi (ch.sumPhase[(size_t) k] + expected * ch.outFreq[(size_t) k]);
                phase = ch.sumPhase[(size_t) k];
            }
            auto re = ch.outMag[(size_t) k] * std::cos (phase), im = ch.outMag[(size_t) k] * std::sin (phase);
            if (controls.effect == Effect::reverb)
            {
                // The ringing, at a random phase, on top of the sound.
                const auto r = random.nextFloat() * twoPi;
                re += ch.reverbMag[(size_t) k] * std::cos (r);
                im += ch.reverbMag[(size_t) k] * std::sin (r);
            }
            data[2 * k] = re;
            data[2 * k + 1] = im;
        }
        fft.performRealOnlyInverseTransform (data);

        // Windowed again and overlapped with the frames before. Four Hann-squared
        // frames add up to 1.5; JUCE's inverse transform is scaled by 1/size.
        const auto gain = 1.0f / 1.5f;
        for (int i = 0; i < size; ++i)
            ch.outAcc[(size_t) i] += data[i] * ch.window[(size_t) i] * gain;
        // The first hop of the accumulator is finished: it plays over the next hop.
        std::copy (ch.outAcc.begin(), ch.outAcc.begin() + hop, ch.queue.begin());
        std::copy (ch.outAcc.begin() + hop, ch.outAcc.begin() + size, ch.outAcc.begin());
        std::fill (ch.outAcc.begin() + (size - hop), ch.outAcc.begin() + size, 0.0f);
    }
}
