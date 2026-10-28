#include <map>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/GraphicEqPanel.h"
#include "ui/EffectPanel.h"
#include "ui/EqPresets.h"
#include "ui/Faceplate.h"
#include "ui/Finish.h"
#include "ui/PerformanceControls.h"
#include "ui/RackView.h"
#include "ui/SignalChain.h"

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

TEST_CASE ("HIDE RACK takes the rack away and shortens the window; a block clicked in the chain brings it back", "[ui][rack]")
{
    RackView::setShown (true, false);
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1280, 720);
    auto* view = editor->getChildComponent (0);
    REQUIRE (view != nullptr);

    auto* rackButton = dynamic_cast<juce::TextButton*> (view->findChildWithID ("rackButton"));
    REQUIRE (rackButton != nullptr);
    std::vector<Faceplate*> faceplates;
    findAll (*editor, faceplates);
    REQUIRE (faceplates.size() == 1);
    std::vector<GraphicEqPanel*> eqPanels;
    findAll (*editor, eqPanels);
    REQUIRE (eqPanels.size() == 1);
    std::vector<EffectPanel*> effectPanels;
    findAll (*editor, effectPanels);
    std::vector<SignalChain*> chains;
    findAll (*editor, chains);
    REQUIRE (chains.size() == 1);
    const auto anyPanelShowing = [&]
    {
        return eqPanels[0]->isVisible()
            || std::any_of (effectPanels.begin(), effectPanels.end(), [] (auto* panel) { return panel->isVisible(); });
    };

    CHECK (rackButton->getButtonText() == "HIDE RACK");
    CHECK (faceplates[0]->isVisible());
    CHECK (anyPanelShowing());

    // Hidden: the faceplate and every block's controls go, the chain moves
    // up under the floor unit and the window keeps its width but gets
    // shorter by the rack's height (the view keeps its scale).
    rackButton->onClick();
    CHECK_FALSE (RackView::isShown());
    CHECK (rackButton->getButtonText() == "SHOW RACK");
    CHECK_FALSE (faceplates[0]->isVisible());
    CHECK_FALSE (anyPanelShowing());
    CHECK (view->getHeight() == MainView::height - MainView::rackHeight);
    CHECK (editor->getWidth() == 1280);
    CHECK (editor->getHeight() == juce::roundToInt (1280.0f * (float) view->getHeight() / (float) MainView::width));
    CHECK_THAT (view->getTransform().mat00, WithinAbs (1280.0f / (float) MainView::width, 0.001));
    CHECK (chains[0]->getBottom() <= view->getHeight());
    checkLayout (*view, "view");

    // Dragging the window keeps the shorter shape.
    auto* constrainer = editor->getConstrainer();
    REQUIRE (constrainer != nullptr);
    CHECK_THAT (constrainer->getFixedAspectRatio(), WithinAbs ((double) MainView::width / (double) view->getHeight(), 0.0001));

    // The other pages fit in the shorter window too.
    for (const auto* tabId : { "presetsTab", "settingsTab", "processorTab" })
    {
        auto* tab = dynamic_cast<juce::Button*> (view->findChildWithID (tabId));
        REQUIRE (tab != nullptr);
        tab->onClick();
        // (Only the top level: the rig list scrolls, so its contents are taller than it.)
        for (auto* child : view->getChildren())
            if (child->isVisible())
            {
                INFO (tabId << ": " << typeid (*child).name() << " at " << child->getBounds().toString());
                CHECK (view->getLocalBounds().contains (child->getBounds()));
                if (child != chains[0] && child != rackButton)
                    CHECK (child->getBottom() <= chains[0]->getY());   // nothing runs into the chain
            }
    }
    CHECK_FALSE (RackView::isShown());

    // Clicking a block in the chain opens the rack on that block, at full height again.
    chains[0]->onSelect (3);
    CHECK (RackView::isShown());
    CHECK (rackButton->getButtonText() == "HIDE RACK");
    CHECK (faceplates[0]->isVisible());
    CHECK (effectPanels[3]->isVisible());
    CHECK (view->getHeight() == MainView::height);
    CHECK (editor->getWidth() == 1280);
    CHECK (editor->getHeight() == 720);
    checkLayout (*view, "view");

    // And the button brings it back too.
    rackButton->onClick();
    rackButton->onClick();
    CHECK (RackView::isShown());
    CHECK (editor->getHeight() == 720);
    CHECK (effectPanels[3]->isVisible());
}

