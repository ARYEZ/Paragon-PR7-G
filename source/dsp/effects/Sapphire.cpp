#include "Sapphire.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        constexpr double pi = 3.14159265358979;
        constexpr double gainPotOhms = 250.0e3, tonePotOhms = 10.0e3, levelPotOhms = 100.0e3;
        constexpr double stageOneFeedback = 22.0e3, stageOneGround = 1.5e3;    // R29, R31 (with C22 0.15 uF)
        constexpr double stageTwoFeedback = 33.0e3, stageTwoGround = 2.2e3;    // R27, R34 (with C24 1 uF)
        constexpr double senseOhms = 10.0;                                       // ours: lets the network's current be read
        constexpr double diodeSaturation = 2.52e-9, diodeThermal = 1.752 * 25.85e-3;   // 1SS133 (1N4148 class)
        constexpr double swingUp = 3.6, swingDown = 3.8;                        // ours: the transistor op-amps' outputs
        constexpr double finalRail = 3.8;                                        // IC1b on 8 V

        double audioTaper (double knob) noexcept { return (std::pow (10.0, 2.0 * std::clamp (knob, 0.0, 1.0)) - 1.0) / 99.0; }
        double prewarp (double hz, double rate) noexcept { return std::tan (pi * std::min (hz, rate * 0.45) / rate); }
        double swing (double v) noexcept { return v >= 0.0 ? opAmpClip (v, swingUp) : opAmpClip (v, swingDown); }

        // One transistor op-amp stage: non-inverting, `feedbackOhms` + half of
        // GAIN (with `acrossFarads` over both) back to the inverting input,
        // `groundOhms` + `groundFarads` from there to ground. Returns the pot's index.
        int buildStage (Circuit& c, double feedbackOhms, double acrossFarads, double groundOhms, double groundFarads, double rate) noexcept
        {
            c = {};
            const auto in = c.node(), minus = c.node(), out = c.node(), mid = c.node(), leg = c.node();
            c.opAmp (in, minus, out);
            c.resistor (minus, mid, feedbackOhms);
            const auto pot = c.resistor (mid, out, gainPotOhms * 0.5);
            c.capacitor (out, minus, acrossFarads);
            c.resistor (minus, leg, groundOhms);
            c.capacitor (leg, 0, groundFarads);
            c.setInput (in);
            c.setOutput (out);
            c.prepare (rate);
            return pot;
        }

        // The network between the stages. `stageDriven`: its input is stage 1's
        // output and the diode node is ground; otherwise its input is the diode
        // node and stage 1's output is ground. Its output: the network's side
        // of the sense resistor.
        void buildNetwork (Circuit& c, bool stageDriven, double rate) noexcept
        {
            c = {};
            const auto source = stageDriven ? c.node() : 0;
            const auto diode = stageDriven ? 0 : c.node();
            const auto n1 = c.node(), n2 = c.node(), n3 = c.node(), bypass = c.node(), gate = c.node();
            c.resistor (source, n1, 100.0e3);      // R36
            c.capacitor (n1, n2, 0.1e-6);          // C34
            c.capacitor (n1, n3, 0.047e-6);        // C35
            c.resistor (n3, 0, 15.0e3);            // R51
            c.resistor (n2, n3, 1.0e6);            // R50
            c.capacitor (source, bypass, 220.0e-12);   // C26
            c.resistor (bypass, n2, 330.0e3);      // R37
            c.capacitor (n2, gate, 0.0022e-6);     // C27 into stage 2
            c.resistor (gate, 0, 1.0e6);           // R35
            c.resistor (n2, diode, senseOhms);
            c.setInput (stageDriven ? source : diode);
            c.setOutput (n2);
            c.prepare (rate);
        }
    }

    double Sapphire::stageOneGain (double knob) noexcept
    {
        return 1.0 + (stageOneFeedback + gainPotOhms * audioTaper (knob)) / stageOneGround;
    }

    double Sapphire::stageTwoGain (double knob) noexcept
    {
        return 1.0 + (stageTwoFeedback + gainPotOhms * audioTaper (knob)) / stageTwoGround;
    }

    void Sapphire::prepare (double oversampledRate, double baseRate) noexcept
    {
        rate = oversampledRate;
        inputG = prewarp (1.0 / (2.0 * pi * 0.1e-6 * 220.0e3), rate);
        gateG = prewarp (1.0 / (2.0 * pi * 0.0022e-6 * 1.0e6), rate);
        stageOnePot = buildStage (stageOne, stageOneFeedback, 47.0e-12, stageOneGround, 0.15e-6, rate);
        stageTwoPot = buildStage (stageTwo, stageTwoFeedback, 100.0e-12, stageTwoGround, 1.0e-6, rate);
        buildNetwork (fromStage, true, rate);
        buildNetwork (fromDiodes, false, rate);

        tail = {};
        {
            auto& c = tail;
            const auto in = c.node(), shelf = c.node(), top = c.node(), wiper = c.node(), bottom = c.node();
            const auto plus = c.node(), minus = c.node(), out = c.node(), mid = c.node(), base = c.node(), emitter = c.node();
            c.resistor (in, shelf, 5.6e3);         // R26
            c.capacitor (in, shelf, 0.0056e-6);    // C17
            c.capacitor (shelf, 0, 0.0056e-6);     // C19
            c.capacitor (shelf, top, 0.018e-6);    // C100
            toneUpper = c.resistor (top, wiper, tonePotOhms * 0.5);       // TONE: up is towards C100 (brighter)
            toneLower = c.resistor (wiper, bottom, tonePotOhms * 0.5);
            c.capacitor (bottom, 0, 0.018e-6);     // C101
            c.resistor (wiper, 0, levelPotOhms);   // LEVEL at full (PR7-G's LEVEL knob follows)
            c.capacitor (wiper, plus, 0.047e-6);   // C10
            c.resistor (plus, 0, 470.0e3);         // R13
            c.opAmp (plus, minus, out);            // IC1b
            c.resistor (minus, out, 6.8e3);        // R9
            c.capacitor (minus, out, 0.0022e-6);   // C8
            c.capacitor (minus, mid, 0.056e-6);    // C9
            c.resistor (mid, emitter, 1.2e3);      // R21
            c.capacitor (mid, base, 0.056e-6);     // C16
            c.resistor (base, 0, 470.0e3);         // R10
            c.opAmp (base, emitter, emitter);      // Q7, an emitter follower
            c.setInput (in);
            c.setOutput (out);
            c.prepare (baseRate);
        }

        gainKnob = toneKnob = -1.0;
        setGain (0.5);
        setTone (0.5);
        reset();
    }

    void Sapphire::reset() noexcept
    {
        stageOne.reset();
        stageTwo.reset();
        fromStage.reset();
        fromDiodes.reset();
        tail.reset();
        inputIn = inputOut = gateIn = gateOut = diodeVolts = diodeVoltsPrev = 0.0;
    }

    void Sapphire::setGain (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        if (std::abs (k - gainKnob) < 1.0e-4)
            return;
        gainKnob = k;
        stageOne.setResistor (stageOnePot, gainPotOhms * audioTaper (k));
        stageOne.update();
        stageTwo.setResistor (stageTwoPot, gainPotOhms * audioTaper (k));
        stageTwo.update();
    }

    void Sapphire::setTone (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        if (std::abs (k - toneKnob) < 1.0e-4)
            return;
        toneKnob = k;
        tail.setResistor (toneUpper, tonePotOhms * (1.0 - k));
        tail.setResistor (toneLower, tonePotOhms * k);
        tail.update();
    }

    double Sapphire::drive (double volts) noexcept
    {
        // C18 into R23 (7 Hz), then stage 1.
        inputOut = (volts - inputIn) / (1.0 + inputG) + inputOut * (1.0 - inputG) / (1.0 + inputG);
        inputIn = volts;
        const auto first = swing (stageOne.process (inputOut));

        // The diodes: the current from the network into them is what they pass.
        const auto known = fromStage.feedthrough() * first + fromStage.pending() + fromDiodes.pending();
        const auto through = fromDiodes.feedthrough();
        auto v = 2.0 * diodeVolts - diodeVoltsPrev;
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto x = std::clamp (v / (2.0 * diodeThermal), -40.0, 40.0);
            const auto ex = std::exp (x), exInverse = 1.0 / ex;
            const auto f = (known + (through - 1.0) * v) / senseOhms - diodeSaturation * (ex - exInverse);
            const auto d = (through - 1.0) / senseOhms - diodeSaturation / (2.0 * diodeThermal) * (ex + exInverse);
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
        fromStage.process (first);
        fromDiodes.process (v);
        diodeVoltsPrev = diodeVolts;
        diodeVolts = v;

        // C27 into R35 (72 Hz), then stage 2.
        gateOut = (v - gateIn) / (1.0 + gateG) + gateOut * (1.0 - gateG) / (1.0 + gateG);
        gateIn = v;
        return swing (stageTwo.process (gateOut));
    }

    double Sapphire::finish (double volts) noexcept
    {
        return opAmpClip (tail.process (volts), finalRail);
    }
}
