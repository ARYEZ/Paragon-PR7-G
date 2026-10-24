#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/GraphicEqPanel.h"
#include "ui/EqPresets.h"
#include "ui/Finish.h"
#include "ui/PerformanceControls.h"

using Catch::Matchers::WithinAbs;

namespace
{
    template <typename Type>
    void findAll (juce::Component& parent, std::vector<Type*>& found)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* match = dynamic_cast<Type*> (child))
                found.push_back (match);
            findAll (*child, found);
        }
    }

    // Every visible component sits inside its parent and has some size.
    void checkLayout (juce::Component& parent, const juce::String& path)
    {
        for (auto* child : parent.getChildren())
        {
            if (! child->isVisible() || dynamic_cast<juce::TooltipWindow*> (child) != nullptr)
                continue;

            const auto name = path + "/" + (child->getName().isNotEmpty() ? child->getName()
                                                                          : juce::String (typeid (*child).name()));
            INFO (name << " at " << child->getBounds().toString() << " in " << parent.getLocalBounds().toString());
            CHECK_FALSE (child->getBounds().isEmpty());
            CHECK (parent.getLocalBounds().expanded (1).contains (child->getBounds()));
            checkLayout (*child, name);
        }
    }

    std::vector<float> playThrough (ParagonX1AudioProcessor& processor, const std::vector<float>& input)
    {
        juce::AudioBuffer<float> buffer (2, (int) input.size());
        buffer.clear();
        std::copy (input.begin(), input.end(), buffer.getWritePointer (0));
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        return { buffer.getReadPointer (1), buffer.getReadPointer (1) + buffer.getNumSamples() };
    }

    void set (ParagonX1AudioProcessor& processor, const char* id, float plainValue)
    {
        auto* parameter = processor.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }
}

TEST_CASE ("The window lays out cleanly at every size", "[ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    for (const auto width : { 960, 1280, 1920 })
    {
        DYNAMIC_SECTION (width << " wide")
        {
            editor->setSize (width, width * 9 / 16);
            REQUIRE (editor->getWidth() == width);

            // The view is scaled, not re-laid-out: its transform matches the window.
            auto* view = editor->getChildComponent (0);
            REQUIRE (view != nullptr);
            CHECK_THAT (view->getTransform().mat00, WithinAbs ((float) width / (float) MainView::width, 0.001));

            checkLayout (*view, "view");
        }
    }
}

TEST_CASE ("PEARL BLACK recolours the unit's printing, and every open window follows", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> first (processor.createEditor()), second (processor.createEditor());

    // RIG LEVEL is printed on the unit: ink on white, light on black.
    const auto rigLevelColour = [] (juce::Component& editor)
    {
        std::vector<juce::Label*> labels;
        findAll (editor, labels);
        for (auto* label : labels)
            if (label->getText() == "RIG LEVEL")
                return label->findColour (juce::Label::textColourId);
        return juce::Colour();
    };
    CHECK (rigLevelColour (*first) == ParagonColours::ink);

    // Chosen in one window (here, without saving the preference), it shows in both.
    Finish::set (Finish::Type::pearlBlack, false);
    CHECK (Finish::isBlack());
    CHECK (rigLevelColour (*first) == Finish::ink());
    CHECK (rigLevelColour (*second) == Finish::ink());
    CHECK (Finish::ink().getPerceivedBrightness() > 0.8f);

    // Pearl white with neon keys: the white unit's ink, the black unit's keys.
    Finish::set (Finish::Type::pearlWhiteNeon, false);
    CHECK (! Finish::isBlack());
    CHECK (Finish::neonKeys());
    CHECK (rigLevelColour (*first) == ParagonColours::ink);

    // The key and knob drawing still works in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        first->setSize (1280, 720);
        const auto image = first->createComponentSnapshot (first->getLocalBounds());
        CHECK (image.isValid());
    }
    CHECK (rigLevelColour (*first) == ParagonColours::ink);
}

