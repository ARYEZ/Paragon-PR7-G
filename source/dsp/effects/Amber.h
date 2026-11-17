#pragma once

#include "Circuit.h"

namespace effects
{
    // Amber: OD / DISTORTION's first overdrive, modelled on the 1977 compact
    // overdrive that started the category (the yellow one), from Electric
    // Druid's redrawing of the original schematic ("Designing a classic
    // OD-1/TS-808/SD-1 Overdrive", 2023):
    //
    //   buffer -> 4.7 nF into 100k (a 338 Hz low cut) -> drive stage -> 884 Hz filter -> LEVEL -> buffer
    //
    // The drive stage is a non-inverting op-amp (TL072). Its feedback is
    // 33k plus the 1M log DRIVE pot, with 100 pF across it and the clipping
    // diodes across that: two 1N4148s in series one way and one the other,
    // so one half of the wave clips at about 0.6 V and the other at about
    // 1.2 V. That asymmetry is the pedal's signature (even harmonics, a
    // "tube-like" push). Its leg to ground is 4.7k with 47 nF, so only what's
    // above about 720 Hz gets the full gain of 8 (DRIVE at 0) to 220 (at 10).
    // There is no tone control: a fixed 884 Hz low-pass (10k and 18 nF round
    // an inverting unity-gain stage) follows.
    //
    // The diode node is solved sample by sample (Newton's method, starting
    // from the last two samples' trend, as PR7-G 0.82.1's drive models).
    class Amber
    {
    public:
        void prepare (double oversampledRate, double baseRate) noexcept;
        void reset() noexcept;

        // DRIVE, 0 to 1 as the pedal's knob turns (a 1M audio-taper pot). Call once per block.
        void setDrive (double knob) noexcept;

        // The low cut and the drive stage, at the oversampled rate: guitar volts in, volts out.
        double clip (double volts) noexcept;
        // The fixed filter and the output coupling, at the base rate (inverting, as the pedal).
        double finish (double volts) noexcept;

        // The drive stage's gain above its 720 Hz corner, for DRIVE 0 to 1 (for tests).
        static double stageGain (double knob) noexcept;

    private:
        double period = 1.0 / 384000.0;
        double feedbackOhms = 33.0e3;

        // The 338 Hz low cut (4.7 nF into 100k), trapezoidal.
        double inputG = 0.0, inputHistory = 0.0, inputState = 0.0;
        // The 4.7k + 47 nF leg to ground: the capacitor's voltage and the last current.
        double groundCapVolts = 0.0, groundAmps = 0.0, groundK = 0.0;
        // The 100 pF across the feedback, and the diode node (output minus (-) input).
        double feedbackCapG = 0.0, feedbackCapHistory = 0.0;
        double diodeVolts = 0.0, diodeVoltsPrev = 0.0;

        // The 884 Hz low-pass and the output coupling (C108 into the LEVEL pot), at the base rate.
        double lowPassA = 0.0, lowPassState = 0.0, lowPassIn = 0.0;
        double couplingA = 0.0, couplingState = 0.0, couplingIn = 0.0;
    };
}
