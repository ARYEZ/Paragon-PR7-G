#pragma once

#include "Circuit.h"

namespace effects
{
    // Bloom: COMP / GATE's BLOOM type, modelled on the 1979 compact attack
    // delay (the "slow gear"), from its factory schematic (service notes, 1981)
    // and a published redrawing. A note starts silent and swells in, like a
    // violin bow or a volume pedal, by itself; the next swell needs the last
    // note to die away below SENS first.
    //
    //   buffer -> 22k -> [JFET to ground] -> buffer        (the volume control)
    //   SENS (100k audio) -> 0.022 uF into 220k -> op-amp, 1 + 1M / (390 + 1 uF)
    //       -> clipped to a square -> phase splitter -> two transistors that
    //       conduct while the square swings: "a note is here"
    //   while a note is here, the JFET's gate is pulled down through ATTACK
    //   (20k linear) and the JFET turns off: the guitar swells in; when the
    //   note falls below SENS, the gate is pulled back up and it mutes again.
    //
    // From the circuit: the volume control (22k into a 2SK30A with its two 1M
    // resistors, which keep it a clean resistor; 470k across it), the detector
    // (its coupling, gain and clipping), SENS and ATTACK's pots.
    // Our estimate (the owner's choice, PR7-G 0.90.0): the timing. The factory
    // drawing and the redrawing disagree on the parts that set it; the swell
    // runs through ATTACK (1k + 20k) into the factory drawing's 68 uF, about
    // 70 ms to 1.4 s, the range the pedal is known for; the mute comes back in
    // about 30 ms (47k into the redrawing's 0.5-0.68 uF at the gate). Also ours:
    // the JFET's typical figures (4 mA, pinching off at -1.2 V).
    class Bloom
    {
    public:
        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // SENS and ATTACK, 0 to 10 as their knobs turn.
        void process (float* samples, int numSamples, float sens, float attack) noexcept;

        // How open the volume control is now, 0 (muted) to 1, for the meter.
        float getOpen() const noexcept { return open; }

        // The volume control's gain for an opening 0 to 1 (for tests).
        static double gainFor (double opening) noexcept;
        // The swell's time constant for ATTACK 0 to 10, in seconds.
        static double swellSeconds (float attack) noexcept;
        static constexpr double rearmSeconds = 47.0e3 * 0.68e-6;

    private:
        double rate = 48000.0;
        double couplingG = 0.0, couplingIn = 0.0, couplingOut = 0.0;
        double shelfB0 = 1.0, shelfB1 = 0.0, shelfA1 = 0.0, shelfIn = 0.0, shelfOut = 0.0;
        double swing = 0.0, swingRelease = 0.0, rearm = 0.0;
        double opening = 0.0;
        float open = 0.0f;
    };
}