TEST_CASE ("The chain and footswitches pick which block's knobs show; the graphic EQ is the default", "[ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // The knob panels in signal-chain order: eleven EffectPanels, with the
    // graphic EQ's own panel as block 6 (after the cabinet).
    std::vector<EffectPanel*> effectPanels;
    findAll (*editor, effectPanels);
    REQUIRE (effectPanels.size() == 11);
    std::vector<GraphicEqPanel*> eqPanels;
    findAll (*editor, eqPanels);
    REQUIRE (eqPanels.size() == 1);
    std::vector<juce::Component*> panels (effectPanels.begin(), effectPanels.end());
    panels.insert (panels.begin() + 6, eqPanels[0]);

    const auto visiblePanel = [&]
    {
        int count = 0, index = -1;
        for (size_t i = 0; i < panels.size(); ++i)
            if (panels[i]->isVisible())
            {
                ++count;
                index = (int) i;
            }
        REQUIRE (count == 1);
        return index;
    };

    // To start with the dock shows the graphic EQ.
    CHECK (visiblePanel() == 6);

    std::vector<SignalChain*> chains;
    findAll (*editor, chains);
    REQUIRE (chains.size() == 1);
    for (int block = 0; block < 12; ++block)
    {
        chains[0]->onSelect (block);
        CHECK (visiblePanel() == block);
    }

    // Footswitch 3's name shows modulation (block 8); footswitch 4's shows delay (block 9).
    std::vector<Footswitch*> footswitches;
    findAll (*editor, footswitches);
    REQUIRE (footswitches.size() == 4);
    footswitches[2]->onSelect();
    CHECK (visiblePanel() == 8);
    footswitches[3]->onSelect();
    CHECK (visiblePanel() == 9);

    // The GRAPHIC EQ button on any other block's panel goes back to the EQ.
    std::vector<juce::TextButton*> buttons;
    findAll (*editor, buttons);
    const auto back = std::find_if (buttons.begin(), buttons.end(), [] (auto* b) { return b->getButtonText() == "GRAPHIC EQ"; });
    REQUIRE (back != buttons.end());
    CHECK ((*back)->isVisible());
    (*back)->onClick();
    CHECK (visiblePanel() == 6);
    CHECK_FALSE ((*back)->isVisible());
}

TEST_CASE ("BYPASS passes the clean guitar, on both channels", "[ui][bypass]")
{
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (1, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    // Make the processed sound very different from the input...
    set (processor, ParamIDs::driveOn, 1.0f);
    set (processor, ParamIDs::driveGain, 10.0f);
    set (processor, ParamIDs::inputGain, 12.0f);
    set (processor, ParamIDs::bypass, 1.0f);

    std::vector<float> input (512);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = 0.3f * std::sin (0.05f * (float) i);

    for (int block = 0; block < 4; ++block)
        playThrough (processor, input);   // let the 20 ms bypass fade finish (one block is ~11 ms)
    const auto output = playThrough (processor, input);
    for (size_t i = 0; i < input.size(); ++i)
        REQUIRE_THAT (output[i], WithinAbs (input[i], 1.0e-6));

    CHECK (processor.getBypassParameter() == processor.getParameters().getParameter (ParamIDs::bypass));
}

TEST_CASE ("BYPASS stays in time when the amp adds latency", "[ui][bypass]")
{
    // At 44.1 kHz a 48 kHz model is resampled, which adds latency. The
    // bypassed signal must be delayed by the same amount, or toggling bypass
    // would shift the guitar against the rest of the mix.
    ParagonX1AudioProcessor processor;
    processor.setPlayConfigDetails (1, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);
    processor.loadAmpModel (juce::File (PARAGON_NAM_EXAMPLE_MODELS).getChildFile ("wavenet.nam"));
    REQUIRE (processor.waitForLoads (20000));
    processor.prepareToPlay (44100.0, 512);
    const auto latency = processor.getLatencySamples();
    REQUIRE (latency > 0);

    set (processor, ParamIDs::bypass, 1.0f);
    for (int block = 0; block < 4; ++block)
        playThrough (processor, std::vector<float> (512, 0.0f));   // let the bypass fade finish

    std::vector<float> click (512, 0.0f);
    click[10] = 0.5f;
    const auto output = playThrough (processor, click);
    CHECK_THAT (output[(size_t) (10 + latency)], WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (output[10], WithinAbs (0.0, 1.0e-6));
}

TEST_CASE ("BYPASS is global: rigs don't change it and it doesn't mark them modified", "[ui][bypass]")
{
    ParagonX1AudioProcessor processor;
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ParagonX1UiTests").getChildFile (juce::Uuid().toString());
    processor.useRigFolder (folder);

    processor.loadRig (*processor.getRigLibrary().findFactory ("Studio Clean"));
    set (processor, ParamIDs::bypass, 1.0f);
    CHECK_FALSE (processor.isRigModified());

    processor.loadRig (*processor.getRigLibrary().findFactory ("Slapback"));
    CHECK (processor.getParameters().getRawParameterValue (ParamIDs::bypass)->load() > 0.5f);
    folder.deleteRecursively();
}

TEST_CASE ("The expression screen: click a line to assign the pedal, again to unassign; scroll to step", "[ui]")
{
    ParagonX1AudioProcessor processor;
    auto& state = processor.getParameters();
    ExpressionAssign screen (state, ExpressionAssign::Style::pedal);
    screen.setBounds (0, 0, 88, 101);   // about its size on the unit

    const auto target = [&] { return juce::roundToInt (state.getRawParameterValue (ParamIDs::expressionTarget)->load()); };
    const auto rowCentre = [&] (int line)
    {
        // The glass's content: the heading at the top, the position bar at
        // the bottom, the lines showing evenly between (as the screen lays them out).
        const auto glass = screen.getLocalBounds().toFloat().reduced (7.0f).reduced (5.0f, 4.0f);
        const auto top = glass.getY() + 15.0f, height = (glass.getHeight() - 25.0f) / (float) ExpressionAssign::visibleRows;
        return juce::Point<float> (glass.getCentreX(), top + height * ((float) line + 0.5f));
    };
    const auto click = [&] (int line)
    {
        const auto position = rowCentre (line);
        const auto now = juce::Time::getCurrentTime();
        screen.mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          &screen, &screen, now, position, now, 1, false));
    };
    const auto scroll = [&] (float deltaY)
    {
        juce::MouseWheelDetails wheel {};
        wheel.deltaY = deltaY;
        const auto now = juce::Time::getCurrentTime();
        screen.mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {}, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 &screen, &screen, now, {}, now, 0, false), wheel);
    };

    REQUIRE (target() == ExpressionTargets::off);

    click (1);
    CHECK (target() == ExpressionTargets::wah);
    click (4);
    CHECK (target() == ExpressionTargets::loFi);
    click (4);
    CHECK (target() == ExpressionTargets::off);

    // WHAMMY-WAH also switches the FILTER / PITCH block on as a Whammy-Wah.
    click (5);
    CHECK (target() == ExpressionTargets::whammyWah);
    CHECK (state.getRawParameterValue (ParamIDs::fpOn)->load() > 0.5f);
    CHECK (juce::roundToInt (state.getRawParameterValue (ParamIDs::fpType)->load()) == (int) effects::FilterPitch::Type::whammyWah);
    click (5);
    CHECK (target() == ExpressionTargets::off);

    // Scrolling down from unassigned starts at the top, then steps down; it stops at the ends.
    scroll (-1.0f);
    CHECK (target() == ExpressionTargets::volume);
    scroll (-1.0f);
    CHECK (target() == ExpressionTargets::wah);
    scroll (1.0f);
    scroll (1.0f);
    CHECK (target() == ExpressionTargets::volume);
    click (0);
    CHECK (target() == ExpressionTargets::off);

    // The list is longer than the screen: the down arrow (the heading's
    // right end) scrolls it, and the last line becomes TALK BOX, which also
    // switches FILTER / PITCH on as a Talk Box.
    const auto glass = screen.getLocalBounds().toFloat().reduced (7.0f).reduced (5.0f, 4.0f);
    const auto downArrow = juce::Point<float> (glass.getRight() - 5.0f, glass.getY() + 6.5f);
    for (int i = 0; i < 2; ++i)
    {
        const auto now = juce::Time::getCurrentTime();
        screen.mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), downArrow, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          &screen, &screen, now, downArrow, now, 1, false));
    }
    CHECK (target() == ExpressionTargets::off);   // scrolling doesn't choose anything
    click (ExpressionAssign::visibleRows - 1);
    CHECK (target() == ExpressionTargets::talkBox);
    CHECK (juce::roundToInt (state.getRawParameterValue (ParamIDs::fpType)->load()) == (int) effects::FilterPitch::Type::talkBox);
}

