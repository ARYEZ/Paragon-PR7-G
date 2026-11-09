#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "rigs/FactoryRigs.h"
#include "ui/EqPresets.h"

using Catch::Matchers::WithinAbs;

namespace
{
    // A fresh, empty rigs folder for each test.
    struct TempRigFolder
    {
        TempRigFolder() { folder.deleteRecursively(); }
        ~TempRigFolder() { folder.deleteRecursively(); }
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ParagonX1RigTests")
                                .getChildFile (juce::Uuid().toString());
    };

    struct RigRig   // a processor with its rigs in a temporary folder
    {
        RigRig()
        {
            processor.setPlayConfigDetails (1, 2, 48000.0, 256);
            processor.prepareToPlay (48000.0, 256);
            processor.useRigFolder (temp.folder);
        }

        void set (const char* id, float plainValue)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        }

        float get (const char* id) { return processor.getParameters().getRawParameterValue (id)->load(); }

        const RigLibrary::Entry& entryNamed (const juce::String& name)
        {
            for (const auto& entry : processor.getRigLibrary().getEntries())
                if (entry.rig.name == name)
                    return entry;
            FAIL ("No rig called " << name);
            return processor.getRigLibrary().getEntries().front();
        }

        TempRigFolder temp;
        ParagonX1AudioProcessor processor;
    };

    const juce::File exampleModels { PARAGON_NAM_EXAMPLE_MODELS };
}

TEST_CASE ("A rig survives being written to a file and read back", "[rigs]")
{
    Rig rig;
    rig.name = "AC/DC: \"Back in Black\" & <more>";
    rig.bank = 2;
    rig.number = 17;
    rig.favourite = true;
    rig.tags = { "Rock", "High Gain" };
    rig.parameters = { { "driveGain", 6.5f }, { "delayTime", 420.0f } };
    rig.ampModelPath = "C:\\Models\\Plexi.nam";
    rig.cabIrPath = "/Users/me/IRs/4x12 V30.wav";

    const auto restored = Rig::fromXml (*juce::XmlDocument::parse (rig.toXml()->toString()));
    REQUIRE (restored.has_value());
    CHECK (restored->name == rig.name);
    CHECK (restored->getSlotName() == "C17");
    CHECK (restored->favourite);
    CHECK (restored->tags == rig.tags);
    CHECK (restored->parameters == rig.parameters);
    CHECK (restored->ampModelPath == rig.ampModelPath);
    CHECK (restored->cabIrPath == rig.cabIrPath);
    CHECK_FALSE (restored->keepAmpAndCab);
}

TEST_CASE ("Files that aren't rigs, or are from a newer version, are refused", "[rigs]")
{
    CHECK_FALSE (Rig::fromXml (*juce::XmlDocument::parse ("<SomethingElse/>")).has_value());
    CHECK_FALSE (Rig::fromXml (*juce::XmlDocument::parse ("<ParagonRig version=\"99\" name=\"x\"/>")).has_value());
}

TEST_CASE ("Every factory rig is valid", "[rigs]")
{
    ParagonX1AudioProcessor processor;
    juce::StringArray names, slots;

    for (const auto& rig : makeFactoryRigs())
    {
        INFO (rig.name);
        CHECK (rig.keepAmpAndCab);
        names.addIfNotAlreadyThere (rig.name);
        slots.addIfNotAlreadyThere (rig.getSlotName());

        for (const auto& [id, value] : rig.parameters)
        {
            INFO (id);
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.getParameters().getParameter (id));
            REQUIRE (parameter != nullptr);                          // no typos in parameter IDs
            CHECK_FALSE (ParamIDs::notInRigs.contains (id));        // rigs don't touch global settings
            const auto range = parameter->getNormalisableRange();
            CHECK (value >= range.start);
            CHECK (value <= range.end);
        }
    }

    CHECK (names.size() == (int) makeFactoryRigs().size());   // unique names
    CHECK (slots.size() == (int) makeFactoryRigs().size());   // no two in the same slot
}

TEST_CASE ("The Whammy-Wah factory rigs use the XP-100 programs they're named for", "[rigs]")
{
    const auto rigs = makeFactoryRigs();
    const auto find = [&] (const juce::String& name) -> const Rig&
    {
        const auto found = std::find_if (rigs.begin(), rigs.end(), [&] (const Rig& rig) { return rig.name == name; });
        REQUIRE (found != rigs.end());
        return *found;
    };
    const auto programOf = [] (const Rig& rig)
    {
        return effects::FilterPitch::xpProgramNames()[juce::roundToInt (rig.parameters.at (ParamIDs::fpXpProgram))];
    };

    CHECK (programOf (find ("Nu-Metal Screech")) == "21 Down Oct / Up Oct");
    CHECK (programOf (find ("Quasar Lead")) == "13 Up 1 Octave");
    CHECK (programOf (find ("Quasar Wah Crunch")) == "02 Classic Wah");
    CHECK (programOf (find ("Harmony Bends")) == "24 Up 4th / Up 5th");

    // The screech's EQ is the Tight Low End preset (bands it leaves out are at 0 dB).
    const auto& screech = find ("Nu-Metal Screech");
    CHECK (juce::roundToInt (screech.parameters.at (ParamIDs::expressionTarget)) == ExpressionTargets::whammyWah);
    const auto& tight = *std::find_if (EqPresets::all.begin(), EqPresets::all.end(),
                                       [] (const auto& preset) { return juce::String (preset.name) == "Tight Low End"; });
    for (int band = 0; band < ParamIDs::eqBands.size(); ++band)
    {
        const auto found = screech.parameters.find (ParamIDs::eqBands[band]);
        CHECK_THAT (found != screech.parameters.end() ? found->second : 0.0f, Catch::Matchers::WithinAbs (tight.gainsDb[(size_t) band], 0.01));
    }
    CHECK_THAT (screech.parameters.at (ParamIDs::eqLevel), Catch::Matchers::WithinAbs (tight.levelDb, 0.01));
}

