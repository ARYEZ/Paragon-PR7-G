#include "FactoryRigs.h"

#include "Parameters.h"
#include "dsp/effects/Cipher.h"

namespace
{
    Rig make (const juce::String& name, int bank, int number, const juce::String& tags,
              std::initializer_list<std::pair<const char*, float>> changes)
    {
        Rig rig;
        rig.name = name;
        rig.bank = bank;
        rig.number = number;
        rig.tags = juce::StringArray::fromTokens (tags, ",", {});
        rig.keepAmpAndCab = true;
        // Only the changes from the defaults: loading a rig resets every
        // parameter it doesn't mention to its default (every effect off).
        for (const auto& [id, value] : changes)
            rig.parameters[id] = value;
        return rig;
    }

    // A CIPHER rig's drawn parts: envelope shapes by slot (1-based, Cipher::shape
    // numbers), the sequencer's steps, the macros and their names.
    Rig withCipher (Rig rig, std::initializer_list<std::pair<int, int>> shapes, const juce::String& sequence = {},
                    const juce::String& macros = {}, const juce::String& names = {})
    {
        for (const auto& [slot, shape] : shapes)
            rig.cipherEnvelopes[slot - 1] = effects::Cipher::toText (effects::Cipher::shape (shape));
        rig.cipherSequence = sequence;
        rig.cipherMacros = macros;
        rig.cipherMacroNames = names;
        return rig;
    }

    // Delay sync choices (index into effects::Delay::syncNames).
    constexpr float syncOff = 0, quarter = 3, eighth = 6, dottedEighth = 7;
    // Modulation types (index into effects::Modulation::typeNames).
    constexpr float chorus = 0, flanger = 1, tremolo = 3, dimension = 4, uniVibe = 5, rotary = 6, vibrato = 7,
                    harmonicTremolo = 8, autoPan = 9, doubler = 10;
    // FILTER / PITCH types.
    constexpr float autoWah = 0, pitchShifter = 1, whammyWah = 2, talkBoxType = 3;
    constexpr float pedalTalkBox = 8;   // the expression pedal's TALK BOX setting
    constexpr float daybreakComp = 1;   // COMP / GATE's TYPE
    constexpr float glitchSequence = 1, glitchTexturizer = 3, glitchSixteenth = 3;   // GLITCH's MODE and LENGTH
    constexpr float pedalGlitch = 7;   // the expression pedal's GLITCH setting
    constexpr float pedalWah = 2;      // the expression pedal's WAH / FILTER setting
    constexpr float glitchKillswitch = 4;
    constexpr int E = 4;               // bank E: rigs inspired by particular songs
    constexpr float cosmonaut = 2, halo = 3, abyss = 6;   // OD / DISTORTION's MODEL
    // HAVOK's scratch programs (61-68, indices 60-67) and sounds, and X for each speed.
    constexpr float scratchBaby = 60, scratchChirp = 61, scratchTransformer = 62, scratchFlare = 63, scratchTear = 64,
                    scratchCrab = 65, scratchScribble = 66, scratchManual = 67;
    constexpr float soundStab = 3, soundDropD = 4;   // SOUND: the stabs scratch best
    constexpr float rateEighth = 0.5f, rateSixteenth = 0.75f;
    constexpr float digitalDelay = 0;
    constexpr float xpCryWah = 1, xpUpOctave = 12, xpOctDownOctUp = 20, xpUp4thUp5th = 23;   // Whammy-Wah programs 02, 13, 21 and 24
    constexpr float pedalWhammyWah = 6;   // the expression pedal's WHAMMY-WAH setting
    // Delay types (index into effects::Delay::typeNames).
    constexpr float analogDelay = 1, tapeDelay = 2, modDelay = 3, reverseDelay = 4, multiTap = 5, shimmer = 6, ducking = 7;
    // Reverb types (index into effects::Reverb::typeNames).
    constexpr float room = 1, hall = 2, cathedral = 3, plate = 4, spring = 5, digital = 6, reverse = 7;
    // LO-FI types.
    constexpr float telephone = 0, amRadio = 1, megaphone = 2, cassette = 3, bitcrusher = 4;

    // For bank F and the album banks after it.
    constexpr int F = 5;                 // bank F: an album's guitar sounds (Spiritbox, Eternal Blue)
    constexpr float phaser = 2;          // MOD's Hadron
    constexpr float banshee = 1;         // OD / DISTORTION's MODEL: the Tube Screamer, as a boost into the amp
    constexpr float pedalVolume = 1;     // the expression pedal as a volume pedal (swells)
    constexpr float sixteenth = 9;       // DELAY / MOD sync: 1/16
    constexpr float havokRingFilter = 41;   // HAVOK program 42: RING MOD+FILTER
    constexpr int G = 6;                 // bank G: an album's guitar sounds (Korn, Korn)
    constexpr float wholeNote = 1;       // DELAY sync: 1/1, a bar in 4/4
    constexpr float xpBrassAttack = 8;   // QUASAR program 09: Brass Attack Sens (an envelope low-pass)
    constexpr int H = 7;                 // bank H: an album's guitar sounds (Linkin Park, Hybrid Theory)
    constexpr float glitchInterfere = 2;
    constexpr float xpEnvFilter = 7;     // QUASAR program 08: Env Filter Sens
    constexpr int I = 8;                 // bank I: an album's guitar sounds (Periphery, Periphery III: Select Difficulty)
    constexpr float xpDiveBomb = 19;     // QUASAR program 20: Down 6 Octaves, a dive bomb
    constexpr int J = 9;                 // bank J: an album's guitar sounds (Sevendust, Next)
    constexpr int K = 10;                // bank K: an album's guitar sounds (Tool, Aenima)
    constexpr float glitchEighth = 5;    // GLITCH's LENGTH: 1/8
    constexpr int L = 11;                // bank L: an album's guitar sounds (Rage Against the Machine, Evil Empire)
    constexpr float xpUpTwoOctaves = 13, xpUpSecond = 14, xpDownTwoOctaves = 18;   // QUASAR programs 14, 15 and 19
    constexpr int M = 12;                // bank M: an album's guitar sounds (Cold, 13 Ways to Bleed on Stage)
    constexpr int N = 13;                // bank N: an album's guitar sounds (30 Seconds to Mars, 30 Seconds to Mars)
    constexpr int O = 14;                // bank O: CIPHER, the envelope filter and multi-effect
    constexpr float pedalCipherMacro = 10;   // the expression pedal on CIPHER's MACRO 1
    constexpr int P = 15;                // bank P: an album's guitar sounds (Static-X, Wisconsin Death Trip)
}

