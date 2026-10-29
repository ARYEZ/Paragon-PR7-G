#include "Rig.h"

#include "Parameters.h"
#include "dsp/effects/Modulation.h"

namespace
{
    // Bump when the file layout changes in a way older code can't read.
    constexpr int fileVersion = 1;

    // Which plugin version's parameters the rig was saved with, for the
    // occasional parameter whose default changed (older plugins ignore it).
    // 10: the drive's MODEL, defaulting to the Tube Screamer model.
    // 15: the reverb's TYPE, defaulting to HALL.
    constexpr int parameterVersion = 15;

    const juce::Identifier rigTag { "ParagonRig" };
    const juce::Identifier parameterTag { "Param" };
}

juce::String Rig::bankName (int bankIndex)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, numBanks - 1, bankIndex)));
}

juce::String Rig::getSlotName() const
{
    return bankName (bank) + juce::String (number).paddedLeft ('0', 2);
}

std::unique_ptr<juce::XmlElement> Rig::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> (rigTag);
    xml->setAttribute ("version", fileVersion);
    xml->setAttribute ("parameterVersion", parameterVersion);
    xml->setAttribute ("name", name);
    xml->setAttribute ("bank", bankName (bank));
    xml->setAttribute ("number", number);
    xml->setAttribute ("favourite", favourite);
    xml->setAttribute ("tags", tags.joinIntoString (","));
    xml->setAttribute ("ampModel", ampModelPath);
    xml->setAttribute ("cabIr", cabIrPath);
    if (keepAmpAndCab)
        xml->setAttribute ("keepAmpAndCab", true);

    for (const auto& [id, value] : parameters)
    {
        auto* parameter = xml->createNewChildElement (parameterTag);
        parameter->setAttribute ("id", id);
        parameter->setAttribute ("value", value);
    }
    return xml;
}

void Rig::convertOldRotary (std::map<juce::String, float>& parameters, const ParamIDs::ModIds& ids)
{
    const auto found = [&parameters] (const char* id) { return parameters.find (id) != parameters.end(); };
    if (found (ids.cycFast) || ! found (ids.type)
        || juce::roundToInt (parameters[ids.type]) != (int) effects::Modulation::Type::rotary)
        return;
    const auto valueOr = [&] (const char* id, float otherwise) { return found (id) ? parameters[id] : otherwise; };
    const auto cyclone = effects::Cyclone::fromOldRotary (valueOr (ids.rate, 0.8f), valueOr (ids.depth, 0.5f), valueOr (ids.mix, 0.5f));
    parameters[ids.cycFast] = cyclone.fast ? 1.0f : 0.0f;
    parameters[ids.cycFastSpeed] = cyclone.fastHz;
    parameters[ids.cycSlowSpeed] = cyclone.slowHz;
    parameters[ids.cycLow] = parameters[ids.cycHigh] = cyclone.mixDist;
    parameters[ids.cycMode] = (float) cyclone.mode;
}

std::optional<Rig> Rig::fromXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (rigTag) || xml.getIntAttribute ("version", 0) > fileVersion)
        return std::nullopt;

    Rig rig;
    rig.name = xml.getStringAttribute ("name", "Untitled").trim();
    if (rig.name.isEmpty())
        rig.name = "Untitled";

    const auto bankLetter = xml.getStringAttribute ("bank", "A").toUpperCase();
    rig.bank = bankLetter.isEmpty() ? 0 : juce::jlimit (0, numBanks - 1, (int) (bankLetter[0] - 'A'));
    rig.number = juce::jlimit (1, maxNumber, xml.getIntAttribute ("number", 1));
    rig.favourite = xml.getBoolAttribute ("favourite");
    rig.tags = juce::StringArray::fromTokens (xml.getStringAttribute ("tags"), ",", {});
    rig.tags.trim();
    rig.tags.removeEmptyStrings();
    rig.ampModelPath = xml.getStringAttribute ("ampModel");
    rig.cabIrPath = xml.getStringAttribute ("cabIr");
    rig.keepAmpAndCab = xml.getBoolAttribute ("keepAmpAndCab");

    for (const auto* parameter : xml.getChildWithTagNameIterator (parameterTag))
        if (const auto id = parameter->getStringAttribute ("id"); id.isNotEmpty())
            rig.parameters[id] = (float) parameter->getDoubleAttribute ("value");

    // Saved before the drive had a MODEL: it was the original overdrive,
    // model 0. Without this it would load as the newer default.
    const auto savedWith = xml.getIntAttribute ("parameterVersion", 0);
    if (savedWith < 10 && rig.parameters.count ("driveModel") == 0)
        rig.parameters["driveModel"] = 0.0f;
    // Saved before the reverb had a TYPE: it was the original, now CLASSIC (0).
    if (savedWith < 15 && rig.parameters.count ("reverbType") == 0)
        rig.parameters["reverbType"] = 0.0f;
    // Saved with the original rotary speaker: Cyclone, set as close to it as it goes.
    for (const auto* ids : { &ParamIDs::modIds, &ParamIDs::mod2Ids })
        convertOldRotary (rig.parameters, *ids);

    return rig;
}
