#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

// Where the player keeps their NAM amp models and cabinet IRs, chosen on
// the SETTINGS page. Saved on this computer (with the finish), so every
// PR7-G in every project uses them. The amp and cab browsers open there,
// and a rig whose file has moved is looked for there by name.
namespace LibraryFolders
{
    enum class Kind { amps, cabs };

    // The PR7-G settings file on this computer (shared with the finish).
    juce::PropertiesFile::Options settingsOptions();

    // The chosen folder, or none. Safe from any thread.
    juce::File get (Kind);
    void set (Kind, const juce::File& folder, bool remember = true);

    // A file with this name in the folder or any folder inside it, or none.
    juce::File findByName (const juce::File& folder, const juce::String& fileName);

    // Where to look for a missing file: the same name in the chosen folder.
    // Returns the file itself if it exists or nothing better is found.
    juce::File locate (Kind, const juce::File& file);
}