TEST_CASE ("The rig library saves, finds, renames and deletes rigs", "[rigs]")
{
    TempRigFolder temp;
    RigLibrary library (temp.folder);
    const auto factoryCount = library.getEntries().size();

    Rig rig;
    rig.name = "Cyber Drive";
    const auto first = library.saveAsNew (rig);
    REQUIRE (first.existsAsFile());
    REQUIRE (library.find (first) != nullptr);

    SECTION ("A taken name gets a number")
    {
        const auto second = library.saveAsNew (rig);
        REQUIRE (library.find (second) != nullptr);
        CHECK (library.find (second)->rig.name == "Cyber Drive 2");
        CHECK (second != first);
    }

    SECTION ("Names that can't be file names still save")
    {
        rig.name = "AC/DC: Live?";
        const auto file = library.saveAsNew (rig);
        REQUIRE (file.existsAsFile());
        CHECK (library.find (file)->rig.name == "AC/DC: Live?");
    }

    SECTION ("Overwrite and delete")
    {
        auto changed = library.find (first)->rig;
        changed.tags = { "Lead" };
        REQUIRE (library.overwrite (first, changed));
        CHECK (library.find (first)->rig.tags == juce::StringArray { "Lead" });

        REQUIRE (library.remove (first));
        CHECK (library.find (first) == nullptr);
        CHECK (library.getEntries().size() == factoryCount);
    }

    SECTION ("A corrupt rig file is skipped, not fatal")
    {
        temp.folder.getChildFile (juce::String ("broken") + Rig::fileExtension).replaceWithText ("<ParagonRig name=");
        library.rescan();
        CHECK (library.getEntries().size() == factoryCount + 1);
    }
}

TEST_CASE ("Favourites stick, including on factory rigs", "[rigs]")
{
    TempRigFolder temp;
    {
        RigLibrary library (temp.folder);
        library.setFavourite (*library.findFactory ("Slapback"), true);
    }

    RigLibrary reopened (temp.folder);
    CHECK (reopened.findFactory ("Slapback")->rig.favourite);
    CHECK_FALSE (reopened.findFactory ("Studio Clean")->rig.favourite);
}

TEST_CASE ("Renamed factory rigs are still found by their old names", "[rigs][state]")
{
    // Every new name is a real factory rig; other names pass through.
    TempRigFolder temp;
    {
        RigLibrary library (temp.folder);
        for (auto name : { "Leslie Spin", "Whammy Lead", "XP Wah Crunch", "Numb Solo (Floyd)", "Show Me the Way (Frampton)", "Laser Transformer" })
        {
            CAPTURE (name);
            CHECK (library.findFactory (name) == nullptr);
            CHECK (library.findFactory (currentFactoryName (name)) != nullptr);
        }
        CHECK (currentFactoryName ("Slapback") == "Slapback");
        CHECK (currentFactoryName ("My Own Rig") == "My Own Rig");
    }

    // A favourite saved under an old name stays a favourite.
    temp.folder.createDirectory();
    temp.folder.getChildFile ("factory-favourites.txt").replaceWithText ("Leslie Spin\nSlapback");
    RigLibrary reopened (temp.folder);
    CHECK (reopened.findFactory ("Rotary Spin")->rig.favourite);
    CHECK (reopened.findFactory ("Slapback")->rig.favourite);

    // A project saved with an old factory rig loaded finds it by its new name.
    juce::MemoryBlock saved;
    {
        RigRig r;
        r.processor.loadRig (r.entryNamed ("Rotary Spin"));
        r.processor.getStateInformation (saved);
    }
    auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
    REQUIRE (xml != nullptr);
    REQUIRE (xml->getStringAttribute ("rigFactory") == "Rotary Spin");
    xml->setAttribute ("rigFactory", "Leslie Spin");   // as 0.48.0 and earlier saved it
    xml->setAttribute ("rigName", "Leslie Spin");
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary (*xml, old);

    RigRig restored;
    restored.processor.setStateInformation (old.getData(), (int) old.getSize());
    CHECK (restored.processor.getCurrentRig().name == "Rotary Spin");
    CHECK (restored.processor.getCurrentRig().factoryName == "Rotary Spin");
    REQUIRE (restored.processor.findCurrentRigEntry() != nullptr);
    CHECK (restored.processor.findCurrentRigEntry()->rig.name == "Rotary Spin");
}

