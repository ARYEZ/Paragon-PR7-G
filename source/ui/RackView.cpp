#include "RackView.h"

#include "LibraryFolders.h"

namespace RackView
{
    namespace
    {
        constexpr auto settingKey = "rackShown";

        struct State
        {
            State()
            {
                // Read once, the first time a window opens.
                juce::PropertiesFile file (LibraryFolders::settingsOptions());
                shown = file.getBoolValue (settingKey, true);
            }

            bool shown = true;
            juce::ChangeBroadcaster broadcaster;
        };

        State& state()
        {
            // Shared by every PR7-G window open in the DAW (they're in one
            // process), and only touched on the message thread.
            static State instance;
            return instance;
        }
    }

    bool isShown() { return state().shown; }

    void setShown (bool shown, bool remember)
    {
        if (remember)
        {
            juce::PropertiesFile file (LibraryFolders::settingsOptions());
            file.setValue (settingKey, shown);
            file.saveIfNeeded();
        }
        if (state().shown != shown)
        {
            state().shown = shown;
            state().broadcaster.sendSynchronousChangeMessage();
        }
    }

    juce::ChangeBroadcaster& changes() { return state().broadcaster; }
}
