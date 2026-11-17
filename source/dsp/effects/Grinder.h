#pragma once

#include "Circuit.h"

namespace effects
{
    // Grinder: OD / DISTORTION's heavy metal distortion, modelled on the 1983
    // compact heavy metal pedal (the "chainsaw" of Swedish death metal), from
    // its original factory schematic (as published by Hobby-Hour):
    //
    //   buffer -> Q6 (NPN) -> Q7 (PNP) -> op-amp 1b with asymmetric diodes in its feedback
    //          -> 10k, two germanium diodes in series with the signal, 10k, two silicon diodes to ground
    //          -> buffer -> the COLOR MIX EQ (three gyrator boosts) -> LEVEL -> buffer
    //
    // Q6 and Q7 are shunt-feedback stages (22k and 47 nF in, 470k and 100 pF
    // from collector to base): each about 15 to 20 times. The DIST pot (250k)
    // does two jobs: one end, through 150 ohm and 10 uF, shunts Q6's collector
    // (turned down, Q6 gives almost nothing); the other, through 47k and
    // 47 nF, is op-amp 1b's leg to ground (gain 1.7 to 5.7). 1b's feedback is
    // 220k and 100 pF with one diode one way and two the other (soft,
    // uneven clipping). Then the germanium pair in series (grit around the
    // zero crossings; with 10k each side and a 68k load it takes only about
    // 2 dB off the quietest playing, not the strong gate it's sometimes
    // called), and the silicon pair clips hard (with 1 nF across it).
    //
    // COLOR MIX: two 10k pots between an op-amp's (+) and (-) inputs (3.3k in,
    // 3.3k and 470 pF back), their wipers to series-resonant gyrators: LOW
    // around 87 Hz (1.5 uF; 330, 100k, 68 nF), HIGH two at once, around
    // 960 Hz (0.15 uF; 330, 82k, 6.8 nF) and 1.28 kHz (0.1 uF; 330, 100k,
    // 4.7 nF). Turned up, a pot boosts its band; down, it cuts. All the way up
    // they boost far enough to clip the EQ's op-amp too.
    //
    // Modelled: the op-amp, diode and EQ stages from their parts (Newton's
    // method on the diode nodes; the EQ as a Circuit). Our reading: Q6 and Q7
    // as inverting stages with their open-loop gain from their resistors and
    // bias (not solved transistor by transistor), the germanium diodes' curve
    // (the schematic doesn't name the part), and the DIST and COLOR MIX pots'
    // tapers (Boss's "D" and "G"), taken as linear.
    class Grinder
    {
    public:
        void prepare (double oversampledRate, double baseRate) noexcept;
        void reset() noexcept;

        // DIST, 0 to 1. Call once per block.
        void setDistortion (double knob) noexcept;
        // COLOR MIX LOW and HIGH, 0 to 1 (0.5 flat). Call once per block.
        void setColour (double low, double high) noexcept;

        // The gain stages and clippers, at the oversampled rate: guitar volts in, volts out.
        double clip (double volts) noexcept;
        // The COLOR MIX EQ, at the base rate.
        double equalise (double volts) noexcept;

        // Op-amp 1b's gain above its corner, for DIST 0 to 1 (for tests).
        static double opAmpGain (double knob) noexcept;

    private:
        struct Biquad
        {
            double b0 = 0.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
            void setAnalog (double nb0, double nb1, double nb2, double na0, double na1, double na2, double rate) noexcept;
            double process (double x) noexcept;
            void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
        };
        // One shunt-feedback transistor stage with open-loop gain A: 22k + 47 nF in, 470k // 100 pF back.
        static void setShuntStage (Biquad&, double openGain, double rate) noexcept;

        double rate = 384000.0;
        double distKnob = -1.0, lowKnob = -1.0, highKnob = -1.0;
        Biquad q6, q7;

        // Op-amp 1b: its leg to ground (47k + the pot's half, and 47 nF), the 100 pF and the diode node.
        double groundOhms = 47.0e3, groundCapVolts = 0.0, groundAmps = 0.0, groundK = 0.0;
        double feedbackCapG = 0.0, feedbackCapHistory = 0.0, diodeVolts = 0.0, diodeVoltsPrev = 0.0;

        // C12 1 uF into R23 10k, the germanium pair, R30 10k, the silicon pair // C16 1 nF // R24 68k.
        double gCoupling = 0.0, jCoupling = 0.0;
        double seriesAmps = 0.0, seriesAmpsPrev = 0.0, clipVolts = 0.0, clipVoltsPrev = 0.0;
        double clipCapG = 0.0, clipCapHistory = 0.0;

        Circuit eq;
        int lowTop = 0, lowBottom = 0, highTop = 0, highBottom = 0;
    };
}
