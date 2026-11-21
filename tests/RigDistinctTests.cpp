// No two factory rigs may be (nearly) the same sound under different names.
//
// Two rigs' settings are compared knob by knob: how far each setting is
// apart as a share of its knob's travel (0 to 1), added up. A different
// TYPE, MODEL or switch counts as 1 (a different sound, however small its
// number), and so does any difference in CIPHER's drawn envelopes, steps or
// macros. RIG LEVEL doesn't count: louder isn't a different sound. Factory
// rigs keep the player's amp and cabinet, so on any rig two factory rigs
// differ by exactly these settings.
//
// 0.92.0's audit found 17 groups of rigs (three exact copies, the rest a
// knob nudged by a tenth or so, adding to 0.3 or less) that sounded alike;
// they were made distinct, and this keeps it so.

#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

#include <set>

namespace
{
    constexpr float closest = 0.25f;   // below this (a quarter of one knob's travel in all), two rigs count as one sound

    float settingsDistance (juce::AudioProcessor& processor, const Rig& a, const Rig& b)
    {
        std::set<juce::String> ids;
        for (const auto& [id, value] : a.parameters)
            ids.insert (id);
        for (const auto& [id, value] : b.parameters)
            ids.insert (id);
        ids.erase (ParamIDs::rigLevel);

        float total = 0.0f;
        for (const auto& id : ids)
        {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                [&]() -> juce::AudioProcessorParameter*
                {
                    for (auto* p : processor.getParameters())
                        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() == id)
                            return p;
                    return nullptr;
                }());
            if (parameter == nullptr)
            {
                total += 1.0f;   // an ID the plugin doesn't know: never treated as the same
                continue;
            }
            const auto valueOf = [&] (const Rig& rig)
            {
                const auto found = rig.parameters.find (id);
                return found != rig.parameters.end() ? found->second : parameter->convertFrom0to1 (parameter->getDefaultValue());
            };
            const auto va = valueOf (a), vb = valueOf (b);
            if (va == vb)
                continue;
            const auto stepped = dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr
                              || dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr
                              || dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr;
            total += stepped ? 1.0f : std::abs (parameter->convertTo0to1 (va) - parameter->convertTo0to1 (vb));
        }
        if (a.cipherEnvelopes != b.cipherEnvelopes || a.cipherSequence != b.cipherSequence
            || a.cipherMacros != b.cipherMacros)
            total += 1.0f;
        return total;
    }
}

TEST_CASE ("No two factory rigs are nearly the same settings under different names", "[rigs]")
{
    const auto folder = juce::File::createTempFile ("rigdistinct");
    folder.createDirectory();
    ParagonX1AudioProcessor processor;
    processor.useRigFolder (folder);

    std::vector<const Rig*> rigs;
    for (const auto& entry : processor.getRigLibrary().getEntries())
        if (entry.isFactory)
            rigs.push_back (&entry.rig);
    REQUIRE (rigs.size() > 100);

    const auto print = juce::SystemStats::getEnvironmentVariable ("PARAGON_RIG_DISTANCES", {}).isNotEmpty();
    std::vector<std::tuple<float, juce::String, juce::String>> nearest;
    for (size_t i = 0; i < rigs.size(); ++i)
        for (size_t j = i + 1; j < rigs.size(); ++j)
        {
            const auto distance = settingsDistance (processor, *rigs[i], *rigs[j]);
            const auto names = rigs[i]->getSlotName() + " " + rigs[i]->name + " / " + rigs[j]->getSlotName() + " " + rigs[j]->name;
            if (print)
                nearest.emplace_back (distance, names, juce::String());
            INFO (names << ": " << distance);
            CHECK (distance >= closest);
        }
    if (print)
    {
        std::sort (nearest.begin(), nearest.end());
        for (size_t k = 0; k < std::min<size_t> (30, nearest.size()); ++k)
            std::printf ("%5.2f  %s\n", std::get<0> (nearest[k]), std::get<1> (nearest[k]).toRawUTF8());
    }
    folder.deleteRecursively();
}