std::vector<Rig> makeFactoryRigs()
{
    using namespace ParamIDs;
    constexpr int A = 0, B = 1, C = 2, D = 3;

    return {
        make ("Clean Slate", A, 1, "Clean", {}),

        make ("Studio Clean", A, 2, "Clean", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 }, { compLevel, 3 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.4f }, { reverbMix, 0.2f } }),

        make ("Chorus Clean", A, 3, "Clean,Modulation", {
            { compOn, 1 }, { compThreshold, -22 }, { compRatio, 3 }, { compLevel, 2 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.6f }, { modDepth, 0.5f }, { modMix, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.25f } }),

        make ("Edge of Breakup", A, 4, "Crunch,Drive", {
            { driveOn, 1 }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbMix, 0.2f } }),

        // Daybreak on a clean: smooth, even and sustaining, with half the dry
        // guitar mixed back in so every pick attack still speaks.
        make ("Daybreak Clean", A, 5, "Clean,Compressor", {
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 6 }, { compDetail, 6 }, { compMix, 0.5f }, { compLevel, 1 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.4f }, { modDepth, 0.3f }, { modMix, 0.25f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.2f } }),

        make ("Lead Boost", B, 1, "Lead,Drive", {
            { compThreshold, -20 }, { gateThreshold, -60 }, { compOn, 1 }, { compRatio, 2 },
            { driveOn, 1 }, { driveGain, 4 }, { driveTone, 6 }, { driveLevel, 6 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.25f }, { rigLevel, 2 } }),

        // A low-gain overdrive in front of a high-gain amp tightens the bass
        // and sharpens the attack: the classic metal "boost" trick.
        make ("High Gain Tight", B, 2, "High Gain,Rhythm", {
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -55 },
            { driveOn, 1 }, { driveGain, 1 }, { driveTone, 6 }, { driveLevel, 8 },
            { presence, 2 } }),

        make ("Ambient Swells", C, 1, "Ambient,Clean", {
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 4 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.25f }, { modDepth, 0.6f }, { modMix, 0.35f },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.6f }, { delayMix, 0.35f },
            { delayTone, 5 }, { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.9f }, { reverbTone, 5 }, { reverbMix, 0.45f } }),

        make ("Slapback", C, 2, "Rockabilly,Delay", {
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, syncOff }, { delayTime, 110 }, { delayFeedback, 0.1f },
            { delayMix, 0.3f }, { delayTone, 5 },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.25f }, { reverbMix, 0.15f } }),

        make ("Surf Tremolo", C, 3, "Surf,Modulation", {
            { modOn, 1 }, { modType, tremolo }, { modRate, 6 }, { modDepth, 0.6f },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.7f }, { reverbTone, 7 }, { reverbMix, 0.4f } }),

        make ("Jet Flanger", D, 1, "Modulation,Drive", {
            { driveOn, 1 }, { driveGain, 3 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.2f }, { modDepth, 0.8f }, { modManual, 5 }, { modRes, 7 } }),

        make ("Rotary Spin", D, 2, "Modulation,Clean", {
            { compOn, 1 }, { compThreshold, -20 }, { compRatio, 3 },
            // Cyclone on FAST, its mics a little back from the rotors.
            { modOn, 1 }, { modType, rotary }, { modCycFast, 1 }, { modCycLow, 6 }, { modCycHigh, 6 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.2f } }),

        make ("Nebula Lead", D, 3, "Modulation,Drive,Lead", {
            { driveOn, 1 }, { driveGain, 6 }, { driveTone, 5 }, { driveLevel, 3 },
            { modOn, 1 }, { modType, uniVibe }, { modRate, 1.2f }, { modDepth, 0.75f }, { modMix, 0.5f },
            { reverbOn, 1 }, { reverbMix, 0.18f } }),

        make ("Prism Clean", D, 4, "Clean,Modulation", {
            { compOn, 1 }, { compThreshold, -22 }, { compRatio, 3 }, { compLevel, 2 },
            { modOn, 1 }, { modType, dimension }, { modRate, 0.4f }, { modDepth, 0.5f }, { modMix, 0.5f },
            { delayOn, 1 }, { delayType, modDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        make ("Warble Vibrato", D, 5, "Modulation,Lo-Fi", {
            { modOn, 1 }, { modType, vibrato }, { modRate, 5.5f }, { modDepth, 0.35f },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.5f }, { reverbTone, 4 }, { reverbMix, 0.3f } }),

        make ("Octave Lead", B, 3, "Lead,Pitch,Drive", {
            { compOn, 1 }, { compThreshold, -20 }, { compRatio, 3 },
            { fpOn, 1 }, { fpType, pitchShifter }, { fpInterval, 12 }, { fpMix, 0.45f },
            { driveOn, 1 }, { driveGain, 5 }, { driveTone, 6 }, { driveLevel, 2 },
            { delayOn, 1 }, { delayType, ducking }, { delaySync, dottedEighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbMix, 0.2f } }),

        make ("Harmony Fifths", B, 4, "Lead,Pitch", {
            { fpOn, 1 }, { fpType, pitchShifter }, { fpInterval, 7 }, { fpMix, 0.4f },
            { driveOn, 1 }, { driveGain, 4 }, { driveTone, 5 },
            { reverbOn, 1 }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        make ("Funk Envelope", C, 6, "Funk,Filter,Clean", {
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 4 },
            { fpOn, 1 }, { fpType, autoWah }, { fpSensitivity, 6 }, { fpResonance, 6 }, { fpMix, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.3f }, { reverbMix, 0.15f } }),

        make ("Wide Doubler", D, 6, "Rhythm,Modulation", {
            { driveOn, 1 }, { driveGain, 3 }, { driveTone, 5 },
            { modOn, 1 }, { modType, doubler }, { modRate, 0.8f }, { modDepth, 0.4f }, { modMix, 0.7f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.35f }, { reverbMix, 0.15f } }),

        // LO-FI: for intros and breakdowns. Assign the expression pedal to
        // LO-FI to sweep from the full band (heel) into the effect (toe).
        make ("Quasar Lead", B, 7, "Lead,Pitch,Drive,Expression", {
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpOctave }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveGain, 6 }, { driveTone, 6 }, { driveLevel, 3 },
            { delayOn, 1 }, { delaySync, dottedEighth }, { delayFeedback, 0.25f }, { delayMix, 0.18f },
            { reverbOn, 1 }, { reverbMix, 0.2f } }),

        make ("Quasar Wah Crunch", B, 8, "Wah,Drive,Expression", {
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpCryWah }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveGain, 4 }, { driveTone, 5 }, { driveLevel, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.3f }, { reverbMix, 0.15f } }),

        make ("Harmony Bends", B, 9, "Lead,Pitch,Expression", {
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUp4thUp5th }, { fpMix, 0.8f },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveGain, 5 }, { driveTone, 6 },
            { delayOn, 1 }, { delaySync, quarter }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        // The late-'90s nu-metal screech: the XP-100's program 21 (an octave
        // down and an octave up, blended with the guitar), rocked while a
        // high note rings, into a tight Screamer boost and a high-gain amp
        // (load a Rectifier-style capture). The EQ is the Tight Low End
        // preset, so the octave-down voice doesn't turn to mud.
        make ("Nu-Metal Screech", B, 10, "High Gain,Pitch,Expression", {
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -55 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpOctDownOctUp }, { fpMix, 0.85f },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveGain, 1 }, { driveTone, 6 }, { driveLevel, 8 },
            { presence, 2 },
            { eqOn, 1 }, { "eq63", -6 }, { "eq125", -4 }, { "eq250", -1 }, { "eq1k", 1 }, { "eq2k", 2 }, { "eq4k", 2 }, { "eq8k", 1 },
            { eqLevel, -0.5f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.25f }, { reverbMix, 0.1f } }),

        // Nu-metal turntable rigs: a scooped, gated metal distortion and
        // HAVOK's scratch programs (ours, on our own sounds) in time with
        // the song, all on the stabs (the owner's pick: their hard front
        // edge is what the crossfader cuts bite on). Touch HAVOK's pad, or send MIDI CC 92 from a footswitch,
        // to bring the scratching in; the PROGRAM MAP keys hold the other moves
        // (1 BABY, 2 CHIRP, 3 TRANSFORMER, 4 FLARE, 5 TEAR, 6 MANUAL).
        make ("Nu-Metal Turntable", B, 11, "High Gain,Scratch,Havok", {
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -55 },
            { driveOn, 1 }, { driveModel, abyss }, { driveGain, 6 }, { driveLow, 7 }, { driveMiddle, 3 }, { driveHigh, 6 },
            { eqOn, 1 }, { "eq63", -3 }, { "eq500", -2 }, { "eq1k", -1 }, { "eq4k", 2 },
            { havokOn, 1 }, { havokProgram, scratchChirp }, { havokScratchSound, soundStab }, { havokX, rateSixteenth }, { havokY, 0.7f },
            { "havokMap1Program", scratchBaby }, { "havokMap2Program", scratchChirp }, { "havokMap3Program", scratchTransformer },
            { "havokMap4Program", scratchFlare }, { "havokMap5Program", scratchTear }, { "havokMap6Program", scratchManual } }),

        // A drop-D stab cut up by the crossfader: the transformer's stutter.
        make ("Stab Transformer", B, 12, "High Gain,Scratch,Havok", {
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -55 },
            { driveOn, 1 }, { driveModel, abyss }, { driveGain, 5 }, { driveLow, 6 }, { driveMiddle, 4 }, { driveHigh, 6 },
            { havokOn, 1 }, { havokProgram, scratchTransformer }, { havokScratchSound, soundDropD }, { havokX, rateEighth }, { havokY, 0.55f },
            { "havokMap1Program", scratchTransformer }, { "havokMap2Program", scratchCrab }, { "havokMap3Program", scratchFlare },
            { "havokMap4Program", scratchChirp }, { "havokMap5Program", scratchScribble }, { "havokMap6Program", scratchManual } }),

        // A distorted chord stab scribbled and torn over a heavy rhythm.
        make ("Stab Scribble", B, 13, "High Gain,Scratch,Havok", {
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -55 },
            { driveOn, 1 }, { driveModel, abyss }, { driveGain, 6 }, { driveLow, 7 }, { driveMiddle, 3 }, { driveHigh, 5 },
            { havokOn, 1 }, { havokProgram, scratchScribble }, { havokScratchSound, soundStab }, { havokX, rateEighth }, { havokY, 0.6f },
            { "havokMap1Program", scratchScribble }, { "havokMap2Program", scratchTear }, { "havokMap3Program", scratchBaby },
            { "havokMap4Program", scratchCrab }, { "havokMap5Program", scratchChirp }, { "havokMap6Program", scratchManual } }),

        // Tempo-locked stutters on a crunch, faded in with the pedal: rock
        // forward on a transition or the end of a riff.
        make ("Stutter Edit", C, 10, "Glitch,Drive,Expression", {
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 6 }, { glitchChaos, 2 }, { glitchMix, 1 },
            { expressionTarget, pedalGlitch },
            { driveOn, 1 }, { driveGain, 5 }, { driveTone, 6 }, { driveLevel, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbMix, 0.12f } }),

        // A slow cloud of grains under a clean tone: chords smear into a pad.
        make ("Grain Cloud", D, 10, "Glitch,Ambient,Clean", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchSync, 0 }, { glitchTime, 250 },
            { glitchDensity, 4 }, { glitchChaos, 5 }, { glitchMix, 0.45f },
            { delayOn, 1 }, { delaySync, quarter }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.8f }, { reverbMix, 0.35f } }),

        //======================================================================
        // Bank E: inspired by particular songs. Our settings for the sound,
        // not the artists' own (which are their own gear, played their way).

        // Rage Against the Machine, "Killing in the Name" solo: a Whammy an
        // octave up, rocked to the toe for the squeal, into a cranked crunch
        // with no reverb. (For a higher squeal, PROGRAM 14 Up 2 Octaves.)
        make ("Octave Squeal Solo", E, 1, "Song,Lead,Pitch,Expression", {
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpOctave }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 6 },
            { presence, 1 } }),

        // Rage Against the Machine, "Bulls on Parade" scratch: a wah on the
        // pedal, rocked as you rub the muted strings over the pickup, and
        // the toggle-switch chop (GLITCH Killswitch, 1/16, on the song's
        // grid when the DAW plays).
        make ("Wah Scratch Chop", E, 2, "Song,Wah,Glitch,Expression", {
            { expressionTarget, pedalWah },
            { glitchOn, 1 }, { glitchMode, glitchKillswitch }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 0 }, { glitchMix, 1 },
            { driveOn, 1 }, { driveGain, 4 }, { driveTone, 6 }, { driveLevel, 5 },
            { presence, 1 } }),

        // Pink Floyd. David Gilmour's sounds as his gear guides describe them;
        // the rest is in his hands (vibrato, bends, note choice).

        // "Comfortably Numb" solos: a Big Muff-style fuzz (sustain about 70%,
        // tone 30-40%; Cosmonaut, the Green Russian, stands in for his
        // earlier US ones, its LEVEL set to match) into a delay of about 370 ms, through a subtly mixed
        // rotating speaker (his Yamaha RA-200), with studio reverb.
        make ("Soaring Fuzz Solo", E, 3, "Song,Lead,Fuzz", {
            { driveOn, 1 }, { driveModel, cosmonaut }, { driveGain, 7 }, { driveTone, 3.5f }, { driveLevel, -1.5f },
            // Cyclone slow and mixed in lightly (a third of the way into each rotor's MIX).
            { modOn, 1 }, { modType, rotary }, { modCycFast, 0 }, { modCycSlowSpeed, 0.9f }, { modCycLow, 1.5f }, { modCycHigh, 1.5f },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, syncOff }, { delayTime, 370 }, { delayFeedback, 0.3f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.18f } }),

        // "Us and Them": the neck pickup, clean, through a slow rotating
        // speaker (a Leslie dominated it; a Uni-Vibe is close too), with echo.
        // For the louder chorus, stomp footswitch 2: Halo, the germanium
        // fuzz, is set up and waiting, as Gilmour kicked his in.
        make ("Slow Rotary Clean", E, 4, "Song,Clean,Modulation,Fuzz", {
            { driveOn, 0 }, { driveModel, halo }, { driveGain, 6 }, { driveLevel, 0 },
            // Cyclone slow and well in the sound.
            { modOn, 1 }, { modType, rotary }, { modCycFast, 0 }, { modCycSlowSpeed, 0.7f }, { modCycLow, 4 }, { modCycHigh, 4 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, syncOff }, { delayTime, 420 }, { delayFeedback, 0.25f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.25f } }),

        // "Run Like Hell": muted eighths into a long, clean delay (about
        // 380 ms, five or six repeats) that makes the gallop, with flanging
        // and light compression.
        make ("Galloping Delay", E, 5, "Song,Clean,Delay,Modulation", {
            { compOn, 1 }, { compThreshold, -22 }, { compRatio, 3 }, { compLevel, 2 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.3f }, { modDepth, 0.5f }, { modManual, 4 }, { modRes, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, syncOff }, { delayTime, 380 }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, room }, { reverbMix, 0.12f } }),

        // "Money" solos: the bridge pickup into Halo, the germanium fuzz, with
        // a multi-head echo in the spirit of his Binson Echorec.
        make ("Fuzz Echo Solo", E, 6, "Song,Lead,Fuzz,Delay", {
            { driveOn, 1 }, { driveModel, halo }, { driveGain, 7 }, { driveLevel, 1 },
            { delayOn, 1 }, { delayType, multiTap }, { delaySync, syncOff }, { delayTime, 330 }, { delayFeedback, 0.3f }, { delayMix, 0.22f },
            { reverbOn, 1 }, { reverbType, room }, { reverbMix, 0.15f } }),

        // Tool, "Jambi" solo: a talk box on a tight, heavy tone. Load a
        // high-gain amp capture; the pedal opens the "mouth" from oo to ee.
        make ("Heavy Talk Box", E, 7, "Song,Lead,Talk Box,Expression", {
            { fpOn, 1 }, { fpType, talkBoxType }, { fpResonance, 6 }, { fpMix, 1 },
            { expressionTarget, pedalTalkBox },
            { driveOn, 1 }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 6 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.2f }, { delayMix, 0.12f },
            { reverbOn, 1 }, { reverbType, room }, { reverbMix, 0.1f } }),

        // Peter Frampton, "Show Me the Way": the talk box on a warm, lightly
        // driven tone, the most famous talk box sound there is.
        make ("Warm Talk Box", E, 8, "Song,Lead,Talk Box,Expression", {
            { fpOn, 1 }, { fpType, talkBoxType }, { fpResonance, 5 }, { fpMix, 1 },
            { expressionTarget, pedalTalkBox },
            { driveOn, 1 }, { driveModel, 0 }, { driveGain, 3 }, { driveTone, 5 }, { driveLevel, 2 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.2f } }),

        make ("Phone Intro", B, 5, "Lo-Fi,Clean", {
            { compOn, 1 }, { compThreshold, -22 }, { compRatio, 3 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.35f }, { reverbMix, 0.2f },
            { lofiOn, 1 }, { lofiType, telephone }, { lofiAmount, 6 }, { lofiNoise, 3 }, { lofiLevel, 2 } }),

        make ("Megaphone Verse", B, 6, "Lo-Fi,Drive", {
            { driveOn, 1 }, { driveGain, 5 }, { driveTone, 6 },
            { lofiOn, 1 }, { lofiType, megaphone }, { lofiAmount, 6 }, { lofiNoise, 2 } }),

        make ("Old Radio", C, 7, "Lo-Fi,Vintage", {
            { modOn, 1 }, { modType, tremolo }, { modRate, 4.5f }, { modDepth, 0.3f },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.5f }, { reverbMix, 0.3f },
            { lofiOn, 1 }, { lofiType, amRadio }, { lofiAmount, 5 }, { lofiNoise, 5 }, { lofiLevel, 1 } }),

        make ("Cassette Dreams", D, 7, "Lo-Fi,Ambient", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.4f }, { modDepth, 0.4f }, { modMix, 0.35f },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, digital }, { reverbSize, 0.8f }, { reverbMix, 0.35f },
            { lofiOn, 1 }, { lofiType, cassette }, { lofiAmount, 5 }, { lofiNoise, 4 } }),

        make ("8-Bit Crunch", D, 8, "Lo-Fi,Drive", {
            { driveOn, 1 }, { driveGain, 3 },
            { lofiOn, 1 }, { lofiType, bitcrusher }, { lofiAmount, 6 }, { lofiNoise, 0 }, { lofiMix, 0.8f } }),

        // A reverse reverb: each note swells up out of nowhere.
        make ("Reverse Swell", C, 8, "Ambient,Reverb", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delaySync, quarter }, { delayFeedback, 0.3f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbType, reverse }, { reverbSize, 0.5f }, { reverbMix, 0.55f }, { reverbPreDelay, 0 } }),

        // Delays to try: a rising shimmer pad, and reverse echoes.
        make ("Shimmer Pad", D, 9, "Ambient,Delay,Clean", {
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 3 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.65f }, { delayMix, 0.4f },
            { delayTone, 6 }, { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.7f }, { reverbMix, 0.4f } }),

        make ("Backwards Echo", C, 9, "Psychedelic,Delay", {
            { driveOn, 1 }, { driveGain, 3 },
            { delayOn, 1 }, { delayType, reverseDelay }, { delaySync, eighth }, { delayFeedback, 0.4f }, { delayMix, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbMix, 0.2f } }),

        // These two follow the tempo (tapped, or the DAW's).
        make ("Vintage Amp Trem", C, 4, "Modulation,Clean,Vintage", {
            { modOn, 1 }, { modType, harmonicTremolo }, { modSync, eighth }, { modDepth, 0.7f },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.55f }, { reverbTone, 6 }, { reverbMix, 0.3f } }),

        make ("Wide Pan", C, 5, "Modulation,Ambient", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, autoPan }, { modSync, quarter }, { modDepth, 0.8f },
            { delayOn, 1 }, { delayType, multiTap }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.8f }, { reverbMix, 0.35f } }),

        //======================================================================
        // Bank F: an album's guitar sounds, inspired by Spiritbox's Eternal
        // Blue (2021; the owner's choice). Cleans, effects and leads only: the
        // rhythm distortion is the amp capture's job, and factory rigs keep
        // the user's amp and cab. Named for the sound (no artist, album or
        // song words in the plugin); the CHANGELOG and README credit the
        // songs. Documented: the album was recorded with amp-sim plugins,
        // most songs have five or six layers of guitar ambience (the most on
        // the closer), and the "Secret Garden" verses are a very dry,
        // DI-like clean. The rest is our reading of how these sounds are
        // commonly made, not the record's settings.

        // The opening's ambient clean: a soft octave-up shimmer on the echoes
        // and a big, ducked cathedral.
        make ("Dawn Shimmer Intro", F, 1, "Album,Clean,Ambient,Delay", {
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 }, { compLevel, 2 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.35f }, { modDepth, 0.35f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
            { delayTone, 6 }, { delayPingPong, 1 }, { delayDuck, 0.4f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.8f }, { reverbMix, 0.35f }, { reverbDuck, 0.3f } }),

        // The chorus's singing lead: a light boost into your amp's gain, a
        // dotted-eighth echo that ducks under the notes, a hall behind.
        make ("Aching Chorus Lead", F, 2, "Album,Lead,Delay", {
            { rigLevel, -9 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 6 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.2f } }),

        // Stinging stutters: sixteenth-note repeats grabbed from what you
        // play (on the song's grid when the DAW plays), over your heavy amp.
        make ("Wasp Stutter", F, 3, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 2 }, { glitchMix, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.3f }, { reverbMix, 0.08f } }),

        // Volume swells into a huge space: the expression pedal is a volume
        // pedal (rock it up under each chord), a rising shimmer and a cathedral.
        make ("Peak Swell Pad", F, 4, "Album,Clean,Ambient,Expression", {
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -28 }, { compRatio, 4 }, { compLevel, 3 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.65f }, { delayMix, 0.4f },
            { delayTone, 5 },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.9f }, { reverbMix, 0.5f } }),

        // The "really dry" verse clean, close to a bare DI: firm compression,
        // the lows trimmed and the top lifted, no echo and almost no room.
        make ("Bare DI Clean", F, 5, "Album,Clean,Dry", {
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 6 }, { compDetail, 6 }, { compMix, 1 },
            { eqOn, 1 }, { "eq63", -4 }, { "eq125", -2 }, { "eq500", -1 }, { "eq4k", 2 }, { "eq8k", 3 } }),

        // The chorus's bloom: the clean opened out wide with a lush stereo
        // chorus and a bright plate.
        make ("Blooming Chorus Clean", F, 6, "Album,Clean,Modulation", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, dimension }, { modDepth, 0.6f }, { modMix, 0.5f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.55f }, { reverbMix, 0.28f } }),

        // A glassy lead an octave up, doubled with the note you play, with a
        // short echo: thin, high and singing.
        make ("Glass Octave Lead", F, 7, "Album,Lead,Pitch,Octave", {
            { rigLevel, -9 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.8f }, { octUp, 0.7f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 7 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.18f } }),

        // A calm, hazy clean: a wide dual chorus, a long drifting tape echo
        // and a cathedral, both ducked so the notes stay clear.
        make ("Calm Water Haze", F, 8, "Album,Clean,Ambient", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, dimension }, { modDepth, 0.8f }, { modMix, 0.5f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.4f }, { reverbDuck, 0.4f } }),

        // The guitar as an electronic texture: a cloud of grains smeared into
        // a pad, through a warbling cassette, in a big digital space.
        make ("Machine Dream Texture", F, 9, "Album,FX,Ambient,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 6 }, { glitchChaos, 4 }, { glitchMix, 0.6f },
            { lofiOn, 1 }, { lofiType, cassette }, { lofiAmount, 4 }, { lofiNoise, 2 }, { lofiMix, 0.7f },
            { reverbOn, 1 }, { reverbType, digital }, { reverbSize, 0.8f }, { reverbMix, 0.35f } }),

        // The squealing riff accents: QUASAR an octave up on the expression
        // pedal (heel: your note, toe: an octave up), rocked on the accents,
        // over your heavy amp.
        make ("Riff Squeal Bend", F, 10, "Album,FX,Pitch,Expression", {
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpOctave }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah } }),

        // A metallic shriek: HAVOK's ring modulator and filter held on (HOLD),
        // for the noisy, inharmonic accents. Move X and Y on the pad to retune it.
        make ("Ring Mod Shriek", F, 11, "Album,FX,Havok", {
            { havokOn, 1 }, { havokProgram, havokRingFilter }, { havokX, 0.65f }, { havokY, 0.7f }, { havokHold, 1 } }),

        // The rolling clean: dotted-eighth echoes bouncing side to side, a
        // gentle chorus and a big hall.
        make ("Tide Ping-Pong Clean", F, 12, "Album,Clean,Delay", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.5f }, { modDepth, 0.3f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.45f }, { delayMix, 0.3f },
            { delayPingPong, 1 }, { delayDuck, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.75f }, { reverbMix, 0.3f } }),

        // A soaring lead thrown into space: boosted, a long quarter-note echo
        // and a long hall that bloom in the gaps.
        make ("Ocean Lead", F, 13, "Album,Lead,Delay,Ambient", {
            { rigLevel, -10 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 6 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, quarter }, { delayFeedback, 0.45f }, { delayMix, 0.3f },
            { delayDuck, 0.6f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.3f }, { reverbDuck, 0.4f } }),

        // A wall of layered ambience, as if five or six cleans were stacked:
        // two extra takes either side, an octave up shimmering behind, a
        // rising shimmer echo and a cathedral.
        make ("Layered Ambience Wall", F, 14, "Album,Clean,Ambient", {
            { rigLevel, -7 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 1 }, { octUp, 0.25f },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.6f }, { modMix, 0.6f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, dottedEighth }, { delayFeedback, 0.55f }, { delayMix, 0.35f },
            { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.9f }, { reverbMix, 0.45f } }),

        // A circling clean arpeggio: a slow phaser turning, sixteenth echoes
        // tight behind the notes and a medium hall.
        make ("Orbit Arpeggio", F, 15, "Album,Clean,Modulation,Delay", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, phaser }, { modRate, 0.25f }, { modDepth, 0.6f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, sixteenth }, { delayFeedback, 0.4f }, { delayMix, 0.22f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        //======================================================================
        // Bank G: an album's guitar sounds, inspired by Korn's self-titled
        // debut (1994; the owner's second album bank). Cleans, effects and
        // leads: the detuned seven-string rhythm is the amp capture's job.
        // From Munky's track-by-track (MusicRadar, 2015): the "Blind" chord
        // through an old Silvertone amp built into its case; no Whammy yet,
        // the noises are feedback or notes "super-high up on the fretboard";
        // on "Ball Tongue" Head's bent, vibratoed noise with Munky "about a
        // bar behind him", left to right; a "super-clean", tightly muted
        // middle section on "Faget"; reversed tape swells, "violining", on
        // "Fake"; Head's "aggressive high note" wiggled on "Lies"; a rigid,
        // Ministry-like, drum-machine song in "Helmet in the Bush"; Alice in
        // Chains bending on "Predictable"; horns like hip-hop samples on
        // "Clown". Munky's Ibanez BC-9 chorus, which he calls a really
        // messed-up sounding chorus, has been on his board since this album, and Head used an
        // Electric Mistress flanger (gear lists). The settings are ours.

        // The dissonant chord through a small, boxy amp built into its case:
        // the lows and highs cut away, a nasal middle, a little grit and a
        // small room. Best on a clean or lightly driven capture.
        make ("Box Amp Dissonance", G, 1, "Album,Clean,Drive", {
            { rigLevel, -4 },
            { driveOn, 1 }, { driveModel, halo }, { driveGain, 4 }, { driveLevel, -2 },
            { eqOn, 1 }, { "eq63", -12 }, { "eq125", -9 }, { "eq250", -3 }, { "eq1k", 4 }, { "eq2k", 3 }, { "eq4k", -4 }, { "eq8k", -10 }, { "eq16k", -12 },
            { eqLevel, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.25f }, { reverbMix, 0.15f } }),

        // The creepy, warbling chorus: deep and quick enough to go seasick,
        // in the spirit of Munky's messed-up chorus pedal.
        make ("Warped Chorus Clean", G, 2, "Album,Clean,Modulation", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 2.5f }, { modDepth, 0.9f }, { modMix, 0.5f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.35f }, { reverbMix, 0.15f } }),

        // Bent, vibratoed noises answered a bar later from the other side: a
        // one-bar ping-pong echo plays your phrase back on the far speaker,
        // as the second guitar did. A light boost into your amp's gain.
        make ("Bar-Behind Bend Echo", G, 3, "Album,FX,Delay", {
            { rigLevel, -5 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, wholeNote }, { delayFeedback, 0.1f }, { delayMix, 0.5f },
            { delayPingPong, 1 } }),

        // A howling, feedback-like lead: an octave above every note (feedback
        // often jumps to it), boosted and squeezed so it sustains.
        make ("Harmonic Howl Lead", G, 4, "Album,Lead,Pitch,Octave", {
            { rigLevel, -3 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.7f }, { octUp, 0.6f },
            { compOn, 1 }, { compThreshold, -30 }, { compRatio, 6 }, { compLevel, 4 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 4 }, { driveTone, 7 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f } }),

        // The super-clean, tightly muted section: firm, quick compression, a
        // touch of top, completely dry.
        make ("Tight Muted Clean", G, 5, "Album,Clean,Dry", {
            { rigLevel, 2 },
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 7 }, { compDetail, 4 }, { compMix, 1 }, { compIntensity, 1 },
            { eqOn, 1 }, { "eq63", -3 }, { "eq2k", 2 }, { "eq4k", 2 } }),

        // Swells that play backwards, like guitar tape flipped over: the
        // expression pedal is a volume pedal (swell up from the heel) into
        // a reverse echo, with a soft hall: a bowed, "violining" sound.
        make ("Reverse Tape Swell", G, 6, "Album,FX,Ambient,Expression", {
            { rigLevel, 2 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, reverseDelay }, { delaySync, quarter }, { delayFeedback, 0.4f }, { delayMix, 0.6f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.3f } }),

        // Guitar parts played like sampled hip-hop horns: QUASAR's brass
        // envelope (a low-pass that opens with each pick), a little grit.
        make ("Sampled Horn Stab", G, 7, "Album,FX,Filter", {
            { rigLevel, -3 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpBrassAttack }, { fpMix, 1 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveLevel, 2 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.3f }, { reverbMix, 0.1f } }),

        // An aggressive high note picked hard and shaken: boosted, the upper
        // mids pushed so it screams, a short slap behind it. The wiggle is
        // your fretting hand.
        make ("High Note Shriek", G, 8, "Album,Lead", {
            { rigLevel, -7 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 4 }, { driveTone, 8 }, { driveLevel, 5 },
            { eqOn, 1 }, { "eq250", -2 }, { "eq2k", 3 }, { "eq4k", 3 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, syncOff }, { delayTime, 110 }, { delayFeedback, 0.15f }, { delayMix, 0.15f } }),

        // Rigid, drum-machine industrial: every sixteenth chopped at its end
        // (on the song's grid when your DAW plays) and a metallic flange.
        make ("Machine Grid Chop", G, 9, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchKillswitch }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 4 }, { glitchChaos, 0 }, { glitchMix, 1 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.6f }, { modDepth, 0.6f }, { modManual, 6 }, { modRes, 7 } }),

        // A haunted, slow flange on a clean, in the spirit of Head's
        // Electric Mistress: resonant and drifting, in a dark room.
        make ("Haunted Flange Clean", G, 10, "Album,Clean,Modulation", {
            { rigLevel, 1 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.12f }, { modDepth, 0.8f }, { modManual, 4 }, { modRes, 7 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.45f }, { reverbTone, 4 }, { reverbMix, 0.2f } }),

        // A sludgy bending lead: a thick fuzz, a slow phaser and a dark echo.
        make ("Sludge Bend Lead", G, 11, "Album,Lead,Fuzz", {
            { driveOn, 1 }, { driveModel, cosmonaut }, { driveGain, 6 }, { driveTone, 3.5f }, { driveLevel, -2 },
            { modOn, 1 }, { modType, phaser }, { modRate, 0.3f }, { modDepth, 0.5f },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.25f }, { delayMix, 0.15f }, { delayTone, 4 } }),

        // A dread-filled throbbing clean: a slow, deep uni-vibe and a dark spring.
        make ("Dread Vibe Clean", G, 12, "Album,Clean,Modulation", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, uniVibe }, { modRate, 1.2f }, { modDepth, 0.8f }, { modMix, 0.5f },
            { reverbOn, 1 }, { reverbType, spring }, { reverbSize, 0.5f }, { reverbTone, 4 }, { reverbMix, 0.25f } }),

        // A loose, spiralling jam: a wide chorus into a long tape echo that
        // keeps on repeating, and a big hall: for drawn-out, improvised endings.
        make ("Spiral Jam Echo", G, 13, "Album,FX,Ambient,Delay", {
            { rigLevel, -3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.8f }, { modDepth, 0.6f }, { modMix, 0.4f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.7f }, { delayMix, 0.35f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.8f }, { reverbMix, 0.3f } }),

        // A drone to play with pipes: an octave below under your note,
        // doubled wide and sustained, for open, droning chords.
        make ("Pipe Drone Octave", G, 14, "Album,FX,Pitch,Sub", {
            { rigLevel, 2 },
            { octOn, 1 }, { octSubOne, 0.7f }, { octDry, 1 }, { octUp, 0 },
            { compOn, 1 }, { compThreshold, -30 }, { compRatio, 5 }, { compLevel, 3 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.5f }, { modMix, 0.5f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.2f } }),

        // A funky riff through a vintage four-stage phaser, the kind in a box
        // of '70s pedals, with a touch of grit and no space.
        make ("Funk Phase Riff", G, 15, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, phaser }, { modRate, 0.9f }, { modDepth, 0.7f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveLevel, 2 } }),

        //======================================================================
        // Bank H: an album's guitar sounds, inspired by Linkin Park's Hybrid
        // Theory (2000; the owner's third album bank). Cleans, effects and
        // leads: the drop-tuned rhythm is the amp capture's job. Documented
        // (Kerrang's oral history, Songfacts, Gearnews): the "Points of
        // Authority" riff was Brad Delson's, chopped and rearranged on the
        // computer by Mike Shinoda so much that Brad learnt his own part from
        // it; the "In the End" verses are Brad's harmonics; the heavy parts
        // are several takes panned left and right; Joe Hahn's turntables run
        // through the record ("Cure for the Itch" is his). The gear was a
        // PRS CE24 into a Dual Rectifier, a noise suppressor, probably a
        // compressor. The other rigs are our reading of the record's sounds.

        // The chopped riff: what you play cut into sixteenths and grabbed
        // back in stutters, as if rearranged on a computer (on the song's
        // grid when your DAW plays). Over your heavy amp.
        make ("Chopped Grid Riff", H, 1, "Album,FX,Glitch", {
            { rigLevel, -2 },
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 6 }, { glitchChaos, 1 }, { glitchMix, 1 } }),

        // The same idea, rougher: the riff broken up as if the signal kept
        // failing, pieces dropped out, reversed or caught in short loops.
        make ("Broken Signal Riff", H, 2, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchInterfere }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 4 }, { glitchMix, 1 } }),

        // Ringing harmonics for a verse: a firm, bright clean, a light chorus,
        // a dotted-eighth echo and a plate, so each chime hangs in the air.
        make ("Harmonic Chime Verse", H, 3, "Album,Clean,Delay", {
            { rigLevel, 7 },
            { compOn, 1 }, { compThreshold, -28 }, { compRatio, 4 }, { compLevel, 3 },
            { eqOn, 1 }, { "eq125", -3 }, { "eq4k", 2 }, { "eq8k", 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.6f }, { modDepth, 0.3f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // Answering the DJ: HAVOK's scratch programs on the six PROGRAM MAP
        // keys, the stab on the record, in time with the song. Touch the pad
        // (or a MIDI pad, CC 92) to scratch between your riffs.
        make ("Turntable Answer", H, 4, "Album,FX,Scratch,Havok,Touch", {
            { havokOn, 1 }, { havokProgram, scratchChirp }, { havokScratchSound, soundStab }, { havokX, rateSixteenth }, { havokY, 0.7f },
            { "havokMap1Program", scratchBaby }, { "havokMap2Program", scratchChirp }, { "havokMap3Program", scratchTransformer },
            { "havokMap4Program", scratchFlare }, { "havokMap5Program", scratchTear }, { "havokMap6Program", scratchCrab } }),

        // A drifting, dusty texture from your guitar: a cloud of grains,
        // through a warbling cassette, in a reverse-swelling space.
        make ("Needle Drift Texture", H, 5, "Album,FX,Ambient,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 5 }, { glitchChaos, 5 }, { glitchMix, 0.6f },
            { lofiOn, 1 }, { lofiType, cassette }, { lofiAmount, 5 }, { lofiNoise, 4 }, { lofiMix, 0.8f },
            { reverbOn, 1 }, { reverbType, reverse }, { reverbSize, 0.6f }, { reverbMix, 0.45f } }),

        // A hazy verse clean: a slow, wide chorus, a quarter-note echo and a
        // hall, both ducked so the picking stays clear.
        make ("Hazy Verse Clean", H, 6, "Album,Clean,Ambient", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.4f }, { modDepth, 0.5f }, { modMix, 0.4f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, quarter }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { delayDuck, 0.4f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.7f }, { reverbMix, 0.35f }, { reverbDuck, 0.3f } }),

        // A picked intro clean: close and present, a short slap and a small
        // room, before the band comes in.
        make ("Picked Intro Clean", H, 7, "Album,Clean", {
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 }, { compLevel, 2 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, syncOff }, { delayTime, 120 }, { delayFeedback, 0.15f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.35f }, { reverbMix, 0.18f } }),

        // A thin, far-away part, as if through a phone band, for a quiet
        // verse figure under the vocal.
        make ("Phone Band Riff", H, 8, "Album,FX,Lo-Fi", {
            { lofiOn, 1 }, { lofiType, telephone }, { lofiAmount, 6 }, { lofiNoise, 2 }, { lofiMix, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.2f }, { delayMix, 0.15f } }),

        // Takes stacked and spread: two extra "takes" either side of your
        // part, as the record layers its guitars left and right.
        make ("Wide Layered Takes", H, 9, "Album,FX,Modulation", {
            { rigLevel, -2 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.6f }, { modMix, 0.7f }, { modStereo, 1 } }),

        // A single-note line with an octave above it, boosted into your amp,
        // for melodic leads that cut through.
        make ("Octave Line Lead", H, 10, "Album,Lead,Pitch,Octave", {
            { rigLevel, -5 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.8f }, { octUp, 0.5f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f } }),

        // Soft swells: the expression pedal is a volume pedal (rock it up
        // under each note), into a long echo and a hall.
        make ("Soft Swell Pad", H, 11, "Album,Clean,Ambient,Expression", {
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // A melodic lead with a dotted-eighth echo ducking under it and a hall.
        make ("Dotted Echo Lead", H, 12, "Album,Lead,Delay", {
            { rigLevel, -6 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.4f }, { delayMix, 0.3f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.2f } }),

        // A crushed, sampled-sounding hook: your guitar through a bitcrusher,
        // as if lifted from a record.
        make ("Bitcrushed Hook", H, 13, "Album,FX,Lo-Fi", {
            { lofiOn, 1 }, { lofiType, bitcrusher }, { lofiAmount, 9 }, { lofiNoise, 1 }, { lofiMix, 1 },
            { eqOn, 1 }, { "eq63", -4 }, { "eq8k", -3 } }),

        // A backwards riser for transitions: every note echoes back in
        // reverse, swelling into the next part, in a hall.
        make ("Reverse Riser", H, 14, "Album,FX,Ambient,Delay", {
            { rigLevel, -6 },
            { delayOn, 1 }, { delayType, reverseDelay }, { delaySync, quarter }, { delayFeedback, 0.6f }, { delayMix, 0.6f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // A filtered, sample-like riff: an envelope filter opening with each
        // pick, so the part sounds processed rather than played.
        make ("Filter Sweep Riff", H, 15, "Album,FX,Filter", {
            { rigLevel, -4 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpEnvFilter }, { fpMix, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, sixteenth }, { delayFeedback, 0.2f }, { delayMix, 0.12f } }),

        //======================================================================
        // Bank I: an album's guitar sounds, inspired by Periphery's
        // Periphery III: Select Difficulty (2016; the owner's fourth album
        // bank). Cleans, effects and leads: the extended-range rhythm is the
        // amp capture's job. Documented: three guitarists writing three parts,
        // two double-tracked guitars left and right with the centre one on a
        // lead or counterpoint (Misha Mansoor to Jackson Guitars); recorded
        // through Fractal Axe-Fx units; Misha's pedals on the album included
        // a Free The Tone Flight Time delay (precise, tempo-locked) and a
        // noise gate; orchestral moments; "Lune" ends on thick synth layers
        // over a spacious groove (reviews). The rest is our reading.

        // The centre guitar's lead between the two rhythm parts: gated tight,
        // boosted into your amp, a tempo-locked dotted-eighth echo and a plate.
        make ("Center Counterpoint Lead", I, 1, "Album,Lead,Delay", {
            { rigLevel, -7 },
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -62 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.18f } }),

        // A crisp, tempo-locked echo on a compressed clean: eighth notes,
        // precise and bright, as a digital delay set to the song's tempo.
        make ("Locked Echo Clean", I, 2, "Album,Clean,Delay", {
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { delayTone, 8 },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.22f } }),

        // Your part as two double-tracked guitars, one each side.
        make ("Twin Takes Spread", I, 3, "Album,FX,Modulation", {
            { rigLevel, -3 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.5f }, { modMix, 0.7f }, { modStereo, 1 } }),

        // Glassy clean arpeggios: compressed, bright, a light chorus, a
        // dotted-eighth echo and a plate.
        make ("Glassy Arpeggio Clean", I, 4, "Album,Clean,Delay", {
            { rigLevel, 10 },
            { compOn, 1 }, { compThreshold, -28 }, { compRatio, 4 }, { compLevel, 3 },
            { eqOn, 1 }, { "eq125", -2 }, { "eq4k", 2 }, { "eq8k", 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.6f }, { modDepth, 0.35f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.22f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.55f }, { reverbMix, 0.22f } }),

        // Swells that sit with a string section: the pedal is a volume pedal,
        // a wide dual chorus and a big hall.
        make ("String Section Swell", I, 5, "Album,Clean,Ambient,Expression", {
            { rigLevel, 3 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { modOn, 1 }, { modType, dimension }, { modDepth, 0.7f }, { modMix, 0.5f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // The guitar as a synth layer: an octave above, smeared into grains,
        // a rising shimmer and a cathedral.
        make ("Synth Layer Pad", I, 6, "Album,FX,Ambient,Pitch", {
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.8f }, { octUp, 0.4f },
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 7 }, { glitchChaos, 2 }, { glitchMix, 0.5f },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // A fluid legato lead: squeezed for sustain, boosted, a quarter-note
        // echo and a hall.
        make ("Legato Echo Lead", I, 7, "Album,Lead,Delay", {
            { rigLevel, -4 },
            { compOn, 1 }, { compThreshold, -30 }, { compRatio, 5 }, { compLevel, 4 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, quarter }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.2f } }),

        // A lead with an octave above gliding with it, boosted into your amp.
        make ("Octave Glide Lead", I, 8, "Album,Lead,Pitch,Octave", {
            { rigLevel, -8 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.8f }, { octUp, 0.55f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 7 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.18f } }),

        // Stop-start stutters: what you play grabbed back in sixteenths (on
        // the song's grid when your DAW plays), over your heavy amp.
        make ("Stop-Start Chop", I, 9, "Album,FX,Glitch", {
            { rigLevel, -3 },
            { compOn, 1 }, { compThreshold, 0 }, { gateThreshold, -60 },
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 2 }, { glitchMix, 1 } }),

        // A bright, poppy chorus clean: a lush stereo chorus, an eighth echo
        // and a medium hall.
        make ("Bright Chorus Clean", I, 10, "Album,Clean,Modulation", {
            { rigLevel, 6 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.8f }, { modDepth, 0.5f }, { modMix, 0.45f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.55f }, { reverbMix, 0.22f } }),

        // A lead that blooms into space: boosted, a long dotted-quarter echo
        // and a cathedral that swell in the gaps.
        make ("Bloom Lead", I, 11, "Album,Lead,Ambient,Delay", {
            { rigLevel, -8 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, 4 }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { delayDuck, 0.6f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.3f }, { reverbDuck, 0.4f } }),

        // A flanged clean for a bridge: a slow, wide flange and a room.
        make ("Flanged Bridge Clean", I, 12, "Album,Clean,Modulation", {
            { rigLevel, 2 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.2f }, { modDepth, 0.7f }, { modManual, 5 }, { modRes, 5 }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.45f }, { reverbMix, 0.2f } }),

        // An ending that drifts away: every note swells in backwards from a
        // reverse reverb, with a rising shimmer behind.
        make ("Reverse Shimmer Outro", I, 13, "Album,FX,Ambient", {
            { rigLevel, -4 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.6f }, { delayMix, 0.35f },
            { reverbOn, 1 }, { reverbType, reverse }, { reverbSize, 0.7f }, { reverbMix, 0.5f }, { reverbPreDelay, 0 } }),

        // A dive-bomb accent: QUASAR's six-octave dive on the expression
        // pedal (heel: your note, toe: down to a rumble), over your heavy amp.
        make ("Dive Accent", I, 14, "Album,Lead,Pitch,Expression", {
            { rigLevel, -2 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpDiveBomb }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah } }),

        // A sparkling, compressed clean: squeezed even, the top lifted, a
        // little chorus and a small plate.
        make ("Sparkle Compressed Clean", I, 15, "Album,Clean", {
            { rigLevel, 8 },
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 6 }, { compDetail, 5 }, { compMix, 0.8f },
            { eqOn, 1 }, { "eq125", -2 }, { "eq2k", 1 }, { "eq8k", 3 }, { "eq16k", 2 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.5f }, { modDepth, 0.25f }, { modMix, 0.25f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.35f }, { reverbMix, 0.15f } }),

        //======================================================================
        // Bank J: an album's guitar sounds, inspired by Sevendust's Next
        // (2005; the owner's fifth album bank, and PR7-V's bank Q). Cleans,
        // effects and leads: the drop-tuned rhythm is the amp capture's job.
        // Documented: John Connolly on lead (and producing) and Sonny Mayo
        // on rhythm, the first album after Clint Lowery left; drop tunings;
        // reviewers note electronic effects on "Never". Little more is
        // published about the guitars, so this bank is mostly our reading.
        // J04 answers, on guitar, the delayed scream on "Silence" the owner
        // asked PR7-V to nail.

        // A soaring lead: boosted into your amp, a dotted-eighth echo that
        // ducks under the notes, a hall.
        make ("Champion Lead", J, 1, "Album,Lead,Delay", {
            { rigLevel, -7 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.2f } }),

        // Clean arpeggios: compressed, a gentle chorus, an eighth echo, a plate.
        make ("Fragment Arpeggio Clean", J, 2, "Album,Clean,Delay", {
            { rigLevel, 6 },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.6f }, { modDepth, 0.35f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // A wave of broken noise for accents: your riff breaking up, pieces
        // dropping out or caught in short loops, over your heavy amp.
        make ("Scar Noise Accent", J, 3, "Album,FX,Glitch", {
            { rigLevel, -2 },
            { glitchOn, 1 }, { glitchMode, glitchInterfere }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 6 }, { glitchChaos, 5 }, { glitchMix, 1 } }),

        // A note thrown into echoes that keep going after it stops: a big
        // quarter-note throw, ducked while you play, thinned and roughened.
        make ("Echo Throw Accent", J, 4, "Album,FX,Ambient,Delay", {
            { rigLevel, -4 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveLevel, 3 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, quarter }, { delayFeedback, 0.55f }, { delayMix, 0.5f },
            { delayTone, 5 }, { delayDuck, 0.3f } }),

        // A soft, close verse clean: a faint chorus and a plate.
        make ("Hushed Verse Clean", J, 5, "Album,Clean", {
            { rigLevel, 5 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.4f }, { modDepth, 0.25f }, { modMix, 0.25f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.45f }, { reverbMix, 0.2f } }),

        // Lonely swells: the expression pedal is a volume pedal (swell up from
        // the heel), into a tape echo and a big hall.
        make ("Abandoned Swell", J, 6, "Album,Clean,Ambient,Expression", {
            { rigLevel, 2 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.45f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // An electronic edge: a cloud of grains through a radio band.
        make ("Wired Texture", J, 7, "Album,FX,Glitch", {
            { rigLevel, 7 },
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 6 }, { glitchChaos, 3 }, { glitchMix, 0.5f },
            { lofiOn, 1 }, { lofiType, amRadio }, { lofiAmount, 5 }, { lofiNoise, 2 }, { lofiMix, 0.6f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.15f } }),

        // A dark, phased clean: a slow phaser and a dark room.
        make ("Crimson Phaser Clean", J, 8, "Album,Clean,Modulation", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, phaser }, { modRate, 0.3f }, { modDepth, 0.6f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.45f }, { reverbTone, 4 }, { reverbMix, 0.2f } }),

        // A lead with an octave above it, boosted into your amp.
        make ("Fallen Octave Lead", J, 9, "Album,Lead,Pitch,Octave", {
            { rigLevel, -8 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.8f }, { octUp, 0.55f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f } }),

        // A big chorus clean: a wide dual chorus, a quarter echo and a hall.
        make ("Faith Chorus Clean", J, 10, "Album,Clean,Modulation", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, dimension }, { modDepth, 0.7f }, { modMix, 0.5f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, quarter }, { delayFeedback, 0.3f }, { delayMix, 0.18f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.7f }, { reverbMix, 0.28f } }),

        // A singing lead, soulful and warm: boosted, a slap and a plate.
        make ("Living Soul Lead", J, 11, "Album,Lead", {
            { rigLevel, -10 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 5 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, syncOff }, { delayTime, 140 }, { delayFeedback, 0.2f }, { delayMix, 0.18f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.55f }, { reverbMix, 0.2f } }),

        // A soft farewell clean: a slow tape echo and a long hall.
        make ("Farewell Clean", J, 12, "Album,Clean,Ambient", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.45f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.35f } }),

        // Twin guitars in harmony: a fifth above every note, mixed in under
        // your line, boosted into your amp.
        make ("Fifth Harmony Lead", J, 13, "Album,Lead,Pitch", {
            { rigLevel, -6 },
            { fpOn, 1 }, { fpType, pitchShifter }, { fpInterval, 7 }, { fpMix, 0.4f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 6 }, { driveLevel, 4 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.25f }, { delayMix, 0.15f } }),

        // Your part as two guitars, one each side.
        make ("Wall Takes Spread", J, 14, "Album,FX,Modulation", {
            { rigLevel, -3 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.5f }, { modMix, 0.7f }, { modStereo, 1 } }),

        // A soulful wah lead: the expression pedal is a wah, boosted, a plate.
        make ("Soul Wah Lead", J, 15, "Album,Lead,Wah,Expression", {
            { rigLevel, -5 },
            { expressionTarget, pedalWah },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 4 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.18f } }),

        //======================================================================
        // Bank K: an album's guitar sounds, inspired by Tool's Aenima (1996;
        // the owner's sixth album bank). Cleans, effects and leads: the
        // rhythm is the amp capture's job. Documented: on this album Adam
        // Jones (with Justin Chancellor) turned to effects pedals for
        // variation, "Forty Six & 2" and "Pushit" the clearest examples
        // (Andertons); his board from then lists a Boss DD-3 delay and BF-2
        // flanger (Equipboard, from his tech's board video); a mid-'70s
        // Marshall Super Bass was the sole amp through this album. Forum
        // quotes of an old interview, not verified: an Epilady shaver on the
        // strings for the hum at the start of "Pushit", an EBow at the start
        // of "Third Eye". The rest is our reading.

        // A clean through a slow flanger into a digital delay, as a compact
        // flanger and delay on a board.
        make ("Flanged Delay Clean", K, 1, "Album,Clean,Modulation,Delay", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, flanger }, { modRate, 0.15f }, { modDepth, 0.6f }, { modManual, 5 }, { modRes, 5 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.35f }, { delayMix, 0.22f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.12f } }),

        // A riff swirling through a deeper flange, over your amp.
        make ("Swirl Flange Riff", K, 2, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, flanger }, { modRate, 0.25f }, { modDepth, 0.8f }, { modManual, 4 }, { modRes, 7 } }),

        // A clean with a quarter-note digital echo and a little room.
        make ("Clean Repeat Echo", K, 3, "Album,Clean,Delay", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, quarter }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.12f } }),

        // A buzzing hum, as if a shaver were held to the strings: HAVOK's
        // ring modulator held on at a low, buzzy setting (X tunes the buzz).
        make ("Shaver Buzz Drone", K, 4, "Album,FX,Havok", {
            { havokOn, 1 }, { havokProgram, havokRingFilter }, { havokX, 0.15f }, { havokY, 0.5f }, { havokHold, 1 } }),

        // A bowed, endless note: no pick attack (swell it with the pedal,
        // a volume pedal), squeezed to sustain, into a long echo and a hall.
        make ("Bowed Sustain Swell", K, 5, "Album,Clean,Ambient,Expression", {
            { rigLevel, 1 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -34 }, { compRatio, 8 }, { compLevel, 5 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // A long, hazy clean for slow-building sections: a dotted echo and
        // a cathedral, both ducked.
        make ("Long Haze Clean", K, 6, "Album,Clean,Ambient", {
            { rigLevel, 2 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { delayDuck, 0.4f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.35f }, { reverbDuck, 0.3f } }),

        // A biting lead with grit, boosted into your amp, a slap and a room.
        make ("Bass Head Lead", K, 7, "Album,Lead", {
            { rigLevel, -9 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 5 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, syncOff }, { delayTime, 130 }, { delayFeedback, 0.2f }, { delayMix, 0.15f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.12f } }),

        // A tense, dry verse clean: firm compression, a short slap.
        make ("Tense Verse Clean", K, 8, "Album,Clean", {
            { rigLevel, 3 },
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 6 }, { compDetail, 5 }, { compMix, 1 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, syncOff }, { delayTime, 90 }, { delayFeedback, 0.1f }, { delayMix, 0.12f } }),

        // Clean arpeggios: a gentle chorus, an eighth echo, a plate.
        make ("Arpeggio Chorus Clean", K, 9, "Album,Clean,Modulation", {
            { rigLevel, 7 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.5f }, { modDepth, 0.35f }, { modMix, 0.3f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.18f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // A wash of rumbling noise: grains smeared from what you play,
        // through a noisy radio, in a big space.
        make ("Rumble Noise Wash", K, 10, "Album,FX,Ambient,Glitch", {
            { rigLevel, 4 },
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 8 }, { glitchChaos, 7 }, { glitchMix, 0.7f },
            { lofiOn, 1 }, { lofiType, amRadio }, { lofiAmount, 6 }, { lofiNoise, 6 }, { lofiMix, 0.8f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.45f } }),

        // A crackling phone line: your guitar thin and noisy, as on an
        // answering machine.
        make ("Crackle Phone Line", K, 11, "Album,FX,Lo-Fi", {
            { lofiOn, 1 }, { lofiType, telephone }, { lofiAmount, 7 }, { lofiNoise, 5 }, { lofiMix, 1 } }),

        // A skipping record: eighth-note slices of what you play caught and
        // repeated unevenly (on the song's grid when your DAW plays).
        make ("Skipping Record", K, 12, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchEighth },
            { glitchDensity, 7 }, { glitchChaos, 6 }, { glitchMix, 1 },
            { lofiOn, 1 }, { lofiType, cassette }, { lofiAmount, 4 }, { lofiNoise, 3 }, { lofiMix, 0.6f } }),

        // A heavy, doomy fuzz lead: a thick fuzz and a dark echo.
        make ("Doom Fuzz Lead", K, 13, "Album,Lead,Fuzz", {
            { rigLevel, -4 },
            { driveOn, 1 }, { driveModel, cosmonaut }, { driveGain, 7 }, { driveTone, 4 }, { driveLevel, -2 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.3f }, { delayMix, 0.18f }, { delayTone, 4 } }),

        // A wheezing waltz: an octave below under your note through a slow
        // rotating speaker, like a fairground organ.
        make ("Rotating Waltz Clean", K, 14, "Album,Clean,Pitch,Sub", {
            { rigLevel, 1 },
            { octOn, 1 }, { octSubOne, 0.5f }, { octDry, 1 }, { octUp, 0 },
            { modOn, 1 }, { modType, rotary }, { modCycFast, 0 }, { modCycSlowSpeed, 0.8f }, { modCycLow, 4 }, { modCycHigh, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // A marching chop: a hard tremolo cutting every sixteenth, over your
        // heavy amp, in time with the song.
        make ("March Tremolo Chop", K, 15, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, tremolo }, { modSync, sixteenth }, { modDepth, 1 } }),

        //======================================================================
        // Bank L: an album's guitar sounds, inspired by Rage Against the
        // Machine's Evil Empire (1996; the owner's seventh album bank).
        // Effects and leads above all, a clean: the rhythm is the amp
        // capture's job. Documented (Morello's own words to Guitarist via
        // MusicRadar, Premier Guitar, Guitar.com, gear guides): the same few
        // pedals from the start, a wah, a Whammy, a delay (a Boss DD-2) and a
        // phaser, plus an EQ pedal as a solo boost and an Ibanez flanger; the
        // "Year of tha Boomerang" lead is heavily multi-tracked Whammy parts
        // two octaves up; the "Bulls on Parade" solo is a scratch made with
        // the pickup selector as a kill switch while rubbing the strings, the
        // wah parked fully down; "People of the Sun" is built on a repeated
        // scratch across a power chord; the Whammy makes the squeals and
        // octave harmonies. The rest is our reading. (Bank E's Wah Scratch
        // Chop is the pedal-wah version of the scratch.)

        // The two-octave lead: your line two octaves up, doubled wide as if
        // multi-tracked, boosted into your amp.
        make ("Two-Octave Stack Lead", L, 1, "Album,Lead,Pitch", {
            { rigLevel, -6 },
            { fpOn, 1 }, { fpType, pitchShifter }, { fpInterval, 24 }, { fpMix, 1 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.5f }, { modMix, 0.6f }, { modStereo, 1 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveLevel, 4 } }),

        // The scratch: the wah parked fully down (a fixed, nasal peak) and
        // the end of every sixteenth cut, as if flicking the pickup selector
        // (on the song's grid when your DAW plays). Rub the muted strings.
        make ("Parked Wah Scratch", L, 2, "Album,FX,Glitch,Wah", {
            { glitchOn, 1 }, { glitchMode, glitchKillswitch }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 0 }, { glitchMix, 1 },
            { eqOn, 1 }, { "eq63", -10 }, { "eq125", -8 }, { "eq250", -4 }, { "eq1k", 4 }, { "eq2k", 8 }, { "eq4k", 2 }, { "eq8k", -6 }, { "eq16k", -10 } }),

        // A two-octave squeal on the expression pedal: heel your note, toe
        // two octaves up (rock to the heel before you play).
        make ("Two-Octave Pedal Shriek", L, 3, "Album,Lead,Pitch,Expression", {
            { rigLevel, -2 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpTwoOctaves }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah } }),

        // An octave harmony: an octave above and below your riff at once.
        make ("Octave Harmony Riff", L, 4, "Album,FX,Pitch,Octave", {
            { rigLevel, -3 },
            { octOn, 1 }, { octSubOne, 0.4f }, { octDry, 0.9f }, { octUp, 0.6f } }),

        // The kill-switch stutter alone: the end of every sixteenth cut, as
        // if flicking a pickup selector off and on, in time with the song.
        make ("Toggle Kill Stutter", L, 5, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchKillswitch }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 6 }, { glitchChaos, 0 }, { glitchMix, 1 } }),

        // A solo lifted by an EQ pedal: the mids pushed and the level up,
        // into your amp.
        make ("Graphic Boost Lead", L, 6, "Album,Lead", {
            { rigLevel, -6 },
            { eqOn, 1 }, { "eq250", 1 }, { "eq500", 3 }, { "eq1k", 5 }, { "eq2k", 3 }, { eqLevel, 4 } }),

        // A riff through a slow phaser.
        make ("Slow Phase Riff", L, 7, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, phaser }, { modRate, 0.4f }, { modDepth, 0.7f } }),

        // A jet flange on a riff, in the spirit of a '80s Ibanez flanger.
        make ("Jet Flange Riff", L, 8, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, flanger }, { modRate, 0.3f }, { modDepth, 0.8f }, { modManual, 6 }, { modRes, 8 } }),

        // A delay pushed to the edge: short repeats fed back until they
        // run away, for noises and swirls between riffs.
        make ("Runaway Echo Swirl", L, 9, "Album,FX,Ambient,Delay", {
            { rigLevel, -3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, sixteenth }, { delayFeedback, 0.88f }, { delayMix, 0.4f } }),

        // A scratched chord with a short slap, like a picked stab doubled
        // right behind itself.
        make ("Scratched Chord Slap", L, 10, "Album,FX,Delay", {
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, syncOff }, { delayTime, 45 }, { delayFeedback, 0.15f }, { delayMix, 0.45f } }),

        // A wah lead on the expression pedal, boosted into your amp.
        make ("Wah Sweep Lead", L, 11, "Album,Lead,Wah,Expression", {
            { rigLevel, -6 },
            { expressionTarget, pedalWah },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveLevel, 4 } }),

        // A siren dive on the pedal: heel your note, toe two octaves down.
        make ("Siren Dive", L, 12, "Album,Lead,Pitch,Expression", {
            { rigLevel, -2 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpDownTwoOctaves }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah } }),

        // A whole-step bend on the pedal: heel your note, toe a tone up, for
        // bends no string could make.
        make ("Whole-Step Pedal Bend", L, 13, "Album,Lead,Pitch,Expression", {
            { rigLevel, -2 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpSecond }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah } }),

        // A dry, muted clean pulse, firm and close.
        make ("Muted Clean Pulse", L, 14, "Album,Clean,Dry", {
            { rigLevel, 3 },
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 6 }, { compDetail, 5 }, { compMix, 1 },
            { eqOn, 1 }, { "eq63", -3 }, { "eq2k", 2 } }),

        // Your guitar as if through a talk-radio broadcast, with static.
        make ("Radio Static Riff", L, 15, "Album,FX,Lo-Fi", {
            { lofiOn, 1 }, { lofiType, amRadio }, { lofiAmount, 7 }, { lofiNoise, 5 }, { lofiMix, 1 } }),

        //======================================================================
        // Bank M: an album's guitar sounds, inspired by Cold's 13 Ways to
        // Bleed on Stage (2000; the owner's eighth album bank). Cleans,
        // effects and leads: the drop-tuned rhythm is the amp capture's job.
        // Documented (credits, reviews): Kelly Hayes on guitar with Terry
        // Balsamo brought in for this album, Scooter Ward giving up guitar
        // for vocals, keyboards and piano; Chris Vrenna among those credited;
        // reviewers hear it as cleaner-sounding than the debut. Little is
        // published about the guitar sounds, so this bank is mostly our
        // reading.

        // A haunted clean arpeggio: a slow, wide chorus, an eighth echo and
        // a dark plate.
        make ("Haunted Arpeggio Clean", M, 1, "Album,Clean,Modulation", {
            { rigLevel, 4 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.4f }, { modDepth, 0.5f }, { modMix, 0.4f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.2f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.55f }, { reverbTone, 4 }, { reverbMix, 0.22f } }),

        // A dark, lush chorus clean for a verse, and a hall.
        make ("Dark Chorus Clean", M, 2, "Album,Clean,Modulation", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, dimension }, { modDepth, 0.6f }, { modMix, 0.5f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbTone, 4 }, { reverbMix, 0.25f } }),

        // A sparse, dry verse clean: firm and close, nothing after the note.
        make ("Sparse Verse Clean", M, 3, "Album,Clean,Dry", {
            { rigLevel, 3 },
            { compOn, 1 }, { compType, daybreakComp }, { compAmount, 5 }, { compDetail, 5 }, { compMix, 1 },
            { eqOn, 1 }, { "eq63", -3 }, { "eq4k", 1 } }),

        // A doomy lead: boosted, a slow phaser and a dark echo.
        make ("Doom Phase Lead", M, 4, "Album,Lead,Modulation", {
            { rigLevel, -4 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 5 }, { driveLevel, 5 },
            { modOn, 1 }, { modType, phaser }, { modRate, 0.25f }, { modDepth, 0.6f },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.3f }, { delayMix, 0.18f }, { delayTone, 4 } }),

        // A whispered, trembling clean: a slow, soft tremolo and a plate.
        make ("Whisper Tremolo Clean", M, 5, "Album,Clean,Modulation", {
            { rigLevel, 4 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, tremolo }, { modSync, eighth }, { modDepth, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.22f } }),

        // A warped, tape-worn clean: through a wobbling cassette, a room.
        make ("Warped Tape Clean", M, 6, "Album,Clean,Lo-Fi", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { lofiOn, 1 }, { lofiType, cassette }, { lofiAmount, 5 }, { lofiNoise, 2 }, { lofiMix, 0.7f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.15f } }),

        // A spinning, circus-organ clean: an octave above your note through
        // a fast rotating speaker.
        make ("Spinning Calliope Clean", M, 7, "Album,Clean,Pitch,Octave", {
            { rigLevel, 1 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.9f }, { octUp, 0.6f },
            { modOn, 1 }, { modType, rotary }, { modCycFast, 1 }, { modCycLow, 4 }, { modCycHigh, 4 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.45f }, { reverbMix, 0.18f } }),

        // A drowsy haze: a slow chorus and a reverse reverb that swells in
        // behind each note.
        make ("Narcotic Haze", M, 8, "Album,Clean,Ambient", {
            { rigLevel, 6 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.3f }, { modDepth, 0.6f }, { modMix, 0.4f }, { modStereo, 1 },
            { reverbOn, 1 }, { reverbType, reverse }, { reverbSize, 0.6f }, { reverbMix, 0.45f } }),

        // A bitter, singing lead: boosted, a dotted-eighth echo, a plate.
        make ("Bitter Lead", M, 9, "Album,Lead,Delay", {
            { rigLevel, -7 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.35f }, { delayMix, 0.22f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.18f } }),

        // A hexed riff: a deep, resonant flange over your amp.
        make ("Hex Flange Riff", M, 10, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, flanger }, { modRate, 0.35f }, { modDepth, 0.8f }, { modManual, 5 }, { modRes, 8 } }),

        // Industrial grit: stutters on the song's grid and a bitcrushed edge,
        // as programming might cut up a guitar.
        make ("Industrial Grit Chop", M, 11, "Album,FX,Glitch,Lo-Fi", {
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 5 }, { glitchChaos, 3 }, { glitchMix, 1 },
            { lofiOn, 1 }, { lofiType, bitcrusher }, { lofiAmount, 7 }, { lofiNoise, 1 }, { lofiMix, 0.5f } }),

        // A spacey pad: a rising shimmer and a long tape echo in a big space.
        make ("Space Echo Pad", M, 12, "Album,FX,Ambient,Delay", {
            { rigLevel, -3 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.6f }, { delayMix, 0.35f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // Wounded swells: the pedal is a volume pedal (swell from the heel),
        // into a tape echo and a big hall.
        make ("Wounded Swell", M, 13, "Album,Clean,Ambient,Expression", {
            { rigLevel, 2 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.45f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.85f }, { reverbMix, 0.4f } }),

        // A bell-like clean to sit with piano: an octave above, bright and
        // even, a plate.
        make ("Bell Tone Clean", M, 14, "Album,Clean,Pitch,Octave", {
            { rigLevel, 3 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.9f }, { octUp, 0.5f },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 4 }, { compLevel, 2 },
            { eqOn, 1 }, { "eq4k", 2 }, { "eq8k", 2 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.22f } }),

        // Your part as two guitars, one each side.
        make ("Twin Guitar Spread", M, 15, "Album,FX,Modulation", {
            { rigLevel, -3 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.5f }, { modMix, 0.7f }, { modStereo, 1 } }),

        //======================================================================
        // Bank N: an album's guitar sounds, inspired by 30 Seconds to Mars'
        // debut, 30 Seconds to Mars (2002; the owner's ninth album bank).
        // Cleans, effects and leads: the heavy riffs are the amp capture's
        // job. Documented (credits, reviews): produced by Bob Ezrin and Brian
        // Virtue with the band, recorded in rural Wyoming; a space-themed
        // concept record, heavy riffs under pounding synths; Jared Leto on
        // guitar and synth, Solon Bixler on extra guitar; Ezrin's piano on two
        // songs; one review: Tool's take on The Dark Side of the Moon. Nothing
        // is published on the guitar effects, so the sounds are our reading.

        // A riff chopped into a synth-like pulse: a deep sixteenth tremolo,
        // a wide dimension.
        make ("Pulsing Synth Riff", N, 1, "Album,FX,Modulation", {
            { rigLevel, 1 },
            { modOn, 1 }, { modType, tremolo }, { modSync, sixteenth }, { modDepth, 0.9f },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.15f } }),

        // A clean that reaches out to the horizon: a dotted-eighth echo, wide,
        // a hall.
        make ("Horizon Delay Clean", N, 2, "Album,Clean,Delay", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.4f }, { delayMix, 0.28f },
            { delayPingPong, 1 },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.22f } }),

        // A huge, planet-sized lead: an octave below and above your note,
        // boosted, a long echo.
        make ("Planetary Drone Lead", N, 3, "Album,Lead,Pitch,Octave,Sub", {
            { rigLevel, -5 },
            { octOn, 1 }, { octSubOne, 0.5f }, { octDry, 0.9f }, { octUp, 0.5f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 5 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, quarter }, { delayFeedback, 0.35f }, { delayMix, 0.2f } }),

        // A countdown riff: a slow, deep phaser over your amp.
        make ("Countdown Phase Riff", N, 4, "Album,FX,Modulation", {
            { rigLevel, 3 },
            { modOn, 1 }, { modType, phaser }, { modRate, 0.2f }, { modDepth, 0.8f } }),

        // A dark ritual: a slow, deep, resonant flange and a dark room.
        make ("Dark Ritual Flange", N, 5, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, flanger }, { modRate, 0.15f }, { modDepth, 0.9f }, { modManual, 3 }, { modRes, 7 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.5f }, { reverbTone, 3 }, { reverbMix, 0.2f } }),

        // A reflective clean: a shimmer rising out of each note, a hall.
        make ("Reflective Shimmer Clean", N, 6, "Album,Clean,Ambient,Delay", {
            { rigLevel, 2 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.55f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.8f }, { reverbMix, 0.35f } }),

        // An orbiting clean: your note circling slowly from side to side,
        // a plate.
        make ("Orbit Pan Clean", N, 7, "Album,Clean,Modulation", {
            { rigLevel, 8 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, autoPan }, { modSync, wholeNote }, { modDepth, 0.8f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        // A clean to sit with a piano: an octave above, soft, a warm plate.
        make ("Piano Partner Clean", N, 8, "Album,Clean,Pitch,Octave", {
            { rigLevel, 3 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.9f }, { octUp, 0.45f },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { eqOn, 1 }, { "eq250", 1 }, { "eq8k", -2 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.55f }, { reverbTone, 4 }, { reverbMix, 0.25f } }),

        // A starfield: volume swells (swell from the heel) into a long echo
        // and a cathedral.
        make ("Starfield Swell", N, 9, "Album,Clean,Ambient,Expression", {
            { rigLevel, 2 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, quarter }, { delayFeedback, 0.5f }, { delayMix, 0.3f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.9f }, { reverbMix, 0.45f } }),

        // A lead that travels: boosted, a long tape echo that keeps going.
        make ("Long-Haul Echo Lead", N, 10, "Album,Lead,Delay", {
            { rigLevel, -6 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, tapeDelay }, { delaySync, quarter }, { delayFeedback, 0.55f }, { delayMix, 0.25f },
            { delayDuck, 0.5f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.15f } }),

        // Thrust: your riff as two guitars, one each side, a wall of them.
        make ("Wide Thrust Riff", N, 11, "Album,FX,Modulation", {
            { rigLevel, -3 },
            { modOn, 1 }, { modType, doubler }, { modDepth, 0.6f }, { modMix, 0.8f }, { modStereo, 1 } }),

        // A robot choir: the talk box shaping your lead into vowels (the pedal
        // opens the mouth), boosted, a plate.
        make ("Robot Choir Lead", N, 12, "Album,Lead,Talk Box,Expression", {
            { rigLevel, 2 },
            { fpOn, 1 }, { fpType, talkBoxType }, { fpResonance, 6 }, { fpMix, 1 },
            { expressionTarget, pedalTalkBox },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // A transmission from far away: your guitar through a radio, with
        // static and an echo.
        make ("Distant Transmission", N, 13, "Album,FX,Lo-Fi", {
            { lofiOn, 1 }, { lofiType, amRadio }, { lofiAmount, 6 }, { lofiNoise, 4 }, { lofiMix, 1 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, eighth }, { delayFeedback, 0.35f }, { delayMix, 0.25f } }),

        // A synth-like lead: an envelope filter that opens with how hard you
        // pick, a dark echo.
        make ("Envelope Synth Lead", N, 14, "Album,Lead,Filter", {
            { rigLevel, 6 },
            { fpOn, 1 }, { fpType, autoWah }, { fpSensitivity, 7 }, { fpResonance, 7 }, { fpMix, 1 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 2 }, { driveTone, 5 }, { driveLevel, 5 },
            { delayOn, 1 }, { delayType, analogDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.15f } }),

        // Weightless: your notes scattered into floating fragments, in a big
        // space.
        make ("Weightless Texture", N, 15, "Album,FX,Ambient,Glitch", {
            { rigLevel, 1 },
            { glitchOn, 1 }, { glitchMode, glitchTexturizer }, { glitchDensity, 6 }, { glitchChaos, 4 }, { glitchMix, 0.6f },
            { reverbOn, 1 }, { reverbType, cathedral }, { reverbSize, 0.85f }, { reverbMix, 0.45f } }),

        //======================================================================
        // Bank O: CIPHER (0.76.0), one idea each from its manual: a filter
        // pulsing in time, a pick-triggered wah, the sequencer chopping,
        // formants, tape tricks, beat repeats, a snapped arpeggio, spectral
        // pads, the vocoder, ring mod and comb sweeps, a wobble, a radio sweep
        // and a filtered ping-pong. Our own presets (the original's 600 are its
        // makers'); each keeps your amp and cab.

        // A resonant 101 filter pulsing in eighths; MACRO 1 (FILTER, also on the
        // expression pedal) opens it.
        withCipher (make ("Filter Pulse", O, 1, "Cipher,FX,Filter,Expression", {
            { cipherOn, 1 }, { expressionTarget, pedalCipherMacro }, { rigLevel, 2 },
            { "cipher1Type", 7 }, { "cipher1Main", 0.42f }, { "cipher1P1", 0.65f }, { "cipher1P2", 0.3f },
            { "cipher1EnvAmount", 0.5f }, { "cipher1Speed", 11 }, { "cipher1Glide", 20 } }),
            { { 1, 5 } }, {}, "1:cipher1Main:0.35:0", "FILTER"),

        // A wah that each pick sets off: the envelope plucks the filter up and lets it fall.
        withCipher (make ("Pick Wah", O, 2, "Cipher,FX,Filter", {
            { cipherOn, 1 }, { rigLevel, 3 },
            { "cipher1Type", 7 }, { "cipher1Main", 0.38f }, { "cipher1P1", 0.7f }, { "cipher1P2", 0.2f },
            { "cipher1EnvAmount", 0.55f }, { "cipher1Style", 1 }, { "cipher1Speed", 8 },
            { "cipher1RetrigAudio", 1 }, { "cipher1EnvLoop", 0 }, { cipherTrigLevel, -30 } }),
            { { 1, 7 } }),

        // The sequencer chops the sound in a sixteenth pattern (a silent Gain slot on its lit steps).
        withCipher (make ("Stutter Gate", O, 3, "Cipher,FX,Glitch", {
            { cipherOn, 1 }, { cipherSeqOn, 1 }, { cipherSeqSpeed, 2 }, { rigLevel, 3 },
            { "cipher1Type", 19 }, { "cipher1Main", 0.0f }, { "cipher1EnvAmount", 0 } }),
            {}, "0000cc88,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000"),

        // A talking formant: the vowel sweeps A-E-I-O-U and back each bar.
        withCipher (make ("Talking Formant", O, 4, "Cipher,FX,Filter", {
            { cipherOn, 1 }, { rigLevel, 11 },
            { "cipher1Type", 15 }, { "cipher1Main", 0.5f }, { "cipher1P1", 0.6f }, { "cipher1EnvAmount", 1.0f }, { "cipher1Speed", 3 } }),
            { { 1, 3 } }),

        // Half-time tape: each quarter note's sound played back at half speed, an octave down, under the dry.
        make ("Half-Time Tape", O, 5, "Cipher,FX,Pitch", {
            { cipherOn, 1 }, { rigLevel, 2 },
            { "cipher1Type", 36 }, { "cipher1Main", 0.25f }, { "cipher1P1", 0.4f }, { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.6f } }),

        // Beat repeats: the last sixteenth looped, a new one every quarter, on the sequencer's beats.
        withCipher (make ("Beat Repeat Chop", O, 6, "Cipher,FX,Glitch", {
            { cipherOn, 1 }, { cipherSeqOn, 1 }, { cipherSeqSpeed, 2 }, { rigLevel, 1 },
            { "cipher1Type", 33 }, { "cipher1Option", 0 }, { "cipher1Main", 0.667f }, { "cipher1P1", 0.4f }, { "cipher1P2", 0.2f },
            { "cipher1EnvAmount", 0 } }),
            {}, "0000f0f0,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000"),

        // An arpeggio: the pitch shifter stepping through the envelope's steps,
        // snapped to A minor pentatonic, beside the dry note.
        withCipher (make ("Pentatonic Arp", O, 7, "Cipher,FX,Pitch", {
            { cipherOn, 1 }, { cipherScale, 4 }, { cipherScaleRoot, 9 }, { rigLevel, 6 },
            { "cipher1Type", 23 }, { "cipher1Main", 0.5f }, { "cipher1P1", 0.7f }, { "cipher1Style", 3 },
            { "cipher1EnvAmount", 0.5f }, { "cipher1Speed", 8 }, { "cipher1Mix", 0.6f } }),
            { { 1, 6 } }),

        // A frozen spectral pad: each frequency held and fading, into a spectral reverb.
        make ("Spectral Freeze Pad", O, 8, "Cipher,FX,Ambient", {
            { cipherOn, 1 }, { rigLevel, -8 },
            { "cipher1Type", 48 }, { "cipher1Main", 0.85f }, { "cipher1P1", 0.4f }, { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.7f },
            { "cipher2Type", 49 }, { "cipher2Main", 0.6f }, { "cipher2EnvAmount", 0 } }),

        // A robot choir: your playing shapes the vocoder's oscillator.
        make ("Robot Choir", O, 9, "Cipher,FX", {
            { cipherOn, 1 }, { rigLevel, 9 },
            { "cipher1Type", 32 }, { "cipher1Main", 0.4f }, { "cipher1P2", 0.3f }, { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.85f } }),

        // A ring-mod siren: the modulator rising and falling every two bars.
        withCipher (make ("Ring Siren", O, 10, "Cipher,FX", {
            { cipherOn, 1 }, { rigLevel, 3 },
            { "cipher1Type", 25 }, { "cipher1Main", 0.55f }, { "cipher1P1", 0.8f }, { "cipher1EnvAmount", 0.6f }, { "cipher1Speed", 2 } }),
            { { 1, 3 } }),

        // A comb riser: the comb climbing over two bars, gliding.
        withCipher (make ("Comb Riser", O, 11, "Cipher,FX,Filter", {
            { cipherOn, 1 }, { rigLevel, 0 },
            { "cipher1Type", 21 }, { "cipher1Main", 0.35f }, { "cipher1P1", 0.85f }, { "cipher1P2", 0.6f },
            { "cipher1EnvAmount", 0.8f }, { "cipher1Speed", 2 }, { "cipher1Glide", 50 } }),
            { { 1, 1 } }),

        // A wide phaser swirl, its two sides apart.
        withCipher (make ("Phaser Swirl", O, 12, "Cipher,FX,Modulation", {
            { cipherOn, 1 }, { rigLevel, 3 },
            { "cipher1Type", 22 }, { "cipher1Main", 0.45f }, { "cipher1P1", 0.6f }, { "cipher1P2", 0.6f },
            { "cipher1EnvAmount", 0.6f }, { "cipher1Speed", 3 }, { "cipher1Stereo", 0.6f } }),
            { { 1, 4 } }),

        // A wobble: the dual filter jumping in triplet eighths, glided into a wobble.
        withCipher (make ("Wobble Filter", O, 13, "Cipher,FX,Filter", {
            { cipherOn, 1 }, { rigLevel, -5 },
            { "cipher1Type", 14 }, { "cipher1Main", 0.38f }, { "cipher1P1", 0.6f }, { "cipher1P2", 0.3f },
            { "cipher1EnvAmount", 0.8f }, { "cipher1Speed", 12 }, { "cipher1Glide", 60 } }),
            { { 1, 5 } }),

        // A lo-fi radio being tuned: a narrow spectral band sweeping, then a lower sample rate.
        withCipher (make ("Lo-Fi Radio Sweep", O, 14, "Cipher,FX,Lo-Fi", {
            { cipherOn, 1 }, { rigLevel, 4 },
            { "cipher1Type", 44 }, { "cipher1Main", 0.55f }, { "cipher1P1", 0.2f }, { "cipher1EnvAmount", 0.5f }, { "cipher1Speed", 2 },
            { "cipher2Type", 28 }, { "cipher2Main", 0.65f }, { "cipher2P1", 0.6f }, { "cipher2EnvAmount", 0 } }),
            { { 1, 3 } }),

        // A filtered ping-pong: a high-pass sweeping each bar into eighth echoes bouncing side to side.
        withCipher (make ("Ping-Pong Filter Echo", O, 15, "Cipher,FX,Delay", {
            { cipherOn, 1 }, { rigLevel, 1 },
            { "cipher1Type", 6 }, { "cipher1Main", 0.3f }, { "cipher1EnvAmount", 0.6f }, { "cipher1Speed", 3 },
            { "cipher2Type", 29 }, { "cipher2Main", 0.385f }, { "cipher2P1", 0.45f }, { "cipher2Stereo", 1.0f }, { "cipher2EnvAmount", 0 },
            { "cipher2Mix", 0.8f } }),
            { { 1, 3 } }),

        //======================================================================
        // Bank P: an album's guitar sounds, inspired by Static-X's Wisconsin
        // Death Trip (1999; the owner's tenth album bank). Cleans, effects and
        // leads: the riffs are the amp capture's job. Documented: produced by
        // Ulrich Wild in about eight weeks; Wayne Static's guitars through his
        // Marshall Valvestate VS100 head (Wild, in an interview); Koichi Fukuda
        // on lead guitar and programming; an industrial "evil disco" sound of
        // riffs, samples and machine-like drums. Nothing is published on the
        // guitar effects, so these are our reading, many on CIPHER.

        // The riff chopped on the sixteenths by CIPHER's sequencer (two on, two off).
        withCipher (make ("Evil Disco Chop", P, 1, "Album,FX,Glitch", {
            { cipherOn, 1 }, { cipherSeqOn, 1 }, { cipherSeqSpeed, 2 }, { rigLevel, 3 },
            { "cipher1Type", 19 }, { "cipher1Main", 0.0f }, { "cipher1EnvAmount", 0 } }),
            {}, "0000cccc,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000000"),

        // A noisy industrial lead: boosted, ring-modulated and crushed.
        make ("Ring Noise Lead", P, 2, "Album,Lead,FX", {
            { rigLevel, -4 },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { cipherOn, 1 }, { "cipher1Type", 25 }, { "cipher1Main", 0.5f }, { "cipher1P1", 0.5f }, { "cipher1P2", 0.6f },
            { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.5f },
            { lofiOn, 1 }, { lofiType, bitcrusher }, { lofiAmount, 5 }, { lofiNoise, 1 }, { lofiMix, 0.4f } }),

        // A synth-like pulse: a resonant filter plucked open every sixteenth.
        withCipher (make ("Sixteenth Synth Pulse", P, 3, "Album,FX,Filter", {
            { cipherOn, 1 }, { rigLevel, 3 },
            { "cipher1Type", 7 }, { "cipher1Main", 0.4f }, { "cipher1P1", 0.7f }, { "cipher1P2", 0.2f },
            { "cipher1EnvAmount", 0.6f }, { "cipher1Speed", 14 } }),
            { { 1, 7 } }),

        // A robotic voice: the riff through CIPHER's vocoder.
        make ("Robot Riff Voice", P, 4, "Album,FX", {
            { cipherOn, 1 }, { rigLevel, 9 },
            { "cipher1Type", 32 }, { "cipher1Main", 0.35f }, { "cipher1P2", 0.3f }, { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.7f } }),

        // A machine-gun clean: a hard sixteenth tremolo, a plate.
        make ("Machine Tremolo Clean", P, 5, "Album,Clean,Modulation", {
            { rigLevel, 4 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { modOn, 1 }, { modType, tremolo }, { modSync, sixteenth }, { modDepth, 1.0f },
            { reverbOn, 1 }, { reverbType, plate }, { reverbSize, 0.5f }, { reverbMix, 0.2f } }),

        // A creepy drone: volume swells (from the heel) into a reverse reverb and a shimmer.
        make ("Graveyard Drone Swell", P, 6, "Album,Clean,Ambient,Expression", {
            { rigLevel, 2 },
            { expressionTarget, pedalVolume },
            { compOn, 1 }, { compThreshold, -26 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, shimmer }, { delaySync, quarter }, { delayFeedback, 0.5f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, reverse }, { reverbSize, 0.7f }, { reverbMix, 0.45f } }),

        // A hypnotic flange on the riff.
        make ("Hypno Flange Riff", P, 7, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, flanger }, { modRate, 0.5f }, { modDepth, 0.8f }, { modManual, 4 }, { modRes, 6 } }),

        // The riff cut and re-sequenced on the sixteenths.
        make ("Glitched Riff Sequence", P, 8, "Album,FX,Glitch", {
            { glitchOn, 1 }, { glitchMode, glitchSequence }, { glitchSync, 1 }, { glitchDivision, glitchSixteenth },
            { glitchDensity, 6 }, { glitchChaos, 4 }, { glitchMix, 1 } }),

        // An octave squeal on the pedal for accents: heel your note, toe an octave up.
        make ("Quasar Squeal Accent", P, 9, "Album,Lead,Pitch,Expression", {
            { rigLevel, -3 },
            { fpOn, 1 }, { fpType, whammyWah }, { fpXpProgram, xpUpOctave }, { fpMix, 1 },
            { expressionTarget, pedalWhammyWah },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 } }),

        // A haunted clean as if through an old phone line, with static.
        make ("Haunted Radio Clean", P, 10, "Album,Clean,Lo-Fi", {
            { rigLevel, 3 },
            { lofiOn, 1 }, { lofiType, telephone }, { lofiAmount, 6 }, { lofiNoise, 3 }, { lofiMix, 1 },
            { reverbOn, 1 }, { reverbType, room }, { reverbSize, 0.4f }, { reverbMix, 0.15f } }),

        // A spectral wash: every frequency held and fading, into a spectral reverb.
        make ("Spectral Wash Clean", P, 11, "Album,Clean,Ambient", {
            { cipherOn, 1 }, { rigLevel, -6 },
            { "cipher1Type", 48 }, { "cipher1Main", 0.8f }, { "cipher1P1", 0.3f }, { "cipher1EnvAmount", 0 }, { "cipher1Mix", 0.6f },
            { "cipher2Type", 49 }, { "cipher2Main", 0.55f }, { "cipher2EnvAmount", 0 } }),

        // A cold, clear clean with a dotted-eighth echo and a hall.
        make ("Frozen Echo Clean", P, 12, "Album,Clean,Delay", {
            { rigLevel, 3 },
            { compOn, 1 }, { compThreshold, -24 }, { compRatio, 3 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, dottedEighth }, { delayFeedback, 0.4f }, { delayMix, 0.25f },
            { reverbOn, 1 }, { reverbType, hall }, { reverbSize, 0.6f }, { reverbMix, 0.25f } }),

        // A pitch dive: CIPHER's shifter falling from an octave up to an octave down each bar, beside the dry.
        withCipher (make ("Pitch Dive Riser", P, 13, "Album,FX,Pitch", {
            { cipherOn, 1 }, { rigLevel, 1 },
            { "cipher1Type", 23 }, { "cipher1Main", 0.5f }, { "cipher1P1", 0.7f }, { "cipher1EnvAmount", 1.0f },
            { "cipher1Speed", 3 }, { "cipher1Mix", 0.5f } }),
            { { 1, 2 } }),

        // A fast phaser on the riff.
        make ("Fast Phase Riff", P, 14, "Album,FX,Modulation", {
            { modOn, 1 }, { modType, phaser }, { modRate, 2.5f }, { modDepth, 0.7f } }),

        // A synth-like lead: an octave above, boosted, chorused wide, an echo.
        make ("Synth Octave Lead", P, 15, "Album,Lead,Pitch,Octave", {
            { rigLevel, -5 },
            { octOn, 1 }, { octSubOne, 0 }, { octDry, 0.9f }, { octUp, 0.6f },
            { driveOn, 1 }, { driveModel, banshee }, { driveGain, 3 }, { driveTone, 6 }, { driveLevel, 5 },
            { modOn, 1 }, { modType, chorus }, { modRate, 0.5f }, { modDepth, 0.5f }, { modMix, 0.4f }, { modStereo, 1 },
            { delayOn, 1 }, { delayType, digitalDelay }, { delaySync, eighth }, { delayFeedback, 0.3f }, { delayMix, 0.18f } }),
    };
}

juce::String currentFactoryName (const juce::String& name)
{
    // Factory rigs renamed in 0.49.0 to our own names (no trademarks,
    // artists or song titles). Projects and favourites saved before then
    // still name the old ones.
    static const std::pair<const char*, const char*> renamed[] {
        { "Leslie Spin", "Rotary Spin" },
        { "Dimension Clean", "Prism Clean" },
        { "Machine Gun Vibe", "Nebula Lead" },
        { "Whammy Lead", "Quasar Lead" },
        { "XP Wah Crunch", "Quasar Wah Crunch" },
        { "Brownface Trem", "Vintage Amp Trem" },
        { "Killing Solo (RATM)", "Octave Squeal Solo" },
        { "Bulls Scratch (RATM)", "Wah Scratch Chop" },
        { "Numb Solo (Floyd)", "Soaring Fuzz Solo" },
        { "Us and Them (Floyd)", "Slow Rotary Clean" },
        { "Run Like Hell (Floyd)", "Galloping Delay" },
        { "Money Solo (Floyd)", "Fuzz Echo Solo" },
        { "Jambi Talk Box (Tool)", "Heavy Talk Box" },
        { "Show Me the Way (Frampton)", "Warm Talk Box" },
        // 0.58.0: B12 moved to a stab, so its name followed.
        { "Laser Transformer", "Stab Transformer" },
    };
    for (const auto& [oldName, newName] : renamed)
        if (name == oldName)
            return newName;
    return name;
}