TEST_CASE ("Rigs are sorted by bank and number, and < > wrap round", "[rigs]")
{
    TempRigFolder temp;
    RigLibrary library (temp.folder);

    Rig late;
    late.name = "Late";
    late.bank = Rig::numBanks - 1;   // the last bank, after every factory rig
    late.number = 50;
    library.saveAsNew (late);

    const auto& entries = library.getEntries();
    for (size_t i = 1; i < entries.size(); ++i)
        CHECK (entries[i - 1].rig.bank * 100 + entries[i - 1].rig.number <= entries[i].rig.bank * 100 + entries[i].rig.number);

    CHECK (entries.back().rig.name == "Late");
    CHECK (library.step (&entries.back(), 1) == &entries.front());
    CHECK (library.step (&entries.front(), -1) == &entries.back());
    CHECK (library.step (nullptr, 1) == &entries.front());

    CHECK (library.nextFreeNumber (0) == 6);   // factory rigs fill A01-A05
}

TEST_CASE ("Saving and loading a rig restores the sound but not the global knobs", "[rigs]")
{
    RigRig r;
    r.set (ParamIDs::driveOn, 1.0f);
    r.set (ParamIDs::driveGain, 7.5f);
    r.set (ParamIDs::delayTime, 640.0f);
    r.set (ParamIDs::rigLevel, -3.0f);
    REQUIRE (r.processor.saveCurrentRigAs ("My Sound"));

    // Change everything, including the global input and output knobs.
    r.set (ParamIDs::driveOn, 0.0f);
    r.set (ParamIDs::driveGain, 1.0f);
    r.set (ParamIDs::delayTime, 100.0f);
    r.set (ParamIDs::rigLevel, 0.0f);
    r.set (ParamIDs::inputGain, 5.0f);
    r.set (ParamIDs::outputGain, -4.0f);

    r.processor.loadRig (r.entryNamed ("My Sound"));

    CHECK (r.get (ParamIDs::driveOn) > 0.5f);
    CHECK_THAT (r.get (ParamIDs::driveGain), WithinAbs (7.5, 0.01));
    CHECK_THAT (r.get (ParamIDs::delayTime), WithinAbs (640.0, 0.5));
    CHECK_THAT (r.get (ParamIDs::rigLevel), WithinAbs (-3.0, 0.01));
    // Global knobs are left alone.
    CHECK_THAT (r.get (ParamIDs::inputGain), WithinAbs (5.0, 0.01));
    CHECK_THAT (r.get (ParamIDs::outputGain), WithinAbs (-4.0, 0.01));
}

TEST_CASE ("A rig that doesn't mention a parameter sets it to its default", "[rigs]")
{
    // E.g. a rig saved by an older version, before that parameter existed.
    RigRig r;
    r.set (ParamIDs::reverbOn, 1.0f);
    r.set (ParamIDs::presence, 6.0f);

    Rig old;
    old.name = "Old Rig";
    old.parameters = { { ParamIDs::driveOn, 1.0f } };
    r.processor.getRigLibrary().saveAsNew (old);
    r.processor.loadRig (r.entryNamed ("Old Rig"));

    CHECK (r.get (ParamIDs::driveOn) > 0.5f);
    CHECK (r.get (ParamIDs::reverbOn) < 0.5f);
    CHECK_THAT (r.get (ParamIDs::presence), WithinAbs (0.0, 0.01));
}

TEST_CASE ("User rigs bring their amp and cab; factory rigs keep yours", "[rigs]")
{
    RigRig r;
    const auto wavenet = exampleModels.getChildFile ("wavenet.nam");
    const auto lstm = exampleModels.getChildFile ("lstm.nam");

    r.processor.loadAmpModel (wavenet);
    REQUIRE (r.processor.waitForLoads (20000));
    REQUIRE (r.processor.saveCurrentRigAs ("Wavenet Rig"));

    r.processor.loadAmpModel (lstm);
    REQUIRE (r.processor.waitForLoads (20000));
    REQUIRE (r.processor.saveCurrentRigAs ("LSTM Rig"));

    SECTION ("Loading a user rig loads its model")
    {
        r.processor.loadRig (r.entryNamed ("Wavenet Rig"));
        REQUIRE (r.processor.waitForLoads (20000));
        CHECK (r.processor.getAmpStatus().file == wavenet);
    }

    SECTION ("Loading a factory rig keeps the current model")
    {
        r.processor.loadRig (r.entryNamed ("Ambient Swells"));
        REQUIRE (r.processor.waitForLoads (20000));
        CHECK (r.processor.getAmpStatus().file == lstm);
        CHECK (r.get (ParamIDs::reverbOn) > 0.5f);
    }

    SECTION ("A user rig saved with no amp unloads the amp")
    {
        r.processor.clearAmpModel();
        REQUIRE (r.processor.waitForLoads (20000));
        REQUIRE (r.processor.saveCurrentRigAs ("No Amp"));
        r.processor.loadRig (r.entryNamed ("Wavenet Rig"));
        REQUIRE (r.processor.waitForLoads (20000));
        r.processor.loadRig (r.entryNamed ("No Amp"));
        REQUIRE (r.processor.waitForLoads (20000));
        CHECK (r.processor.getAmpStatus().file == juce::File());
    }
}

