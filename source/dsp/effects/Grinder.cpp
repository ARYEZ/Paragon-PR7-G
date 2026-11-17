#include "Grinder.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        // The factory schematic's parts.
        constexpr double stageInOhms = 22.0e3, stageInFarads = 0.047e-6;          // R19/C6, R22/C11
        constexpr double stageFeedbackOhms = 470.0e3, stageFeedbackFarads = 100.0e-12;   // R16/C10, R29/C14
        constexpr double q6Collector = 10.0e3, q6Emitter = 22.0, q6ShuntOhms = 150.0;   // R17, R18, R49
        constexpr double q7Collector = 10.0e3, q7Emitter = 120.0;                       // R27, R28
        constexpr double emitterResistance = 52.0;   // ours: 26 mV over about 0.5 mA of collector current
        constexpr double distPotOhms = 250.0e3;
        constexpr double q6Up = 4.8, q6Down = 3.5, q7Up = 3.5, q7Down = 4.6;   // ours, from the stages' bias points on ~8.5 V
        constexpr double groundFixedOhms = 47.0e3, groundFarads = 0.047e-6;      // R42, C22
        constexpr double feedbackOhms = 220.0e3, feedbackFarads = 100.0e-12;     // R20, C9
        constexpr double opAmpRail = 3.5;
        constexpr double couplingFarads = 1.0e-6, seriesOhms = 10.0e3 + 10.0e3;   // C12; R23 and R30
        constexpr double clipFarads = 0.001e-6, clipLoadOhms = 68.0e3;            // C16; R24 (through C15)
        constexpr double siSaturation = 2.52e-9, siThermal = 1.752 * 25.85e-3;    // D3-D5, D8, D9: silicon
        constexpr double geSaturation = 2.0e-7, geThermal = 1.3 * 25.85e-3;       // D6, D7: germanium (ours: a typical small-signal part)
        constexpr double eqRail = 3.8;

        double swing (double v, double up, double down) noexcept { return v >= 0.0 ? opAmpClip (v, up) : opAmpClip (v, down); }
    }

    double Grinder::opAmpGain (double knob) noexcept
    {
        return 1.0 + feedbackOhms / (groundFixedOhms + distPotOhms * (1.0 - std::clamp (knob, 0.0, 1.0)));
    }

    void Grinder::Biquad::setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double sampleRate) noexcept
    {
        // The bilinear transform of (nb0 + nb1 s + nb2 s^2) / (na0 + na1 s + na2 s^2).
        const auto k = 2.0 * sampleRate, k2 = k * k;
        const auto a0 = na0 + na1 * k + na2 * k2;
        b0 = (nb0 + nb1 * k + nb2 * k2) / a0;
        b1 = (2.0 * nb0 - 2.0 * nb2 * k2) / a0;
        b2 = (nb0 - nb1 * k + nb2 * k2) / a0;
        a1 = (2.0 * na0 - 2.0 * na2 * k2) / a0;
        a2 = (na0 - na1 * k + na2 * k2) / a0;
    }

    double Grinder::Biquad::process (double x) noexcept
    {
        const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }

    void Grinder::setShuntStage (Biquad& stage, double openGain, double sampleRate) noexcept
    {
        // An inverting stage of open-loop gain A with Zin = R + 1/(sC) in and
        // Zf = Rf // Cf back: -A Zf / (Zf + (1 + A) Zin), multiplied out.
        const auto r = stageInOhms, c = stageInFarads, rf = stageFeedbackOhms, cf = stageFeedbackFarads, a = openGain;
        stage.setAnalog (0.0, -a * rf * c, 0.0,
                         1.0 + a, (1.0 + a) * (r * c + rf * cf) + rf * c, (1.0 + a) * r * c * rf * cf, sampleRate);
    }

    void Grinder::prepare (double oversampledRate, double baseRate) noexcept
    {
        rate = oversampledRate;
        const auto period = 1.0 / rate;
        groundK = period / (2.0 * groundFarads);
        feedbackCapG = 2.0 * feedbackFarads / period;
        gCoupling = 2.0 * couplingFarads / period;
        clipCapG = 2.0 * clipFarads / period;
        setShuntStage (q7, q7Collector / (emitterResistance + q7Emitter), rate);

        eq = {};
        {
            auto& c = eq;
            const auto in = c.node(), plus = c.node(), minus = c.node(), out = c.node(), lowWiper = c.node(), highWiper = c.node();
            c.resistor (in, plus, 3.3e3);                 // R54
            c.resistor (out, minus, 3.3e3);               // R52
            c.capacitor (out, minus, 470.0e-12);          // C33
            c.opAmp (plus, minus, out);                   // IC3a
            lowTop = c.resistor (minus, lowWiper, 5.0e3);       // COLOR MIX LOW: (-) end ...
            lowBottom = c.resistor (lowWiper, plus, 5.0e3);     // ... (+) end
            highTop = c.resistor (minus, highWiper, 5.0e3);     // COLOR MIX HIGH
            highBottom = c.resistor (highWiper, plus, 5.0e3);
            // Each gyrator: the series capacitor into Y; Y to a follower's output
            // through 330 ohm, and to its (+) through C, which R holds at ground.
            const auto gyrator = [&c] (int wiper, double seriesFarads, double gyratorFarads, double groundOhms)
            {
                const auto y = c.node(), p = c.node(), o = c.node();
                c.capacitor (wiper, y, seriesFarads);
                c.capacitor (y, p, gyratorFarads);
                c.resistor (p, 0, groundOhms);
                c.opAmp (p, o, o);
                c.resistor (y, o, 330.0);
            };
            gyrator (lowWiper, 1.5e-6, 0.068e-6, 100.0e3);    // C35; C30, R51, R47, IC3b: ~87 Hz
            gyrator (highWiper, 0.15e-6, 0.0068e-6, 82.0e3);  // C28; C27, R43, R41, IC2a: ~960 Hz
            gyrator (highWiper, 0.1e-6, 0.0047e-6, 100.0e3);  // C29; C26, R44, R45, IC2b: ~1.28 kHz
            c.setInput (in);
            c.setOutput (out);
            c.prepare (baseRate);
        }
        distKnob = lowKnob = highKnob = -1.0;
        setDistortion (0.5);
        setColour (0.5, 0.5);
        reset();
    }

    void Grinder::reset() noexcept
    {
        q6.reset();
        q7.reset();
        eq.reset();
        groundCapVolts = groundAmps = feedbackCapHistory = diodeVolts = diodeVoltsPrev = 0.0;
        jCoupling = seriesAmps = seriesAmpsPrev = clipVolts = clipVoltsPrev = clipCapHistory = 0.0;
    }

    void Grinder::setDistortion (double knob) noexcept
    {
        const auto k = std::clamp (knob, 0.0, 1.0);
        if (std::abs (k - distKnob) < 1.0e-4)
            return;
        distKnob = k;
        // Up (wiper towards the op-amp's end): Q6's collector sees the whole
        // pot, the op-amp's leg sees none of it. Down: the reverse.
        const auto shunt = q6ShuntOhms + distPotOhms * k;
        const auto load = q6Collector * shunt / (q6Collector + shunt);
        setShuntStage (q6, load / (emitterResistance + q6Emitter), rate);
        groundOhms = groundFixedOhms + distPotOhms * (1.0 - k);
    }

    void Grinder::setColour (double low, double high) noexcept
    {
        const auto l = std::clamp (low, 0.0, 1.0), h = std::clamp (high, 0.0, 1.0);
        if (std::abs (l - lowKnob) < 1.0e-4 && std::abs (h - highKnob) < 1.0e-4)
            return;
        lowKnob = l;
        highKnob = h;
        // Up: the wiper towards the (-) end, which boosts its band.
        eq.setResistor (lowTop, 10.0e3 * (1.0 - l));
        eq.setResistor (lowBottom, 10.0e3 * l);
        eq.setResistor (highTop, 10.0e3 * (1.0 - h));
        eq.setResistor (highBottom, 10.0e3 * h);
        eq.update();
    }

    double Grinder::clip (double volts) noexcept
    {
        // Q6 then Q7, each inverting with its swing limits.
        const auto a = swing (q6.process (volts), q6Up, q6Down);
        const auto b = swing (q7.process (a), q7Up, q7Down);

        // Op-amp 1b: (-) follows (+), so the leg to ground carries this current, and the feedback must too.
        const auto amps = (b - groundCapVolts - groundK * groundAmps) / (groundOhms + groundK);
        groundCapVolts += groundK * (amps + groundAmps);
        groundAmps = amps;
        // Across the feedback (output minus (-)): 220k, 100 pF, D3 when the
        // output is below, D4 and D5 in series when it's above.
        auto v = 2.0 * diodeVolts - diodeVoltsPrev;
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto two = std::exp (std::clamp (v / (2.0 * siThermal), -40.0, 40.0));
            const auto one = std::exp (std::clamp (-v / siThermal, -40.0, 40.0));
            const auto f = v / feedbackOhms + feedbackCapG * v - feedbackCapHistory
                         + siSaturation * (two - 1.0) - siSaturation * (one - 1.0) - amps;
            const auto d = 1.0 / feedbackOhms + feedbackCapG + siSaturation / (2.0 * siThermal) * two + siSaturation / siThermal * one;
            const auto step = std::clamp (-f / d, -0.1, 0.1);
            v += step;
            if (std::abs (step) < 1.0e-7)
                break;
        }
        const auto capAmps = feedbackCapG * v - feedbackCapHistory;
        feedbackCapHistory = feedbackCapG * v + capAmps;
        diodeVoltsPrev = diodeVolts;
        diodeVolts = v;
        const auto opAmpOut = opAmpClip (b + v, opAmpRail);

        // C12, R23, the germanium pair in series, R30, into the silicon pair
        // (with C16 and R24 across it). Two unknowns: the series current and the node.
        const auto sourceR = seriesOhms + 1.0 / gCoupling;
        const auto source = opAmpOut - jCoupling / gCoupling;
        auto i = 2.0 * seriesAmps - seriesAmpsPrev, k = 2.0 * clipVolts - clipVoltsPrev;
        for (int iteration = 0; iteration < 30; ++iteration)
        {
            const auto u = i / (2.0 * geSaturation);
            const auto root = std::sqrt (1.0 + u * u);
            const auto f1 = source - k - i * sourceR - geThermal * std::log (u + root);   // asinh: the germanium pair's voltage
            const auto ex = std::exp (std::clamp (k / siThermal, -40.0, 40.0)), exInverse = 1.0 / ex;
            const auto f2 = i - siSaturation * (ex - exInverse) - (clipCapG * k - clipCapHistory) - k / clipLoadOhms;
            const auto j11 = -sourceR - geThermal / (2.0 * geSaturation * root), j12 = -1.0;
            const auto j21 = 1.0, j22 = -(siSaturation / siThermal * (ex + exInverse) + clipCapG + 1.0 / clipLoadOhms);
            const auto det = j11 * j22 - j12 * j21;
            const auto di = (-f1 * j22 + f2 * j12) / det;
            auto dk = (-f2 * j11 + f1 * j21) / det;
            dk = std::clamp (dk, -0.1, 0.1);
            i += di;
            k += dk;
            if (std::abs (dk) < 1.0e-7 && std::abs (di) < 1.0e-11)
                break;
        }
        if (! std::isfinite (i) || ! std::isfinite (k) || ! std::isfinite (v))
        {
            reset();
            return 0.0;
        }
        jCoupling = 2.0 * i + jCoupling;
        const auto clipCapAmps = clipCapG * k - clipCapHistory;
        clipCapHistory = clipCapG * k + clipCapAmps;
        seriesAmpsPrev = seriesAmps;
        seriesAmps = i;
        clipVoltsPrev = clipVolts;
        clipVolts = k;
        return k;   // into IC1a, a follower
    }

    double Grinder::equalise (double volts) noexcept
    {
        return opAmpClip (eq.process (volts), eqRail);
    }
}
