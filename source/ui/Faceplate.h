#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Icons.h"

// The FACEPLATE: the selected block as a small unit of the PR7 line, beside
// its controls on the PROCESSOR page (where the RIG BROWSER and SELECTED
// RIG panels were), as on the PR7-V. Built only from the PR7's own parts,
// in the current finish: the unit's pearl chassis, its LCD screens, big
// knobs, small keys with light stripes, neon LED bars, and lettering
// stamped in ink or printed in red. It shows what the block is doing, live:
//
//   OD / DIST   the MODEL's name and what kind of pedal it is; its knobs as
//               the unit's big knobs (Skyline's ATTACK as eight small keys,
//               one lit, and its GATE; Abyss's HIGH, LOW, MIDDLE and MID
//               FREQ); the guitar's level going in, in LED bars.
//   NAMP        the loaded model's name and details on a screen (or that
//               none is loaded, or why it failed); RIG LEVEL and PRESENCE;
//               NORMALISE on a key; the level going in.
//   (every other block, for now: its name, icon and TYPE on a screen)
class Faceplate final : public juce::Component,
                        private juce::Timer,
                        private juce::ChangeListener
{
public:
    explicit Faceplate (ParagonX1AudioProcessor&);
    ~Faceplate() override;

    // Which kind of block: the ones with their own faceplates, or any other.
    enum class Unit { drive, amp, other };
    // What's drawn now (for tests).
    enum class Look { generic, drive, amp };

    // `onIds`: the block's on switches (comma separated); `typeId`: its TYPE
    // (or MODEL; or empty).
    void setBlock (Unit unit, const juce::String& title, Icons::Type icon, juce::Colour colour,
                   const juce::String& onIds, const juce::String& typeId);
    Look getLook() const;

    // Moves the meters one step towards the plugin's readings (the timer
    // does it 30 times a second; tests and screenshots call it directly).
    void updateMeters();
    float getShownInput() const noexcept { return shownInput; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // the finish changed
    float raw (const juce::String& id) const;
    float normalised (const juce::String& id) const;
    juce::String text (const juce::String& id) const;
    bool isOn() const;

    // Each draws its name in `header` and the rest in `body`.
    void paintGeneric (juce::Graphics&, juce::Rectangle<float> header, juce::Rectangle<float> body);
    void paintDrive (juce::Graphics&, juce::Rectangle<float> header, juce::Rectangle<float> body);
    void paintAmp (juce::Graphics&, juce::Rectangle<float> header, juce::Rectangle<float> body);
    void paintInputMeter (juce::Graphics&, juce::Rectangle<float> area);

    ParagonX1AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    Unit unit = Unit::other;
    juce::String title, typeId;
    juce::StringArray onIds;
    Icons::Type icon = Icons::Type::eq;
    juce::Colour colour;   // the block's colour in the chain
    juce::Image satin;     // the pearl's grain, as on the unit

    float shownInput = 0.0f;   // the input's peak, with a meter's fall
};
