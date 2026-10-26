#include "LibraryFolders.h"

namespace LibraryFolders
{
    namespace
    {
        const char* keyFor (Kind kind) { return kind == Kind::amps ? "namAmpFolder" : "cabIrFolder"; }

        struct State
        {
            State()
            {
                juce::PropertiesFile file (settingsOptions());
                for (const auto kind : { Kind::amps, Kind::cabs })
                    if (const auto path = file.getValue (keyFor (kind)); juce::File::isAbsolutePath (path))
                        folders[(int) kind] = juce::File (path);
            }

            juce::CriticalSection lock;   // the loader thread reads these too
            juce::File folders[2];
        };

        State& state()
        {
            // Shared by every PR7-G open in the DAW (they're in one process).
            static State instance;
            return instance;
        }
    }

    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Paragon PR7-G";
        options.folderName = "Paragon";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        return options;
    }

    juce::File get (Kind kind)
    {
        const juce::ScopedLock lock (state().lock);
        return state().folders[(int) kind];
    }

    void set (Kind kind, const juce::File& folder, bool remember)
    {
        {
            const juce::ScopedLock lock (state().lock);
            state().folders[(int) kind] = folder;
        }
        if (remember)
        {
            juce::PropertiesFile file (settingsOptions());
            file.setValue (keyFor (kind), folder.getFullPathName());
            file.saveIfNeeded();
        }
    }

    juce::File findByName (const juce::File& folder, const juce::String& fileName)
    {
        if (fileName.isEmpty() || ! folder.isDirectory())
            return {};
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
            if (entry.getFile().getFileName().equalsIgnoreCase (fileName))
                return entry.getFile();
        return {};
    }

    juce::File locate (Kind kind, const juce::File& file)
    {
        if (file.existsAsFile())
            return file;
        if (const auto found = findByName (get (kind), file.getFileName()); found.existsAsFile())
            return found;
        return file;
    }
}