TEST_CASE ("The modified marker tracks changes since loading or saving", "[rigs]")
{
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Studio Clean"));
    CHECK_FALSE (r.processor.isRigModified());

    SECTION ("Turning a knob marks it modified")
    {
        r.set (ParamIDs::driveGain, 9.0f);
        CHECK (r.processor.isRigModified());
    }

    SECTION ("Loading a different amp marks it modified")
    {
        r.processor.loadAmpModel (exampleModels.getChildFile ("wavenet.nam"));
        CHECK (r.processor.isRigModified());
        REQUIRE (r.processor.waitForLoads (20000));
    }

    SECTION ("The global knobs don't count")
    {
        r.set (ParamIDs::inputGain, 3.0f);
        r.set (ParamIDs::outputGain, -3.0f);
        CHECK_FALSE (r.processor.isRigModified());
    }

    SECTION ("Saving clears it")
    {
        r.set (ParamIDs::driveGain, 9.0f);
        REQUIRE (r.processor.saveCurrentRigAs ("Saved"));
        CHECK_FALSE (r.processor.isRigModified());
        r.set (ParamIDs::driveGain, 2.0f);
        CHECK (r.processor.isRigModified());
        REQUIRE (r.processor.saveCurrentRig());   // a user rig now, so plain save works
        CHECK_FALSE (r.processor.isRigModified());
    }
}

TEST_CASE ("Factory rigs can't be saved over", "[rigs]")
{
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Slapback"));
    r.set (ParamIDs::delayMix, 0.9f);
    CHECK_FALSE (r.processor.saveCurrentRig());   // the UI then asks for a new name
    CHECK_THAT (r.entryNamed ("Slapback").rig.parameters.at (ParamIDs::delayMix), WithinAbs (0.3, 0.001));
}

TEST_CASE ("< and > step through the rigs in order", "[rigs]")
{
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Clean Slate"));
    r.processor.stepRig (1);
    CHECK (r.processor.getCurrentRig().name == "Studio Clean");
    r.processor.stepRig (-1);
    r.processor.stepRig (-1);   // wraps round to the last rig
    CHECK (r.processor.getCurrentRig().name == r.processor.getRigLibrary().getEntries().back().rig.name);
}

TEST_CASE ("Deleting the loaded rig keeps the sound, now unsaved", "[rigs]")
{
    RigRig r;
    r.set (ParamIDs::driveGain, 8.0f);
    REQUIRE (r.processor.saveCurrentRigAs ("Temporary"));
    REQUIRE (r.processor.deleteRig (r.entryNamed ("Temporary")));

    CHECK_FALSE (r.processor.getCurrentRig().isUserRig());
    CHECK (r.processor.isRigModified());
    CHECK_THAT (r.get (ParamIDs::driveGain), WithinAbs (8.0, 0.01));
}

TEST_CASE ("The DAW project remembers which rig was loaded", "[rigs][state]")
{
    juce::MemoryBlock saved;
    juce::File rigFile;
    {
        RigRig r;
        r.set (ParamIDs::driveGain, 8.0f);
        REQUIRE (r.processor.saveCurrentRigAs ("Project Rig"));
        r.set (ParamIDs::driveGain, 3.0f);   // tweaked after saving
        rigFile = r.processor.getCurrentRig().file;
        r.processor.getStateInformation (saved);
    }

    ParagonX1AudioProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    REQUIRE (restored.waitForLoads (20000));
    CHECK (restored.getCurrentRig().name == "Project Rig");
    CHECK (restored.getCurrentRig().file == rigFile);
    CHECK (restored.isRigModified());   // the tweak is remembered as unsaved
    CHECK_THAT (restored.getParameters().getRawParameterValue (ParamIDs::driveGain)->load(), WithinAbs (3.0, 0.01));
}

TEST_CASE ("Each rig's RIG LEVEL adds to the master output", "[rigs]")
{
    RigRig r;
    r.set (ParamIDs::rigLevel, 6.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        for (int i = 0; i < 256; ++i)
            buffer.setSample (0, i, 0.1f * std::sin (0.1f * (float) (block * 256 + i)));
        r.processor.processBlock (buffer, midi);
    }
    CHECK_THAT (buffer.getMagnitude (0, 0, 256), WithinAbs (0.1 * 1.9953, 0.005));
}

