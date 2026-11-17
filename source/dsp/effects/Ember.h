#pragma once

#include "Circuit.h"

namespace effects
{
    // Ember: OD / DISTORTION's orange distortion, modelled on the 1978 compact
    // distortion (the best-selling compact pedal), from ElectroSmash's analysis
    // of its DS1-A circuit (part values from its bill of materials and schematic):
    //
    //   buffer -> transistor booster -> buffer -> op-amp gain stage (DIST)
    //          -> 2.2k + 0.47 uF into two diodes to ground (hard clipping)
    //          -> the passive TONE stack (a mid scoop around 500 Hz) -> LEVEL -> buffer
    //
    // The booster (Q2) is driven straight through a 47 nF capacitor with 470k
    // and 250 pF from collector to base: a shunt-feedback stage whose gain
    // ElectroSmash puts at 56 (35 dB) in the treble, falling in the bass. It
    // already clips softly and a little unevenly against the 9 V supply. The
    // op-amp stage is non-inverting, its gain 1 + (DIST pot) / (4.7k + 0.47 uF),
    // up to 22, with 100 pF across the pot. The diodes clip hard at about
    // 0.6 V, with 0.01 uF across them and the TONE stack loading them.
    //
    // The diode node is solved sample by sample (Newton's method, as PR7-G
    // 0.82.1's drive models); the TONE stack is solved from its parts as a
    // Circuit, at the oversampled rate, as it loads the diodes.
    class Ember
    {
    public:
        void prepare (double oversampledRate) noexcept;
        void reset() noexcept;

        // DIST and TONE, 0 to 1 as the pedal's knobs turn (both linear pots). Call once per block.
        void setDistortion (double knob) noexcept;
        void setTone (double knob) noexcept;

        // The whole circuit at the oversampled rate: guitar volts in, volts out (after the TONE stack).
        double process (double volts) noexcept;

        // The op-amp stage's gain above its 72 Hz corner, for DIST 0 to 1 (for tests).
        static double opAmpGain (double knob) noexcept;
        // The booster's gain at high frequencies (for tests).
        static double boosterTrebleGain() noexcept;

    private:
        void buildTone (Circuit& c, bool audioOut) noexcept;

        double rate = 384000.0;
        double distKnob = -1.0, toneKnob = -1.0;

        // The booster: a first-order high-pass shelf (the 47 nF into the shunt
        // feedback), as a bilinear one-pole, then its swing limits.
        double boostB0 = 0.0, boostB1 = 0.0, boostA1 = 0.0, boostIn = 0.0, boostOut = 0.0;
        // C5 68n into R10 100k (23 Hz), before the op-amp.
        double couplingG = 0.0, couplingIn = 0.0, couplingOut = 0.0;
        Circuit opAmpStage;          // the non-inverting gain stage, DIST in its feedback
        int distUpper = 0, distLower = 0;

        // The clipper: R14 2.2k and C9 0.47 uF into the diodes; the TONE stack's
        // load on them (through a 10 ohm sense resistor), and the same stack's output.
        double gCoupling = 0.0, jCoupling = 0.0, diodeNode = 0.0, diodeNodePrev = 0.0;
        Circuit toneLoad, toneOut;
        int toneLoadUpper = 0, toneLoadLower = 0, toneOutUpper = 0, toneOutLower = 0;
    };
}