TEST_CASE ("EQ PRESETS: choosing one sets the faders and switches the EQ on; moving a fader lets go of the name", "[ui]")
{
    ParagonX1AudioProcessor processor;
    auto& state = processor.getParameters();
    GraphicEqPanel panel (state);
    panel.setBounds (0, 0, 520, 220);

    juce::ComboBox* menu = nullptr;
    for (auto* child : panel.getChildren())
        if (auto* box = dynamic_cast<juce::ComboBox*> (child))
            menu = box;
    REQUIRE (menu != nullptr);
    CHECK (menu->getSelectedItemIndex() == -1);   // a flat EQ isn't one of the presets

    const auto value = [&] (const juce::String& id) { return state.getRawParameterValue (id)->load(); };
    const auto& scoop = EqPresets::all[0];
    menu->setSelectedItemIndex (0, juce::sendNotificationSync);

    for (int band = 0; band < effects::GraphicEq::numBands; ++band)
        CHECK_THAT (value (ParamIDs::eqBands[band]), WithinAbs (scoop.gainsDb[(size_t) band], 0.05));
    CHECK_THAT (value (ParamIDs::eqLevel), WithinAbs (scoop.levelDb, 0.05));
    CHECK (value (ParamIDs::eqOn) > 0.5f);
    CHECK (menu->getText() == juce::String (scoop.name));

    // Nudge 1 kHz: it's no longer the preset, so the menu says PRESETS again.
    auto* band = state.getParameter (ParamIDs::eqBands[4]);
    band->setValueNotifyingHost (band->convertTo0to1 (0.0f));
    CHECK (menu->getSelectedItemIndex() == -1);
}