TEST_CASE ("Rigs saved under an old name (Paragon X1, Paragon PR7) carry over to Paragon PR7-G", "[rigs]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("ParagonRename-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    const auto oldFolder = root.getChildFile ("Paragon PR7"), newFolder = root.getChildFile ("Paragon PR7-G");
    REQUIRE (oldFolder.getChildFile ("Rigs").createDirectory());
    REQUIRE (oldFolder.getChildFile ("Rigs").getChildFile ("My Rig.x1rig").replaceWithText ("<ParagonRig/>"));

    CHECK (RigLibrary::carryOverFromOldName (oldFolder, newFolder));
    CHECK (newFolder.getChildFile ("Rigs").getChildFile ("My Rig.x1rig").existsAsFile());
    CHECK (oldFolder.getChildFile ("Rigs").getChildFile ("My Rig.x1rig").existsAsFile());   // copied, not moved

    // Only once: after that the new folder is the one in use, even if rigs are deleted from it.
    newFolder.getChildFile ("Rigs").getChildFile ("My Rig.x1rig").deleteFile();
    CHECK_FALSE (RigLibrary::carryOverFromOldName (oldFolder, newFolder));
    CHECK_FALSE (newFolder.getChildFile ("Rigs").getChildFile ("My Rig.x1rig").exists());

    root.deleteRecursively();
}

TEST_CASE ("Rig files from Paragon X1 (.x1rig) still load", "[rigs]")
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("ParagonOldRigs-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    REQUIRE (folder.createDirectory());
    Rig old;
    old.name = "From X1";
    REQUIRE (old.toXml()->writeTo (folder.getChildFile ("From X1.x1rig")));

    RigLibrary library (folder);
    CHECK (std::any_of (library.getEntries().begin(), library.getEntries().end(),
                        [] (const auto& entry) { return entry.rig.name == "From X1"; }));
    folder.deleteRecursively();
}

TEST_CASE ("A rig file saved before reverb types loads with the original reverb", "[rigs]")
{
    auto xml = juce::XmlDocument::parse (R"(<ParagonRig version="1" parameterVersion="10" name="Old"><Param id="reverbOn" value="1"/></ParagonRig>)");
    REQUIRE (xml != nullptr);
    const auto rig = Rig::fromXml (*xml);
    REQUIRE (rig.has_value());
    REQUIRE (rig->parameters.count ("reverbType") == 1);
    CHECK (rig->parameters.at ("reverbType") == 0.0f);

    Rig current;   // saved by this version: written with its reverb type, and read back as it is
    current.parameters["reverbType"] = 4.0f;
    CHECK (Rig::fromXml (*current.toXml())->parameters.at ("reverbType") == 4.0f);
}

namespace
{
    // Plays one plucked note (a 220 Hz guitar-like tone with harmonics,
    // fading over a second) then three seconds of silence through the
    // loaded rig, and returns both channels.
    std::vector<std::vector<float>> pluck (ParagonX1AudioProcessor& processor, double noteSeconds = 1.0, double hz = 220.0, bool pure = false)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 256;
        const auto total = (int) ((noteSeconds + 3.0) * rate);
        std::vector<std::vector<float>> out (2);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (int start = 0; start < total; start += block)
        {
            buffer.clear();
            for (int i = 0; i < block; ++i)
            {
                const auto t = (start + i) / rate;
                if (t >= noteSeconds)
                    continue;
                const auto envelope = 0.3 * std::exp (-t * 2.0);
                auto sample = std::sin (juce::MathConstants<double>::twoPi * hz * t);
                if (! pure)
                    sample += 0.5 * std::sin (juce::MathConstants<double>::twoPi * 2.0 * hz * t)
                            + 0.3 * std::sin (juce::MathConstants<double>::twoPi * 3.0 * hz * t);
                buffer.setSample (0, i, (float) (envelope * sample));
            }
            processor.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                out[(size_t) ch].insert (out[(size_t) ch].end(), buffer.getReadPointer (ch), buffer.getReadPointer (ch) + block);
        }
        return out;
    }

    float rmsOf (const std::vector<float>& samples, double from, double to)
    {
        const auto a = (size_t) (from * 48000.0), b = std::min (samples.size(), (size_t) (to * 48000.0));
        double sum = 0.0;
        for (auto i = a; i < b; ++i)
            sum += (double) samples[i] * samples[i];
        return (float) std::sqrt (sum / (double) std::max<size_t> (1, b - a));
    }

    float dBOf (float gain) { return 20.0f * std::log10 (std::max (1.0e-9f, gain)); }

    // The level of one frequency (Goertzel) over a stretch.
    float levelAtHz (const std::vector<float>& samples, double hz, double from, double to)
    {
        const auto a = (size_t) (from * 48000.0), b = std::min (samples.size(), (size_t) (to * 48000.0));
        const auto coefficient = 2.0 * std::cos (juce::MathConstants<double>::twoPi * hz / 48000.0);
        double s1 = 0.0, s2 = 0.0;
        for (auto i = a; i < b; ++i)
        {
            const auto s0 = samples[i] + coefficient * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return (float) (2.0 * std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2)) / (double) (b - a));
    }
}