TEST_CASE ("HIDE RACK / SHOW RACK: each new window size already fits the shape and limits the DAW checks it against", "[ui][rack]")
{
    // A DAW like Cubase checks every size the plugin asks for against the
    // plugin's limits and shape at that moment, and "corrects" it if it
    // doesn't fit. This listener does the same check on every resize.
    struct HostCheck final : juce::ComponentListener
    {
        void componentMovedOrResized (juce::Component& component, bool, bool wasResized) override
        {
            if (! wasResized)
                return;
            auto& editor = static_cast<juce::AudioProcessorEditor&> (component);
            const auto* constrainer = editor.getConstrainer();
            const auto width = editor.getWidth(), height = editor.getHeight();
            const auto fits = width >= constrainer->getMinimumWidth() && width <= constrainer->getMaximumWidth()
                           && height >= constrainer->getMinimumHeight() && height <= constrainer->getMaximumHeight()
                           && std::abs ((double) width / constrainer->getFixedAspectRatio() - (double) height) <= 1.0;
            if (! fits)
                misfits.add (juce::String (width) + " x " + juce::String (height));
        }
        juce::StringArray misfits;
    };

    RackView::setShown (true, false);
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (1706, 959);   // the opening size on a 1080p screen
    HostCheck host;
    editor->addComponentListener (&host);
    auto* rackButton = dynamic_cast<juce::TextButton*> (editor->getChildComponent (0)->findChildWithID ("rackButton"));
    REQUIRE (rackButton != nullptr);

    rackButton->onClick();   // hide
    CHECK (editor->getWidth() == 1706);
    CHECK (editor->getHeight() == juce::roundToInt (1706.0f * (float) (MainView::height - MainView::rackHeight) / (float) MainView::width));
    rackButton->onClick();   // show: back to the size it had (to the pixel: 1706 x 9 / 16 is 959.6)
    CHECK (editor->getWidth() == 1706);
    CHECK (std::abs (editor->getHeight() - 959) <= 1);

    // A width dragged to while hidden carries over to the shown window.
    rackButton->onClick();
    editor->setSize (1280, juce::roundToInt (1280.0f * (float) (MainView::height - MainView::rackHeight) / (float) MainView::width));
    rackButton->onClick();
    CHECK (editor->getWidth() == 1280);
    CHECK (editor->getHeight() == 720);

    INFO ("sizes the DAW would have changed: " << host.misfits.joinIntoString (", "));
    CHECK (host.misfits.isEmpty());
    editor->removeComponentListener (&host);
    RackView::setShown (true, false);
}

TEST_CASE ("SHOW RACK never runs the window off the bottom of the screen", "[ui][rack]")
{
    // The DAW keeps the window's top left corner where it is, so a window
    // placed low down grows downwards: SHOW RACK makes it narrower instead,
    // just enough to fit. (Pictured on a 1080p screen with a 40-pixel taskbar.)
    using Editor = ParagonX1AudioProcessorEditor;
    const juce::Rectangle<int> screen (0, 0, 1920, 1040);
    const auto shown = MainView::heightFor (true), hidden = MainView::heightFor (false);

    // Cubase's window, as opened with the rack hidden: the plugin's top at 207.
    const auto width = Editor::widthToFit (1706, shown, screen, 207);
    CHECK (width < 1706);
    CHECK (207 + width * shown / MainView::width <= 1040 - 16);
    CHECK (207 + (width + 4) * shown / MainView::width > 1040 - 16);   // and no narrower than it needs to be (to a few pixels)

    // Near the top of the screen there's room: the width chosen.
    CHECK (Editor::widthToFit (1706, shown, screen, 60) == 1706);
    // The shorter, rack-hidden window fits where the tall one doesn't.
    CHECK (Editor::widthToFit (1706, hidden, screen, 207) == 1706);
    // Never smaller than the smallest window, or bigger than the biggest.
    CHECK (Editor::widthToFit (1706, shown, screen, 900) == 960);
    CHECK (Editor::widthToFit (2400, hidden, screen, 0) == 1920);
}

