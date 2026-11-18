#include "Bloom.h"

#include <algorithm>
#include <cmath>

namespace effects
{
    namespace
    {
        constexpr double pi = 3.14159265358979;
        constexpr double seriesOhms = 22.0e3, acrossOhms = 470.0e3;   // R5, R6
        // The JFET (2SK30A-GR), ours from its typical figures: 4 mA with the
        // gate at the source, pinching off at -1.2 V. Linearised by R10 / R11,
        // it's a resistor of (|Vp| / 2 Idss) / (1 - Vgs / Vp): 150 ohm at rest
        // (about 43 dB down), open once the gate is past pinch-off.
        constexpr double pinchOff = 1.2, zeroBiasAmps = 4.0e-3;
        constexpr double onOhms = pinchOff / (2.0 * zeroBiasAmps);
        constexpr double gateTravel = 1.5 * pinchOff;   // ours: the gate ends a little past pinch-off
        constexpr double timingFarads = 68.0e-6;         // C17 as the factory drawing labels it
        constexpr double rail = 3.5;                     // the 741 on 9 V
        // The transistors conduct while the square swings more than about
        // 1.2 V peak to peak (their coupling holds its low end at -0.6 V).
        constexpr double conductSwing = 0.6;

        double audioTaper (double k) noexcept { return (std::pow (10.0, 2.0 * std::clamp (k, 0.0, 1.0)) - 1.0) / 99.0; }
    }

    double Bloom::gainFor (double o) noexcept
    {
        const auto vgs = -std::clamp (o, 0.0, 1.0) * gateTravel;
        const auto rds = vgs <= -pinchOff ? 1.0e12 : onOhms / (1.0 + vgs / pinchOff);
        const auto shunt = rds * acrossOhms / (rds + acrossOhms);
        return shunt / (seriesOhms + shunt);
    }

    double Bloom::swellSeconds (float attack) noexcept
    {
        return (1.0e3 + 20.0e3 * std::clamp ((double) attack / 10.0, 0.0, 1.0)) * timingFarads;
    }

    void Bloom::prepare (double sampleRate) noexcept
    {
        rate = sampleRate;
        couplingG = std::tan (pi * (1.0 / (2.0 * pi * 0.022e-6 * 220.0e3)) / rate);   // C8 into R18: 33 Hz
        // The op-amp: 1 + R21 / (R19 + 1 / sC10) = (1 + s C10 (R19 + R21)) / (1 + s C10 R19), bilinear.
        const auto k = 2.0 * rate, slow = 1.0e-6 * (390.0 + 1.0e6), fast = 1.0e-6 * 390.0;
        shelfB0 = (1.0 + slow * k) / (1.0 + fast * k);
        shelfB1 = (1.0 - slow * k) / (1.0 + fast * k);
        shelfA1 = (1.0 - fast * k) / (1.0 + fast * k);
        swingRelease = 1.0 - std::exp (-1.0 / (0.02 * rate));   // ours: the square has to stop for about a cycle of a low note
        rearm = 1.0 - std::exp (-1.0 / (rearmSeconds * rate));
        reset();
    }

    void Bloom::reset() noexcept
    {
        couplingIn = couplingOut = shelfIn = shelfOut = swing = opening = 0.0;
        open = 0.0f;
    }

    void Bloom::process (float* samples, int numSamples, float sens, float attack) noexcept
    {
        const auto sensGain = audioTaper (sens / 10.0);
        const auto swell = 1.0 - std::exp (-1.0 / (swellSeconds (attack) * rate));
        for (int i = 0; i < numSamples; ++i)
        {
            // The detector: SENS, the coupling, the op-amp clipping to a square.
            const auto in = (double) samples[i];
            const auto heard = in * sensGain;
            couplingOut = (heard - couplingIn) / (1.0 + couplingG) + couplingOut * (1.0 - couplingG) / (1.0 + couplingG);
            couplingIn = heard;
            shelfOut = shelfB0 * couplingOut + shelfB1 * shelfIn - shelfA1 * shelfOut;
            shelfIn = couplingOut;
            const auto square = opAmpClip (shelfOut, rail);
            swing = std::max (std::abs (square), swing + (0.0 - swing) * swingRelease);

            // The gate: swelling open while a note is here, closing again when it's gone.
            if (swing > conductSwing)
                opening += (1.0 - opening) * swell;
            else
                opening += (0.0 - opening) * rearm;

            samples[i] = (float) (in * gainFor (opening));
        }
        open = (float) opening;
    }
}