namespace
{
    // An album bank's rigs, in slot order.
    std::vector<Rig> bankRigs (int bank)
    {
        std::vector<Rig> rigs;
        for (const auto& rig : makeFactoryRigs())
            if (rig.bank == bank)
                rigs.push_back (rig);
        return rigs;
    }

    // What every album bank's rigs must do, measured on a plucked note.
    void checkAlbumBank (int bank, const juce::StringArray& songWords)
    {
        const auto rigs = bankRigs (bank);
        REQUIRE (rigs.size() == 15);

        // Named for the sound: no word of a song title on the record.
        for (const auto& rig : rigs)
        {
            INFO (rig.name);
            CHECK (rig.tags.contains ("Album"));
            CHECK ((rig.tags.contains ("Clean") || rig.tags.contains ("Lead") || rig.tags.contains ("FX")));
            for (const auto& word : juce::StringArray::fromTokens (rig.name, " -", {}))
                CHECK_FALSE (songWords.contains (word, true));
        }

        // Each rig's loudness while the note plays, the pedal where the rig
        // is meant to start (a volume swell at the toe, a whammy at the
        // heel). A fresh plugin for each: echoes and reverbs ring on after a
        // rig change (as pedals' trails do), so the last rig's tail would
        // be measured too.
        RigRig plain;
        plain.processor.loadRig (plain.entryNamed ("Clean Slate"));
        const auto dry = pluck (plain.processor);
        std::vector<float> levels;
        const auto measures = juce::SystemStats::getEnvironmentVariable ("PARAGON_RIG_MEASURES", {}).isNotEmpty();
        for (const auto& rig : rigs)
        {
            INFO (rig.name);
            RigRig r;
            r.processor.loadRig (r.entryNamed (rig.name));
            const auto target = (int) r.get (ParamIDs::expressionTarget);
            r.set (ParamIDs::expression, target == ExpressionTargets::volume ? 1.0f : 0.0f);
            const auto out = pluck (r.processor);
            const auto playing = rmsOf (out[0], 0.0, 1.0), tail = rmsOf (out[0], 2.0, 3.0);
            levels.push_back (dBOf (playing));
            if (measures)
                std::printf ("%-24s playing %6.1f dB  tail %6.1f dB\n", rig.name.toRawUTF8(), dBOf (playing), dBOf (tail) - dBOf (playing));
            REQUIRE (std::isfinite (playing));

            // Ambient rigs ring on: a second after the note, the space is still there.
            if (rig.tags.contains ("Ambient"))
                CHECK (dBOf (tail) - dBOf (playing) > -30.0f);
            // Dry rigs are dry: nothing after the note.
            if (rig.tags.contains ("Dry"))
                CHECK (dBOf (tail) - dBOf (playing) < -60.0f);
            // The effects rigs change the sound: against the same note with
            // every effect off, the difference is within 10 dB of the note.
            // (Touch rigs play only while the HAVOK pad is touched.)
            if (rig.tags.contains ("FX") && ! rig.tags.contains ("Touch"))
            {
                std::vector<float> difference (out[0].size());
                for (size_t n = 0; n < difference.size(); ++n)
                    difference[n] = out[0][n] - dry[0][n];
                CHECK (dBOf (rmsOf (difference, 0.1, 1.0)) - dBOf (rmsOf (dry[0], 0.1, 1.0)) > -10.0f);
            }
            // Octave rigs add the octave above (Sub: below): on a pure 220 Hz
            // note, that octave comes within 10 dB of the note itself.
            if (rig.tags.contains ("Octave") || rig.tags.contains ("Sub"))
            {
                RigRig o;
                o.processor.loadRig (o.entryNamed (rig.name));
                const auto tone = pluck (o.processor, 1.0, 220.0, true);
                const auto note = levelAtHz (tone[0], 220.0, 0.1, 0.6);
                const auto octave = levelAtHz (tone[0], rig.tags.contains ("Sub") ? 110.0 : 440.0, 0.1, 0.6);
                CAPTURE (dBOf (octave / note));
                CHECK (dBOf (octave / note) > -10.0f);
            }
        }

        // Balanced: switching between them doesn't jump in volume.
        auto sorted = levels;
        std::sort (sorted.begin(), sorted.end());
        const auto median = sorted[sorted.size() / 2];
        for (size_t n = 0; n < rigs.size(); ++n)
        {
            INFO (rigs[n].name << ": " << levels[n] << " dB, the bank's median " << median << " dB");
            CHECK (std::abs (levels[n] - median) < 6.0f);
        }
    }
}

TEST_CASE ("Bank F, an album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (5, { "Sun", "Killer", "Hurt", "Yellowjacket", "Summit", "Secret", "Garden", "Silk", "Strings",
                         "Halcyon", "Strange", "World", "Holy", "Roller", "Eternal", "Blue", "Constance", "Circle" });

    // The squeal: heel down your note, toe down an octave up (wet only).
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Riff Squeal Bend"));
    for (const auto toe : { false, true })
    {
        r.set (ParamIDs::expression, toe ? 1.0f : 0.0f);
        const auto out = pluck (r.processor, 1.0, 220.0, true);
        const auto at220 = levelAtHz (out[0], 220.0, 0.2, 0.7), at440 = levelAtHz (out[0], 440.0, 0.2, 0.7);
        CAPTURE (toe, dBOf (at220), dBOf (at440));
        CHECK ((toe ? at440 > 4.0f * at220 : at220 > 4.0f * at440));
    }
}

