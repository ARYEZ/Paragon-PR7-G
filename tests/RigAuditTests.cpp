// Hidden (run with "[rigaudit]"): for finding factory rigs that sound alike.
// Writes, into the folder in PARAGON_RIGAUDIT, one line per factory rig in
// rigs.txt (slot, name, tags and every setting it changes) and its sound
// for two phrases (a strummed chord, then a picked single-note riff with a
// second of silence for the tails), stereo, as 32-bit floats: <slot>.f32.

#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

namespace
{
    constexpr double auditRate = 48000.0;
    constexpr int auditBlock = 256;

    // The phrase: 0-2 s strums, 2-4 s a riff of single notes, 4-5 s silence.
    float phraseAt (double t)
    {
        if (t < 2.0)
        {
            const double notes[] = { 82.4, 110.0, 146.8, 196.0, 246.9, 329.6 };
            const auto pluck = std::fmod (t, 0.5);
            auto v = 0.0;
            for (const auto hz : notes)
                v += std::sin (juce::MathConstants<double>::twoPi * hz * t);
            return (float) (0.08 * std::exp (-4.0 * pluck) * v);
        }
        if (t < 4.0)
        {
            const double riff[] = { 82.4, 82.4, 98.0, 82.4, 110.0, 82.4, 123.5, 146.8 };
            const auto step = (int) ((t - 2.0) / 0.25);
            const auto into = std::fmod (t - 2.0, 0.25);
            const auto hz = riff[step % 8];
            auto v = 0.0;
            for (int h = 1; h <= 6; ++h)
                v += std::sin (juce::MathConstants<double>::twoPi * hz * h * into) / h;
            return (float) (0.3 * std::exp (-9.0 * into) * v);
        }
        return 0.0f;
    }
}

TEST_CASE ("Audit every factory rig's settings and sound", "[.][rigaudit]")
{
    const auto folder = juce::File (juce::SystemStats::getEnvironmentVariable ("PARAGON_RIGAUDIT", {}));
    REQUIRE (folder.isDirectory());
    const auto rigs = juce::File::createTempFile ("rigaudit");
    rigs.createDirectory();
    ParagonX1AudioProcessor processor;
    processor.useRigFolder (rigs);
    processor.setPlayConfigDetails (1, 2, auditRate, auditBlock);
    processor.prepareToPlay (auditRate, auditBlock);

    juce::String listing;
    for (const auto& entry : processor.getRigLibrary().getEntries())
    {
        if (! entry.isFactory)
            continue;
        const auto& rig = entry.rig;
        juce::String line = rig.getSlotName() + "\t" + rig.name + "\t" + rig.tags.joinIntoString (",") + "\t";
        for (const auto& [id, value] : rig.parameters)
            line << id << "=" << juce::String (value, 4) << ";";
        for (const auto& [slot, text] : rig.cipherEnvelopes)
            line << "env" << slot << "=" << juce::String::toHexString (text.hashCode()) << ";";
        if (rig.cipherSequence.isNotEmpty())
            line << "seq=" << juce::String::toHexString (rig.cipherSequence.hashCode()) << ";";
        if (rig.cipherMacros.isNotEmpty())
            line << "macros=" << juce::String::toHexString (rig.cipherMacros.hashCode()) << ";";
        listing << line << "\n";

        processor.loadRig (entry);
        processor.reset();
        juce::AudioBuffer<float> buffer (2, auditBlock);
        juce::MidiBuffer midi;
        juce::MemoryOutputStream out;
        for (int b = 0; b < (int) (5.0 * auditRate / auditBlock); ++b)
        {
            buffer.clear();
            for (int i = 0; i < auditBlock; ++i)
                buffer.setSample (0, i, phraseAt ((double) (b * auditBlock + i) / auditRate));
            processor.processBlock (buffer, midi);
            for (int i = 0; i < auditBlock; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const auto s = buffer.getSample (c, i);
                    out.write (&s, sizeof (float));
                }
        }
        folder.getChildFile (rig.getSlotName() + ".f32").replaceWithData (out.getData(), out.getDataSize());
    }
    folder.getChildFile ("rigs.txt").replaceWithText (listing);
    rigs.deleteRecursively();
}
