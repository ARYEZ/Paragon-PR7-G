#include "Ember.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace effects
{
    namespace
    {
        // ElectroSmash's DS1-A bill of materials.
        constexpr double c3 = 47.0e-9, r7 = 470.0e3, c4 = 250.0e-12;   // into Q2, its collector-base feedback
        // Q2's own gain with its feedback open (10k collector, 22 ohm emitter
        // and its emitter resistance), set so the stage's treble gain is
        // ElectroSmash's 56: A C3 / (C3 + (1 + A) C4) = 56 gives A = 80.
        constexpr double boosterOpenGain = 80.0;
        constexpr double boosterUp = 3.2, boosterDown = 5.6;   // its collector sits at about 5.8 V on a 9 V supply
        constexpr double c5 = 68.0e-9, r10 = 100.0e3;            // into the op-amp: 23 Hz
        constexpr double distPotOhms = 100.0e3, r13 = 4.7e3, c8 = 0.47e-6, c7 = 100.0e-12;
        constexpr double opAmpRail = 3.5;                        // about how far the op-amp swings from its 4.5 V bias
        constexpr double r14 = 2.2e3, c9 = 0.47e-6;              // into the diodes
        constexpr double senseOhms = 10.0;                       // ours: lets the TONE stack's load be read as a current
        constexpr double tonePotOhms = 20.0e3;
        constexpr double diodeSaturation = 2.52e-9, diodeThermal = 1.752 * 25.85e-3;   // 1N4148

        double prewarp (double hz, double rate) noexcept { return std::tan (3.14159265358979 * std::min (hz, rate * 0.45) / rate); }
        double swing (double v, double up, double down) noexcept { return v >= 0.0 ? opAmpClip (v, up) : opAmpClip (v, down); }
    }

    double Ember::opAmpGain (double knob) noexcept { return 1.0 + distPotOhms * std::clamp (knob, 0.0, 1.0) / r13; }
    double Ember::boosterTrebleGain() noexcept { return boosterOpenGain * c3 / (c3 + (1.0 + boosterOpenGain) * c4); }

    void Ember::buildTone (Circuit& c, bool audioOut) noexcept
    {
        c = {};
        const auto k = c.node(), l = c.node(), b = c.node(), a = c.node(), lowEnd = c.node(), w = c.node(), m = c.node(), o = c.node();
        c.resistor (k, l, senseOhms);
        c.capacitor (l, 0, 0.01e-6);        // C10, across the diodes
        c.resistor (l, b, 6.8e3);           // R16 into C12: the low-pass (234 Hz)
        c.capacitor (b, 0, 0.1e-6);
        c.capacitor (l, a, 0.022e-6);       // C11 and R15 into R17: the high-pass (1.06 kHz)
        c.resistor (a, lowEnd, 2.2e3);
        c.resistor (lowEnd, 0, 6.8e3);
        const auto upper = c.resistor (b, w, tonePotOhms * 0.5);   // TONE: up is towards the high-pass end
        const auto lower = c.resistor (w, lowEnd, tonePotOhms * 0.5);
        c.resistor (w, 0, 100.0e3);         // LEVEL (VR2) at full; PR7-G's LEVEL knob follows
        c.resistor (w, m, 10.0e3);          // R18, C13 and R19 // R20 into the output buffer
        c.capacitor (m, o, 47.0e-9);
        c.resistor (o, 0, 500.0e3);
        c.setInput (k);
        c.setOutput (audioOut ? o : l);
        c.prepare (rate);
        (audioOut ? toneOutUpper : toneLoadUpper) = upper;
        (audioOut ? toneOutLower : toneLoadLower) = lower;
    }

    void Ember::prepare (double oversampledRate) noexcept
    {
        rate = oversampledRate;

        // The booster: H(s) = -A R7 C3 s / ((1 + A) + s R7 (C3 + (1 + A) C4)), by the bilinear transform.
        const auto k2 = 2.0 * rate;
        const auto b1 = -boosterOpenGain * r7 * c3, a0 = 1.0 + boosterOpenGain, a1 = r7 * (c3 + (1.0 + boosterOpenGain) * c4);
        boostB0 = b1 * k2 / (a0 + a1 * k2);
        boostB1 = -boostB0;
        boostA1 = (a0 - a1 * k2) / (a0 + a1 * k2);

        couplingG = prewarp (1.0 / (2.0 * 3.14159265358979 * c5 * r10), rate);

        opAmpStage = {};
        {
            auto& c = opAmpStage;
            const auto in = c.node(), out = c.node(), x = c.node(), g = c.node(), h = c.node();
            c.opAmp (in, x, out);
            distLower = c.resistor (out, x, distPotOhms * 0.5);   // the pot between the output and the wiper
            c.capacitor (out, x, c7);
            distUpper = c.resistor (x, g, distPotOhms * 0.5);     // and between the wiper and R13
            c.resistor (g, h, r13);
            c.capacitor (h, 0, c8);
            c.setInput (in);
            c.setOutput (out);
            c.prepare (rate);
        }
        gCoupling = 2.0 * c9 * rate;
        buildTone (toneLoad, false);
        buildTone (toneOut, true);
        distKnob = toneKnob = -1.0;
        setDistortion (0.5);
        setTone (0.5);
        reset();
    }

    void Ember::reset() noexcept
    {
        opAmpStage.reset();
        toneLoad.reset();
        toneOut.reset();
        boostIn = boostOut = couplingIn = couplingOut = 0.0;
        jCoupling = diodeNode = diodeNodePrev = 0.0;
    }

    void Ember::setDistortion (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        if (std::abs (k - distKnob) < 1.0e-4)
            return;
        distKnob = k;
        opAmpStage.setResistor (distLower, distPotOhms * k);           // clockwise: more pot in the feedback
        opAmpStage.setResistor (distUpper, distPotOhms * (1.0 - k));
        opAmpStage.update();
    }

    void Ember::setTone (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        if (std::abs (k - toneKnob) < 1.0e-4)
            return;
        toneKnob = k;
        for (auto [c, upper, lower] : { std::tuple { &toneLoad, toneLoadUpper, toneLoadLower }, std::tuple { &toneOut, toneOutUpper, toneOutLower } })
        {
            c->setResistor (upper, tonePotOhms * k);
            c->setResistor (lower, tonePotOhms * (1.0 - k));
            c->update();
        }
    }

    double Ember::process (double volts) noexcept
    {
        // The booster (inverting), then its collector's swing: more room
        // downwards than up, so it clips a little unevenly, as ElectroSmash shows.
        boostOut = boostB0 * volts + boostB1 * boostIn - boostA1 * boostOut;
        boostIn = volts;
        const auto boosted = swing (boostOut, boosterUp, boosterDown);

        // C5 into R10 (23 Hz), the op-amp's unity buffer, then the gain stage.
        couplingOut = (boosted - couplingIn) / (1.0 + couplingG) + couplingOut * (1.0 - couplingG) / (1.0 + couplingG);
        couplingIn = boosted;
        const auto gained = opAmpClip (opAmpStage.process (couplingOut), opAmpRail);

        // R14 and C9 into the diodes, loaded by C10 and the TONE stack.
        const auto sourceR = r14 + 1.0 / gCoupling;
        const auto source = gained - jCoupling / gCoupling;
        const auto through = toneLoad.feedthrough(), pending = toneLoad.pending();
        auto v = 2.0 * diodeNode - diodeNodePrev;
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto x = std::clamp (v / diodeThermal, -40.0, 40.0);
            const auto ex = std::exp (x), exInverse = 1.0 / ex;
            const auto load = (v * (1.0 - through) - pending) / senseOhms;
            const auto f = (source - v) / sourceR - diodeSaturation * (ex - exInverse) - load;
            const auto d = -1.0 / sourceR - diodeSaturation / diodeThermal * (ex + exInverse) - (1.0 - through) / senseOhms;
            const auto step = std::clamp (-f / d, -0.1, 0.1);
            v += step;
            if (std::abs (step) < 1.0e-7)
                break;
        }
        if (! std::isfinite (v))
        {
            reset();
            return 0.0;
        }
        jCoupling = 2.0 * (source - v) / sourceR + jCoupling;
        diodeNodePrev = diodeNode;
        diodeNode = v;
        toneLoad.process (v);
        return toneOut.process (v);
    }
}
