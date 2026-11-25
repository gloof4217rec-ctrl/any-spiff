#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE ("one is equal to one", "[dummy]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("Transient Sculptor"));
    }

    SECTION ("program name")
    {
        // Steinberg's VST3 validator fails plugins whose programs have no name
        CHECK (testPlugin.getProgramName (0).isNotEmpty());
    }
}

TEST_CASE ("A/B comparison switches between two parameter sets", "[ab]")
{
    PluginProcessor processor;

    const auto depth = [] (PluginProcessor& p) { return p.apvts.getRawParameterValue ("depth")->load(); };

    // On A, set depth to 60.
    processor.apvts.getParameter ("depth")->setValueNotifyingHost (0.6f);
    CHECK (processor.isShowingA());
    CHECK (std::abs (depth (processor) - 60.0f) < 0.01f);

    // Switch to B: it holds the default (depth 0).
    processor.switchToB();
    CHECK (! processor.isShowingA());
    CHECK (depth (processor) < 0.01f);

    // On B, set depth to 90.
    processor.apvts.getParameter ("depth")->setValueNotifyingHost (0.9f);
    CHECK (std::abs (depth (processor) - 90.0f) < 0.01f);

    // Back to A: depth is 60 again.
    processor.switchToA();
    CHECK (processor.isShowingA());
    CHECK (std::abs (depth (processor) - 60.0f) < 0.01f);

    // Copy A to B: B now matches A, and we land on B.
    processor.copyAToB();
    CHECK (! processor.isShowingA());
    CHECK (std::abs (depth (processor) - 60.0f) < 0.01f);

    // And A is still intact after a round trip.
    processor.switchToA();
    CHECK (std::abs (depth (processor) - 60.0f) < 0.01f);
}

TEST_CASE ("Undo/redo reverts parameter edits", "[undo]")
{
    PluginProcessor processor;

    const auto depth = [] (PluginProcessor& p) { return p.apvts.getRawParameterValue ("depth")->load(); };

    CHECK (depth (processor) < 0.01f);
    CHECK (! processor.canUndo());
    CHECK (! processor.canRedo());

    // Change depth to 30.
    processor.apvts.getParameter ("depth")->setValueNotifyingHost (0.3f);
    CHECK (std::abs (depth (processor) - 30.0f) < 0.01f);
    CHECK (processor.canUndo());

    // Undo: depth reverts to 0.
    processor.undo();
    CHECK (depth (processor) < 0.01f);
    CHECK (processor.canRedo());

    // Redo: depth is 30 again.
    processor.redo();
    CHECK (std::abs (depth (processor) - 30.0f) < 0.01f);
    CHECK (processor.canUndo());
}

TEST_CASE ("Presets load and save parameter states", "[preset]")
{
    PluginProcessor processor;

    const auto depth = [] (PluginProcessor& p) { return p.apvts.getRawParameterValue ("depth")->load(); };

    const auto names = processor.getPresetNames();
    REQUIRE (names.contains ("Default"));
    REQUIRE (names.contains ("De-Click (gentle)"));

    // Load a factory preset: depth becomes 30.
    processor.loadPreset ("De-Click (gentle)");
    CHECK (std::abs (depth (processor) - 30.0f) < 0.1f);

    // Save the current state as a user preset.
    processor.saveCurrentAsUserPreset ("My Test Preset");
    CHECK (processor.getPresetNames().contains ("My Test Preset"));

    // Default resets depth, then the user preset restores it.
    processor.loadPreset ("Default");
    CHECK (depth (processor) < 0.01f);

    processor.loadPreset ("My Test Preset");
    CHECK (std::abs (depth (processor) - 30.0f) < 0.1f);

    // Delete the user preset.
    processor.deleteUserPreset ("My Test Preset");
    CHECK (! processor.getPresetNames().contains ("My Test Preset"));
}

TEST_CASE ("Band listen toggles the parameter without moving it", "[band_listen]")
{
    PluginProcessor processor;

    const auto* listenParam = processor.apvts.getRawParameterValue ("band_listen");
    REQUIRE (listenParam != nullptr);

    processor.beginBandListen (500.0f, 1.0f);
    CHECK (listenParam->load() > 0.5f);

    // Sweeping the listened frequency must not toggle the parameter back off.
    processor.updateBandListen (2000.0f, 2.0f);
    CHECK (listenParam->load() > 0.5f);

    processor.endBandListen();
    CHECK (listenParam->load() < 0.5f);
}


#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2026.0.0 (r0xa7ad6ebc)"));
    #endif
}
#endif