TEST_CASE ("The window opens as big as fits above the taskbar, up to 1824 x 1026", "[ui]")
{
    using Editor = ParagonX1AudioProcessorEditor;
    // 1080p with a 40-pixel taskbar: room left for the DAW's title bar and toolbar.
    const auto fullHd = Editor::openingSize ({ 1920, 1040 });
    CHECK (fullHd == juce::Point<int> (1706, 959));
    CHECK (fullHd.y + 80 <= 1040);
    CHECK (Editor::openingSize ({ 2560, 1400 }) == juce::Point<int> (1824, 1026));
    const auto laptop = Editor::openingSize ({ 1366, 728 });
    CHECK (laptop.x <= 1366 - 40);
    CHECK (laptop.y + 80 <= 728);
    CHECK (Editor::openingSize ({ 800, 600 }).x == 960);   // never below the smallest size
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

    // The knob panels in signal-chain order: twelve EffectPanels (MOD 2
    // after MOD), with the graphic EQ's own panel as block 6 (after the cabinet).
    std::vector<EffectPanel*> effectPanels;
    findAll (*editor, effectPanels);
    REQUIRE (effectPanels.size() == 12);
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
    for (int block = 0; block < 13; ++block)
    {
        chains[0]->onSelect (block);
        CHECK (visiblePanel() == block);
    }

    // Footswitch 3's name shows modulation (block 8); footswitch 4's shows delay (block 10, after MOD 2).
    std::vector<Footswitch*> footswitches;
    findAll (*editor, footswitches);
    REQUIRE (footswitches.size() == 4);
    footswitches[2]->onSelect();
    CHECK (visiblePanel() == 8);
    footswitches[3]->onSelect();
    CHECK (visiblePanel() == 10);

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
    ExpressionAssign screen (state);
    screen.setBounds (0, 0, 272, 160);   // about its size in the EXPRESSION ASSIGN panel
    const auto k = 160.0f / 126.0f;      // its lettering's scale at that height
    // The glass's content: right of it the scroll buttons, inside it the frame.
    const auto glass = screen.getLocalBounds().toFloat().withTrimmedRight (38.0f).reduced (7.0f).reduced (5.0f * k, 4.0f * k);

    const auto target = [&] { return juce::roundToInt (state.getRawParameterValue (ParamIDs::expressionTarget)->load()); };
    const auto rowCentre = [&] (int line)
    {
        // The heading at the top, the position bar at the bottom, the lines
        // showing evenly between (as the screen lays them out).
        const auto top = glass.getY() + 15.0f * k, height = (glass.getHeight() - 25.0f * k) / (float) ExpressionAssign::visibleRows;
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
    const auto downArrow = juce::Point<float> (glass.getRight() - 5.0f * k, glass.getY() + 6.5f * k);
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

TEST_CASE ("The dock's expression screen scrolls with the buttons beside it", "[ui]")
{
    ParagonX1AudioProcessor processor;
    ExpressionAssign screen (processor.getParameters());
    screen.setBounds (0, 0, 272, 160);   // about its size in the EXPRESSION ASSIGN panel

    std::vector<juce::Button*> buttons;
    findAll (screen, buttons);
    REQUIRE (buttons.size() == 2);
    auto* up = buttons[0];
    auto* down = buttons[1];
    // Both inside the component, to the right of the screen.
    for (auto* button : buttons)
    {
        CHECK (screen.getLocalBounds().contains (button->getBounds()));
        CHECK (button->getX() > screen.getWidth() / 2);
    }

    // At the top: only down works. Two steps reach the end of the eight-line list.
    CHECK (! up->isEnabled());
    CHECK (down->isEnabled());
    for (int i = 0; i < ExpressionAssign::numTargets - ExpressionAssign::visibleRows; ++i)
        down->onClick();
    CHECK (! down->isEnabled());
    CHECK (up->isEnabled());

    // Scrolling never assigns the pedal.
    CHECK (juce::roundToInt (processor.getParameters().getRawParameterValue (ParamIDs::expressionTarget)->load()) == ExpressionTargets::off);
}

TEST_CASE ("The GRAPHIC EQ's fader lights are lit like the unit's keys while the EQ is on", "[ui]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    std::vector<GraphicEqPanel*> eqPanels;
    findAll (*editor, eqPanels);
    REQUIRE (eqPanels.size() == 1);
    std::vector<juce::Slider*> faders;
    findAll (*eqPanels[0], faders);
    faders.erase (std::remove_if (faders.begin(), faders.end(),
                                  [] (auto* s) { return ! (bool) s->getProperties()[ParagonLookAndFeel::eqFaderProperty]; }),
                  faders.end());
    REQUIRE (faders.size() == 10);   // nine bands and LEVEL

    const auto litCount = [&]
    {
        int lit = 0;
        for (auto* fader : faders)
        {
            REQUIRE (fader->getProperties().contains (ParagonLookAndFeel::eqFaderLitProperty));
            lit += (bool) fader->getProperties()[ParagonLookAndFeel::eqFaderLitProperty] ? 1 : 0;
        }
        return lit;
    };

    // Switched from the parameter (the DAW, a rig), not only by a click.
    set (processor, ParamIDs::eqOn, 0.0f);
    CHECK (litCount() == 0);
    set (processor, ParamIDs::eqOn, 1.0f);
    CHECK (litCount() == 10);
    set (processor, ParamIDs::eqOn, 0.0f);
    CHECK (litCount() == 0);

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

TEST_CASE ("The FACEPLATE shows the selected block, and the guitar's level going in", "[ui][faceplate]")
{
    ParagonX1AudioProcessor processor;
    constexpr double rate = 48000.0;
    constexpr int blockSize = 256;
    processor.setPlayConfigDetails (1, 2, rate, blockSize);
    processor.prepareToPlay (rate, blockSize);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    std::vector<Faceplate*> faceplates;
    findAll (*editor, faceplates);
    REQUIRE (faceplates.size() == 1);
    auto& faceplate = *faceplates[0];
    std::vector<SignalChain*> chains;
    findAll (*editor, chains);
    REQUIRE (chains.size() == 1);

    // Every block from the signal chain gets its own faceplate.
    using Look = Faceplate::Look;
    const Look looks[] { Look::pitch, Look::classic, Look::filterPitch, Look::drive, Look::amp, Look::cabinet, Look::eq,
                         Look::glitch, Look::modulation, Look::modulation, Look::delay, Look::reverb, Look::loFi };
    static_assert (std::size (looks) == 13);
    for (int block = 0; block < 13; ++block)
    {
        chains[0]->onSelect (block);
        CAPTURE (block);
        CHECK (faceplate.getLook() == looks[block]);
        CHECK (faceplate.createComponentSnapshot (faceplate.getLocalBounds()).isValid());
    }

    // COMP / GATE follows its TYPE: Daybreak has its own faceplate.
    chains[0]->onSelect (1);
    set (processor, ParamIDs::compType, 1.0f);
    CHECK (faceplate.getLook() == Faceplate::Look::daybreak);
    set (processor, ParamIDs::compType, 0.0f);
    CHECK (faceplate.getLook() == Faceplate::Look::classic);

    // Every delay and reverb TYPE draws (Spring and Reverse have their own shapes).
    chains[0]->onSelect (10);
    for (int type = 0; type < effects::Delay::typeNames.size(); ++type)
    {
        set (processor, ParamIDs::delayType, (float) type);
        CHECK (faceplate.createComponentSnapshot (faceplate.getLocalBounds()).isValid());
    }
    chains[0]->onSelect (11);
    for (int type = 0; type < effects::Reverb::typeNames.size(); ++type)
    {
        set (processor, ParamIDs::reverbType, (float) type);
        CHECK (faceplate.createComponentSnapshot (faceplate.getLocalBounds()).isValid());
    }

    // And every TYPE (or MODE) of the stage 3 blocks: MOD and MOD 2 each
    // show their own block's TYPE.
    const auto paintsEvery = [&] (int block, const char* id, int count)
    {
        chains[0]->onSelect (block);
        for (int type = 0; type < count; ++type)
        {
            CAPTURE (block, type);
            set (processor, id, (float) type);
            CHECK (faceplate.createComponentSnapshot (faceplate.getLocalBounds()).isValid());
        }
    };
    paintsEvery (8, ParamIDs::modType, effects::Modulation::typeNames.size());
    paintsEvery (9, ParamIDs::mod2Type, effects::Modulation::typeNames.size());
    paintsEvery (2, ParamIDs::fpType, effects::FilterPitch::typeNames.size());
    paintsEvery (7, ParamIDs::glitchMode, effects::Glitch::modeNames.size());
    paintsEvery (12, ParamIDs::lofiType, effects::LoFi::typeNames.size());

    // Every drive MODEL draws on the same faceplate (each paints without trouble).
    chains[0]->onSelect (3);
    for (int model = 0; model < effects::Drive::modelNames.size(); ++model)
    {
        set (processor, ParamIDs::driveModel, (float) model);
        CHECK (faceplate.getLook() == Faceplate::Look::drive);
        CHECK (faceplate.createComponentSnapshot (faceplate.getLocalBounds()).isValid());
    }

    // Playing: the INPUT meter rises with the guitar, then falls back in silence.
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int b = 0, n = 0; b < 20; ++b)
    {
        buffer.clear();
        for (int i = 0; i < blockSize; ++i, ++n)
            buffer.setSample (0, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) n / (float) rate));
        processor.processBlock (buffer, midi);
    }
    faceplate.updateMeters();
    CHECK (faceplate.getShownInput() > 0.1f);
    for (int tick = 0; tick < 60; ++tick)
        faceplate.updateMeters();
    CHECK (faceplate.getShownInput() < 0.01f);
}