TEST_CASE ("Bank G, a second album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (6, { "Blind", "Ball", "Tongue", "Need", "To", "Clown", "Divine", "Faget", "Shoots", "And", "Ladders",
                         "Predictable", "Fake", "Lies", "Helmet", "In", "The", "Bush", "Daddy" });

    // The second guitar a bar behind, on the other side: a short note at
    // 120 BPM (a bar is 2 s) comes back 2 s later, louder on one side than
    // the other by 6 dB or more, and not much quieter than the note.
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Bar-Behind Bend Echo"));
    const auto out = pluck (r.processor, 0.3);
    const auto noteLeft = rmsOf (out[0], 0.0, 0.3), noteRight = rmsOf (out[1], 0.0, 0.3);
    const auto echoLeft = rmsOf (out[0], 2.0, 2.3), echoRight = rmsOf (out[1], 2.0, 2.3);
    const auto quietBefore = std::max (rmsOf (out[0], 1.0, 1.9), rmsOf (out[1], 1.0, 1.9));
    CAPTURE (dBOf (noteLeft), dBOf (noteRight), dBOf (echoLeft), dBOf (echoRight), dBOf (quietBefore));
    CHECK (std::abs (dBOf (echoLeft) - dBOf (echoRight)) > 6.0f);
    CHECK (dBOf (std::max (echoLeft, echoRight)) - dBOf (std::max (noteLeft, noteRight)) > -12.0f);
    CHECK (dBOf (quietBefore) < dBOf (std::max (echoLeft, echoRight)) - 20.0f);   // nothing in between: it's a bar later
}

TEST_CASE ("Bank H, a third album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (7, { "Papercut", "One", "Step", "Closer", "With", "You", "Points", "Of", "Authority", "Crawling", "Runaway",
                         "By", "Myself", "In", "The", "End", "A", "Place", "For", "My", "Head", "Forgotten", "Cure", "Itch",
                         "Pushing", "Me", "Away" });

    // The chopped riff really is chopped: on a phrase of changing notes,
    // the stutter edits change a good share of its 50 ms moments.
    const auto phrase = [] (ParagonX1AudioProcessor& processor)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 256;
        const double notes[] = { 110.0, 130.8, 146.8, 164.8, 196.0, 220.0, 196.0, 146.8 };
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (int start = 0; start < (int) (2.0 * rate); start += block)
        {
            buffer.clear();
            for (int i = 0; i < block; ++i)
            {
                const auto t = (start + i) / rate;
                const auto hz = notes[(size_t) (t / 0.25) % 8];
                buffer.setSample (0, i, (float) (0.2 * std::sin (juce::MathConstants<double>::twoPi * hz * t)));
            }
            processor.processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
        }
        return out;
    };
    RigRig plain, chopped;
    plain.processor.loadRig (plain.entryNamed ("Clean Slate"));
    chopped.processor.loadRig (chopped.entryNamed ("Chopped Grid Riff"));
    const auto a = phrase (plain.processor), b = phrase (chopped.processor);
    int changed = 0, windows = 0;
    for (size_t start = 4800; start + 2400 <= a.size(); start += 2400, ++windows)
    {
        double difference = 0.0, level = 0.0;
        for (size_t n = start; n < start + 2400; ++n)
        {
            difference += (double) (a[n] - b[n]) * (a[n] - b[n]);
            level += (double) a[n] * a[n];
        }
        if (difference > 0.1 * level)
            ++changed;
    }
    CAPTURE (changed, windows);
    CHECK (changed > windows * 15 / 100);
    CHECK (changed < windows * 90 / 100);
}

TEST_CASE ("Bank I, a fourth album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (8, { "The", "Price", "Is", "Wrong", "Motormouth", "Marigold", "Way", "News", "Goes", "Remain", "Indoors",
                         "Habitual", "Line", "Crosser", "Absolomb", "Catch", "Fire", "Flatline", "Prayer", "Position", "Lune" });

    // The dive: heel down your note, toe down six octaves (to a rumble).
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Dive Accent"));
    for (const auto toe : { false, true })
    {
        r.set (ParamIDs::expression, toe ? 1.0f : 0.0f);
        const auto out = pluck (r.processor, 1.0, 220.0, true);
        const auto at220 = levelAtHz (out[0], 220.0, 0.2, 0.7);
        const auto played = rmsOf (out[0], 0.2, 0.7);
        CAPTURE (toe, dBOf (at220), dBOf (played));
        if (toe)
            CHECK (at220 < 0.1f * played);              // the note itself is gone
        else
            CHECK (at220 > 0.8f * played * 1.41f);      // your note, untouched (a sine's peak is 1.41 x its RMS)
    }
}

