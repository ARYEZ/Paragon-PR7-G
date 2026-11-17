#include "Amber.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        // Electric Druid's redrawing of the 1977 original (parts numbered from 101).
        constexpr double inputFarads = 4.7e-9, inputBiasOhms = 100.0e3;        // C102, R104: 338 Hz
        constexpr double feedbackFixedOhms = 33.0e3, drivePotOhms = 1.0e6;      // R105, VR101 (1M log)
        constexpr double feedbackFarads = 100.0e-12;                            // C103
        constexpr double groundOhms = 4.7e3, groundFarads = 47.0e-9;            // R106, C104: 720 Hz
        constexpr double filterHz = 1.0 / (2.0 * 3.14159265358979 * 10.0e3 * 18.0e-9);   // R107, C105: 884 Hz
        constexpr double couplingHz = 1.0 / (2.0 * 3.14159265358979 * (4.7e3 + 10.0e3) * 1.0e-6);   // C108 into R110 + VR103: 10.8 Hz
        constexpr double opAmpRail = 3.5;   // a TL072 on 9 V swings about this far either way of its bias

        // 1N4148s (the original's diodes are the same silicon class).
        constexpr double diodeSaturation = 2.52e-9, diodeThermal = 1.752 * 25.85e-3;

        double audioTaper (double knob) noexcept { return (std::pow (10.0, 2.0 * std::clamp (knob, 0.0, 1.0)) - 1.0) / 99.0; }
        double prewarp (double hz, double rate) noexcept { return std::tan (3.14159265358979 * std::min (hz, rate * 0.45) / rate); }
    }

    double Amber::stageGain (double knob) noexcept
    {
        return 1.0 + (feedbackFixedOhms + drivePotOhms * audioTaper (knob)) / groundOhms;
    }

    void Amber::prepare (double oversampledRate, double baseRate) noexcept
    {
        period = 1.0 / oversampledRate;
        inputG = prewarp (1.0 / (2.0 * 3.14159265358979 * inputFarads * inputBiasOhms), oversampledRate);
        groundK = period / (2.0 * groundFarads);
        feedbackCapG = 2.0 * feedbackFarads / period;

        const auto lowPassK = prewarp (filterHz, baseRate);
        lowPassA = (1.0 - lowPassK) / (1.0 + lowPassK);
        const auto couplingK = prewarp (couplingHz, baseRate);
        couplingA = (1.0 - couplingK) / (1.0 + couplingK);
        reset();
    }

    void Amber::reset() noexcept
    {
        inputHistory = inputState = groundCapVolts = groundAmps = feedbackCapHistory = diodeVolts = diodeVoltsPrev = 0.0;
        lowPassState = lowPassIn = couplingState = couplingIn = 0.0;
    }

    void Amber::setDrive (double knob) noexcept
    {
        feedbackOhms = feedbackFixedOhms + drivePotOhms * audioTaper (knob);
    }

    double Amber::clip (double volts) noexcept
    {
        // The 338 Hz low cut into the op-amp's (+) input (a one-pole high-pass,
        // by the bilinear transform: y = (x - x1) / (1 + K) + y1 (1 - K) / (1 + K)).
        const auto in = (volts - inputHistory) / (1.0 + inputG) + inputState * (1.0 - inputG) / (1.0 + inputG);
        inputHistory = volts;
        inputState = in;

        // The (-) input follows the (+) input, so the leg to ground (4.7k and
        // 47 nF, trapezoidal) carries this current, and the feedback must too.
        const auto amps = (in - groundCapVolts - groundK * groundAmps) / (groundOhms + groundK);
        groundCapVolts += groundK * (amps + groundAmps);
        groundAmps = amps;

        // Across the feedback (output minus (-) input): 33k + DRIVE, 100 pF,
        // and the diodes: one 1N4148 conducting when the output is above, two
        // in series when it's below.
        const auto gR = 1.0 / feedbackOhms;
        auto v = 2.0 * diodeVolts - diodeVoltsPrev;
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto one = std::exp (std::clamp (v / diodeThermal, -40.0, 40.0));
            const auto two = std::exp (std::clamp (-v / (2.0 * diodeThermal), -40.0, 40.0));
            const auto f = v * gR + feedbackCapG * v - feedbackCapHistory
                         + diodeSaturation * (one - 1.0) - diodeSaturation * (two - 1.0) - amps;
            const auto d = gR + feedbackCapG + diodeSaturation / diodeThermal * one + diodeSaturation / (2.0 * diodeThermal) * two;
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
        const auto capAmps = feedbackCapG * v - feedbackCapHistory;
        feedbackCapHistory = feedbackCapG * v + capAmps;
        diodeVoltsPrev = diodeVolts;
        diodeVolts = v;
        return opAmpClip (in + v, opAmpRail);
    }

    double Amber::finish (double volts) noexcept
    {
        // The filter stage: inverting, unity gain, rolling off above 884 Hz.
        const auto lowPassK = (1.0 - lowPassA) / (1.0 + lowPassA);   // tan (pi fc / fs), back from the pole
        lowPassState = lowPassK / (1.0 + lowPassK) * (volts + lowPassIn) + lowPassA * lowPassState;
        lowPassIn = volts;
        const auto filtered = opAmpClip (-lowPassState, opAmpRail);

        // C108 into the LEVEL pot: blocks the DC an asymmetric clipper makes.
        couplingState = (filtered - couplingIn) * (1.0 + couplingA) * 0.5 + couplingA * couplingState;
        couplingIn = filtered;
        return couplingState;
    }
}