TEST_CASE ("The rack: every block's display, real controls at full size and EXPRESSION ASSIGN on one unit", "[ui][faceplate][rack]")
{
    ParagonX1AudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    std::vector<Faceplate*> faceplates;
    findAll (*editor, faceplates);
    REQUIRE (faceplates.size() == 1);
    auto& rack = *faceplates[0];
    REQUIRE (rack.isRack());
    std::vector<SignalChain*> chains;
    findAll (*editor, chains);
    REQUIRE (chains.size() == 1);
    std::vector<EffectPanel*> effectPanels;
    findAll (*editor, effectPanels);
    std::vector<GraphicEqPanel*> eqPanels;
    findAll (*editor, eqPanels);
    REQUIRE (eqPanels.size() == 1);
    const auto rackBounds = rack.getBounds();

    // EXPRESSION ASSIGN at the rack's right end, on the same unit.
    std::vector<ExpressionPanel*> expression;
    findAll (*editor, expression);
    REQUIRE (expression.size() == 1);
    CHECK (expression[0]->getBounds() == rack.getSideArea() + rack.getPosition());
    CHECK (expression[0]->getX() > rack.getControlArea().getRight() + rack.getX());

    // Every parameter's value, to see what a control changed.
    auto& parameters = processor.getParameters();
    const auto snapshot = [&parameters]
    {
        std::map<juce::String, float> values;
        for (auto* parameter : parameters.processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                values[withId->paramID] = parameter->getValue();
        return values;
    };
    const auto changedSomething = [&] (const std::map<juce::String, float>& before)
    {
        for (const auto& [id, value] : snapshot())
            if (std::abs (value - before.at (id)) > 1.0e-6f)
                return true;
        return false;
    };

    for (int block = 0; block < 13; ++block)
    {
        CAPTURE (block);
        chains[0]->onSelect (block);

        // Exactly one set of controls showing, on the rack's face, between
        // its display and EXPRESSION ASSIGN.
        EffectPanel* panel = nullptr;
        int count = 0;
        for (auto* candidate : effectPanels)
            if (candidate->isVisible())
            {
                CHECK (candidate->isRack());
                panel = candidate;
                ++count;
            }
        if (eqPanels[0]->isVisible())
            ++count;
        REQUIRE (count == 1);
        if (panel == nullptr)
        {
            // The GRAPHIC EQ: its own rack layout, on the face at full size,
            // its faders and LEVEL the real ones.
            CHECK (block == 6);
            auto& eq = *eqPanels[0];
            CHECK (eq.isRack());
            CHECK (eq.getTransform().isIdentity());
            CHECK (eq.getBounds() == rack.getControlArea() + rack.getPosition());
            CHECK (eq.getRight() <= expression[0]->getX());
            std::vector<juce::Slider*> faders;
            findAll (eq, faders);
            REQUIRE (faders.size() == 10);   // nine bands and LEVEL
            std::sort (faders.begin(), faders.end(), [] (auto* a, auto* b) { return a->getX() < b->getX(); });
            for (size_t i = 0; i < faders.size(); ++i)
            {
                CAPTURE (i);
                CHECK (faders[i]->getHeight() >= 100);   // a full-length fader, not shrunk
                CHECK (eq.getLocalBounds().contains (faders[i]->getBounds()));
                const auto id = i < 9 ? ParamIDs::eqBands[(int) i] : juce::String (ParamIDs::eqLevel);
                faders[i]->setValue (-5.0, juce::sendNotificationSync);
                CHECK_THAT (processor.getParameters().getRawParameterValue (id)->load(), WithinAbs (-5.0, 0.05));
            }
            std::vector<juce::ToggleButton*> keys;
            findAll (eq, keys);
            REQUIRE (keys.size() == 1);   // ON
            CHECK ((bool) keys[0]->getProperties()[ParagonLookAndFeel::rackKeyProperty]);
            const auto before = snapshot();
            keys[0]->setToggleState (! keys[0]->getToggleState(), juce::sendNotificationSync);
            CHECK (changedSomething (before));
            continue;
        }
        CHECK (panel->getBounds() == rack.getControlArea() + rack.getPosition());
        CHECK (rackBounds.contains (panel->getBounds()));
        CHECK (panel->getRight() <= expression[0]->getX());
        CHECK (panel->getTransform().isIdentity());

        // Every control showing is one of the unit's parts at full size, and
        // the real one: working it changes a setting.
        std::vector<juce::Component*> showing;
        for (auto* child : panel->getChildren())
            if (child->isVisible() && (dynamic_cast<juce::Slider*> (child) != nullptr || dynamic_cast<juce::ComboBox*> (child) != nullptr
                                       || dynamic_cast<juce::ToggleButton*> (child) != nullptr))
                showing.push_back (child);
        CHECK (showing.size() >= 2);
        for (auto* control : showing)
        {
            if (! control->isVisible())
                continue;   // a TYPE changed by an earlier control hid it
            // (A big knob draws only the middle of its box, which may reach a
            // little past the face's edge: what's drawn stays on the face.)
            const auto drawn = dynamic_cast<juce::Slider*> (control) != nullptr ? control->getBounds().reduced (control->getWidth() / 6, 0) : control->getBounds();
            CAPTURE (drawn.toString());
            CHECK (panel->getLocalBounds().contains (drawn));
            const auto before = snapshot();
            if (auto* slider = dynamic_cast<juce::Slider*> (control))
            {
                CAPTURE (slider->getName(), slider->getWidth(), slider->getHeight());
                CHECK ((bool) slider->getProperties()[ParagonLookAndFeel::pedalKnobProperty]);
                CHECK (slider->getWidth() >= 80);   // the unit's big knob, not shrunk
                const auto range = slider->getRange();
                slider->setValue (slider->getValue() < range.getStart() + range.getLength() * 0.5 ? range.getEnd() : range.getStart(),
                                  juce::sendNotificationSync);
            }
            else if (auto* box = dynamic_cast<juce::ComboBox*> (control))
            {
                CHECK (box->getWidth() >= 80);
                box->setSelectedItemIndex ((box->getSelectedItemIndex() + 1) % box->getNumItems(), juce::sendNotificationSync);
            }
            else if (auto* key = dynamic_cast<juce::ToggleButton*> (control))
            {
                CHECK ((bool) key->getProperties()[ParagonLookAndFeel::rackKeyProperty]);
                CHECK (key->getHeight() >= 24);
                key->setToggleState (! key->getToggleState(), juce::sendNotificationSync);
            }
            CHECK (changedSomething (before));
        }
    }
}
