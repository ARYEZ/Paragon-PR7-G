#pragma once

#include "Circuit.h"

namespace effects
{
    // Sapphire: OD / DISTORTION's blues overdrive, modelled on the 1995 compact
    // blues overdrive (the blue one), from its factory schematic (as published
    // by Hobby-Hour). Its path, as drawn:
    //
    //   buffer -> stage 1 -> filter network -> four diodes to ground -> stage 2
    //          -> treble shelf -> TONE -> LEVEL -> mid-shaping op-amp stage -> buffer
    //
    // The two gain stages are op-amps built from transistors (a JFET pair and
    // a PNP each), not chips. GAIN is a dual 250k audio-taper pot, one half in
    // each stage's feedback:
    //   stage 1: 22k + GAIN, 47 pF across, over 1.5k + 0.15 uF (700 Hz): about
    //            16x to 180x above 700 Hz, the bass left at unity;
    //   stage 2: 33k + GAIN, 100 pF across, over 2.2k + 1 uF (72 Hz): about 16x
    //            to 130x above 72 Hz.
    // Between them, 100k into 0.047 uF + 15k (a treble cut), 0.1 uF on to the
    // diodes, with 220 pF + 330k bringing some treble round it, and 1M; the
    // diodes are two in series each way to ground, clipping at about +/-1.2 V.
    // After stage 2: 5.6k with 5.6 nF across into 5.6 nF (about 6 dB off the
    // top), TONE (10k linear between two 0.018 uF), LEVEL (100k audio), and an
    // op-amp whose feedback (6.8k // 2.2 nF) works against a resonant network
    // around a transistor follower (0.056 uF twice, 1.2k, 470k): its voice.
    //
    // Modelled from the parts: every filter and pot (the tapers are printed on
    // the schematic), the diodes (Newton's method; the network around them,
    // which has two inputs, solved as two linear halves added together). Our
    // reading: the transistor op-amps as ideal op-amps whose outputs level off
    // unevenly, at +3.6 V and -3.8 V around their bias (the PNP pulls up, its
    // 2.2k load pulls down), and the diodes as 1N4148-class parts (1SS133).
    class Sapphire
    {
    public:
        void prepare (double oversampledRate, double baseRate) noexcept;
        void reset() noexcept;

        // GAIN and TONE, 0 to 1. Call once per block.
        void setGain (double knob) noexcept;
        void setTone (double knob) noexcept;

        // The gain stages and diodes, at the oversampled rate: guitar volts in.
        double drive (double volts) noexcept;
        // The shelf, TONE and the last op-amp stage, at the base rate.
        double finish (double volts) noexcept;

        // Each gain stage's gain above its corner, for GAIN 0 to 1 (for tests).
        static double stageOneGain (double knob) noexcept;
        static double stageTwoGain (double knob) noexcept;

    private:
        double rate = 384000.0;
        double gainKnob = -1.0, toneKnob = -1.0;

        // C18 0.1 uF into R23 220k before stage 1.
        double inputG = 0.0, inputIn = 0.0, inputOut = 0.0;
        Circuit stageOne, stageTwo;
        int stageOnePot = 0, stageTwoPot = 0;

        // The network between the stages, as two halves: driven by stage 1
        // with the diode node held at 0 V, and driven by the diode node with
        // stage 1 held at 0 V. Each gives the voltage beside the diodes (behind
        // a 10 ohm sense resistor); added, they give the circuit's.
        Circuit fromStage, fromDiodes;
        double diodeVolts = 0.0, diodeVoltsPrev = 0.0;
        // C27 2.2 nF into R35 1M: into stage 2.
        double gateG = 0.0, gateIn = 0.0, gateOut = 0.0;

        Circuit tail;
        int toneUpper = 0, toneLower = 0;
    };
}
