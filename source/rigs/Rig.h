#pragma once

#include <juce_core/juce_core.h>

#include "Parameters.h"

#include <map>
#include <optional>

// A rig: one complete sound. Every amp, cabinet and effect setting, the
// model and IR files, and how it's filed (name, bank, number, tags).
//
// The INPUT and OUTPUT knobs at the sides aren't part of a rig: they suit
// the player's guitar and monitoring, not a particular sound, so switching
// rigs leaves them alone (as on hardware modellers). Each rig has its own
// RIG LEVEL for balancing rigs against each other instead.
struct Rig
{
    static constexpr int numBanks = 5;          // A to D, and E for the song-inspired rigs
    static constexpr int maxNumber = 99;

    juce::String name { "New Rig" };
    int bank = 0;                               // 0 = A
    int number = 1;                             // 1 to maxNumber
    bool favourite = false;
    juce::StringArray tags;

    // Parameter values in their real units (dB, ms...), by parameter ID.
    // Storing real values rather than 0-1 positions means a rig still
    // loads correctly if a later version changes a knob's range. Loading a
    // rig resets any parameter it doesn't mention to its default, so rigs
    // saved before a parameter existed get that parameter's neutral setting.
    std::map<juce::String, float> parameters;

    juce::String ampModelPath, cabIrPath;

    // Factory rigs set only the effects: we can't ship other people's NAM
    // captures, so they keep whatever amp and cabinet the user has loaded.
    bool keepAmpAndCab = false;

    juce::String getSlotName() const;           // e.g. "A01"
    static juce::String bankName (int bank);    // "A".."E"

    std::unique_ptr<juce::XmlElement> toXml() const;
    // Returns nothing if the XML isn't a rig.
    static std::optional<Rig> fromXml (const juce::XmlElement&);

    // A rig or project saved with the original rotary speaker (before 0.55.0)
    // on MOD or MOD 2: Cyclone's controls set as close to it as they go.
    // Leaves anything already saved with Cyclone alone.
    static void convertOldRotary (std::map<juce::String, float>& parameters, const ParamIDs::ModIds&);

    static constexpr const char* fileExtension = ".pr7rig";
    // From when the plugin was called Paragon X1: still read, never written.
    static constexpr const char* oldFileExtension = ".x1rig";
};