TEST_CASE ("Bank J, a fifth album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (9, { "Hero", "Pieces", "Ugly", "Desertion", "Silence", "Shadows", "In", "Red", "Never", "Failure",
                         "See", "And", "Believe", "This", "Life", "The", "Last", "Song" });

    // The throw: a short note, then its echoes carry on clearly once it
    // stops (a quarter, 0.5 s, apart at 120 BPM), well above the same rig
    // with its DELAY off.
    const auto tailOf = [] (bool delayOn)
    {
        RigRig r;
        r.processor.loadRig (r.entryNamed ("Echo Throw Accent"));
        r.set (ParamIDs::delayOn, delayOn ? 1.0f : 0.0f);
        const auto out = pluck (r.processor, 0.3);
        return std::pair { rmsOf (out[0], 0.0, 0.3), rmsOf (out[0], 0.45, 1.5) };
    };
    const auto [note, tail] = tailOf (true);
    const auto [plainNote, plainTail] = tailOf (false);
    CAPTURE (dBOf (note), dBOf (tail), dBOf (plainTail));
    CHECK (dBOf (tail) - dBOf (note) > -15.0f);
    CHECK (dBOf (tail) - dBOf (plainTail) > 20.0f);
}

TEST_CASE ("Bank K, a sixth album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (10, { "Stinkfist", "Eulogy", "H", "Useful", "Idiot", "Forty", "Six", "2", "Message", "To", "Harry",
                          "Manback", "Hooker", "With", "A", "Penis", "Intermission", "Jimmy", "Die", "Eier", "Von", "Satan",
                          "Pushit", "Cesaro", "Summability", "Aenema", "Ions", "Third", "Eye" });

    // The march chop really chops: across a held note, the level in 10 ms
    // steps swings by more than 20 dB, sixteenths apart.
    RigRig r;
    r.processor.loadRig (r.entryNamed ("March Tremolo Chop"));
    const auto out = pluck (r.processor, 1.0, 220.0, true);
    float loudest = 0.0f, quietest = 1.0f;
    for (double t = 0.1; t < 0.6; t += 0.01)
    {
        const auto level = rmsOf (out[0], t, t + 0.01);
        loudest = std::max (loudest, level);
        quietest = std::min (quietest, level);
    }
    CAPTURE (dBOf (loudest), dBOf (quietest));
    CHECK (dBOf (loudest) - dBOf (quietest) > 20.0f);
}

TEST_CASE ("Bank L, a seventh album's guitar sounds: effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (11, { "People", "Of", "The", "Sun", "Bulls", "On", "Parade", "Vietnow", "Revolver", "Snakecharmer", "Tire",
                          "Me", "Down", "Rodeo", "Without", "A", "Face", "Wind", "Below", "Roll", "Right", "Year", "Tha", "Boomerang" });

    // The two-octave lead: a 220 Hz note comes out at 880 Hz, and the note
    // itself is gone (the shifter is wet only).
    RigRig r;
    r.processor.loadRig (r.entryNamed ("Two-Octave Stack Lead"));
    const auto out = pluck (r.processor, 1.0, 220.0, true);
    const auto at220 = levelAtHz (out[0], 220.0, 0.2, 0.7), at880 = levelAtHz (out[0], 880.0, 0.2, 0.7);
    CAPTURE (dBOf (at220), dBOf (at880));
    CHECK (at880 > 4.0f * at220);

    // The pedal rigs: heel your note, toe the interval (2 octaves up, 2 octaves down, a tone up).
    const std::tuple<const char*, double> pedals[] = { { "Two-Octave Pedal Shriek", 880.0 }, { "Siren Dive", 55.0 },
                                                       { "Whole-Step Pedal Bend", 246.94 } };
    for (const auto& [name, toeHz] : pedals)
    {
        INFO (name);
        for (const auto toe : { false, true })
        {
            RigRig p;   // a fresh plugin for each, the pedal already there
            p.processor.loadRig (p.entryNamed (name));
            p.set (ParamIDs::expression, toe ? 1.0f : 0.0f);
            const auto o = pluck (p.processor, 1.0, 220.0, true);
            const auto note = levelAtHz (o[0], 220.0, 0.25, 0.75), moved = levelAtHz (o[0], toeHz, 0.25, 0.75);
            CAPTURE (toe, dBOf (note), dBOf (moved));
            CHECK ((toe ? moved > 2.0f * note : note > 2.0f * moved));
        }
    }
}

TEST_CASE ("Bank M, an eighth album's guitar sounds: cleans, effects and leads that do what they say", "[rigs][album]")
{
    checkAlbumBank (12, { "Just", "Got", "Wicked", "She", "Said", "No", "One", "End", "Of", "The", "World", "Confession",
                          "It's", "All", "Good", "Send", "In", "Clowns", "Same", "Drug", "Anti", "Love", "Song", "Witch",
                          "Sick", "Man", "Outerspace", "Bleed" });
}
