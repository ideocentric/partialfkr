// SPDX-License-Identifier: AGPL-3.0-or-later
// Debug-only utility — compiled out in Release builds.
#pragma once
#if JUCE_DEBUG

#include "UI/CommandIDs.h"
#include "UI/MainComponent.h"

#include <JuceHeader.h>

/**
 * Automated screenshot sequencer.
 *
 * Launch the app with:
 *   PartialFKR --screenshots /path/to/sample.pfkr [/optional/output/dir]
 *
 * The helper loads the project, walks through a set of UI states using the
 * command manager, captures each state via Component::createComponentSnapshot,
 * and writes PNGs to the output directory (default: docs/screenshots/auto/
 * relative to the project root detected from the binary path).
 * The app quits automatically when done.
 */
class ScreenshotHelper : private juce::Timer
{
public:
    // ── CLI parsing ──────────────────────────────────────────────────────────

    static bool isRequested(const juce::String& cmdLine)
    {
        return cmdLine.containsIgnoreCase("--screenshots");
    }

    static void parseArgs(const juce::String& cmdLine,
                          juce::File& pfkrOut,
                          juce::File& outputDirOut)
    {
        juce::StringArray tokens;
        tokens.addTokens(cmdLine, " ", "\"");
        tokens.removeEmptyStrings();

        int idx = tokens.indexOf("--screenshots", false);
        if (idx < 0) return;

        if (idx + 1 < tokens.size())
            pfkrOut = juce::File(tokens[idx + 1].unquoted());

        if (idx + 2 < tokens.size())
            outputDirOut = juce::File(tokens[idx + 2].unquoted());
        else
            outputDirOut = defaultOutputDir();
    }

    // ── Construction ─────────────────────────────────────────────────────────

    ScreenshotHelper(juce::DocumentWindow&           window,
                     juce::ApplicationCommandManager& cm)
        : window(window), cm(cm)
    {}

    void start(const juce::File& pfkrFile, const juce::File& outputDir)
    {
        this->pfkrFile  = pfkrFile;
        this->outputDir = outputDir;
        this->outputDir.createDirectory();
        juce::Logger::writeToLog("ScreenshotHelper: output -> "
                                 + this->outputDir.getFullPathName());
        step = 0;
        startTimer(400);
    }

private:
    // ── Helpers ──────────────────────────────────────────────────────────────

    void invoke(juce::CommandID cmd)
    {
        cm.invokeDirectly(cmd, false);
    }

    /** Find a component by ID starting from the content component, not the
     *  DocumentWindow root (which adds JUCE-internal wrapper layers). */
    juce::Component* findComp(const juce::String& id) const
    {
        auto* content = window.getContentComponent();
        if (content == nullptr) return nullptr;
        // Check the content component itself first, then its children.
        if (content->getComponentID() == id) return content;
        return content->findChildWithID(id);
    }

    void saveWindow(const juce::String& id)
    {
        save(window, id);
    }

    void saveChild(const juce::String& componentID, const juce::String& imageID)
    {
        if (auto* child = findComp(componentID))
            save(*child, imageID);
        else
            juce::Logger::writeToLog("ScreenshotHelper: not found: " + componentID);
    }

    void save(juce::Component& comp, const juce::String& id)
    {
        auto bounds = comp.getLocalBounds();
        if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        {
            juce::Logger::writeToLog("  skipped (zero bounds): " + id);
            return;
        }
        comp.repaint();
        auto img = comp.createComponentSnapshot(bounds);
        juce::File f = outputDir.getChildFile(id + ".png");
        juce::FileOutputStream os(f);
        if (os.openedOk())
        {
            juce::PNGImageFormat{}.writeImageToStream(img, os);
            juce::Logger::writeToLog("  saved: " + f.getFileName());
        }
    }

    void setNthSlider(const juce::String& compID, int n, double value)
    {
        if (auto* root = findComp(compID))
        {
            int count = 0;
            setSliderRecursive(*root, n, value, count);
        }
    }

    static bool setSliderRecursive(juce::Component& comp, int target,
                                   double value, int& count)
    {
        if (auto* s = dynamic_cast<juce::Slider*>(&comp))
            if (count++ == target) { s->setValue(value, juce::sendNotification); return true; }
        for (int i = 0; i < comp.getNumChildComponents(); ++i)
            if (setSliderRecursive(*comp.getChildComponent(i), target, value, count))
                return true;
        return false;
    }

    void clickButton(const juce::String& compID, const juce::String& label)
    {
        if (auto* root = findComp(compID))
            clickButtonRecursive(*root, label);
    }

    static bool clickButtonRecursive(juce::Component& comp, const juce::String& label)
    {
        if (auto* btn = dynamic_cast<juce::Button*>(&comp))
            if (btn->getButtonText().containsIgnoreCase(label))
                { btn->triggerClick(); return true; }
        for (int i = 0; i < comp.getNumChildComponents(); ++i)
            if (clickButtonRecursive(*comp.getChildComponent(i), label))
                return true;
        return false;
    }

    void keyToPartialView(int keyCode)
    {
        if (auto* pv = findComp("partialView"))
            pv->keyPressed(juce::KeyPress(keyCode));
    }

    // ── Step sequencer ────────────────────────────────────────────────────────

    void timerCallback() override
    {
        stopTimer();
        runStep();
    }

    void runStep()
    {
        auto* mc = dynamic_cast<MainComponent*>(window.getContentComponent());
        if (mc == nullptr) return;

        switch (step++)
        {
            // ── 0: Empty window ──────────────────────────────────────────────
            case 0:
                saveWindow("S-01_empty_window");
                mc->loadProject(pfkrFile);
                startTimer(900);
                break;

            // ── 1: Loaded — capture default states ──────────────────────────
            case 1:
                saveWindow("S-02_loaded_window");
                saveChild("partialView",   "S-11_canvas_default");
                saveChild("transportBar",  "S-16_transport_stopped");
                saveChild("reductionPanel","S-33_filters_default");
                saveChild("inspectorPanel","S-42_inspector_no_selection");
                saveChild("levelMeter",    "S-20_level_meter");
                saveChild("gainKnob",      "S-21_gain_fader");
                startTimer(200);
                break;

            // ── 2: Zoom in ───────────────────────────────────────────────────
            case 2:
                invoke(CommandIDs::viewZoomIn);
                invoke(CommandIDs::viewZoomIn);
                invoke(CommandIDs::viewZoomIn);
                startTimer(200);
                break;

            case 3:
                saveChild("partialView", "S-12_canvas_zoomed_in");
                invoke(CommandIDs::viewZoomFit);
                startTimer(200);
                break;

            // ── 4: Zoom to fit ───────────────────────────────────────────────
            case 4:
                saveChild("partialView", "S-13_canvas_zoom_fit");
                invoke(juce::StandardApplicationCommandIDs::selectAll);
                startTimer(200);
                break;

            // ── 5: All partials selected ─────────────────────────────────────
            case 5:
                saveWindow("S-24_window_all_selected");
                saveChild("partialView",   "S-25_canvas_all_selected");
                saveChild("inspectorPanel","S-43_inspector_with_selection");
                saveChild("reductionPanel","S-26_edit_mode_select_active");
                invoke(juce::StandardApplicationCommandIDs::deselectAll);
                startTimer(200);
                break;

            // ── 6: Direct Select mode ────────────────────────────────────────
            case 6:
                keyToPartialView('a');
                startTimer(200);
                break;

            case 7:
                saveChild("reductionPanel","S-27_edit_mode_direct_select_active");
                keyToPartialView('v');          // back to Select
                invoke(CommandIDs::viewTogglePanel);
                startTimer(200);
                break;

            // ── 8: Panel hidden ──────────────────────────────────────────────
            case 8:
                saveWindow("S-04_panel_hidden");
                invoke(CommandIDs::viewTogglePanel);   // show again
                startTimer(300);
                break;

            // ── 9: Top-N filter ──────────────────────────────────────────────
            case 9:
                setNthSlider("reductionPanel", 0, 12.0);
                startTimer(300);
                break;

            case 10:
                saveChild("reductionPanel","S-34_filters_topN_set");
                saveChild("partialView",   "S-37_canvas_after_topN_filter");
                saveWindow("S-36_window_after_filter");
                clickButton("reductionPanel", "Reset");
                startTimer(200);
                break;

            // ── 11: Frequency band filter ────────────────────────────────────
            case 11:
                setNthSlider("reductionPanel", 3, 150.0);
                setNthSlider("reductionPanel", 4, 1200.0);
                startTimer(300);
                break;

            case 12:
                saveChild("reductionPanel","S-35_filters_freq_band");
                clickButton("reductionPanel", "Reset");
                invoke(CommandIDs::transportLoop);
                startTimer(200);
                break;

            // ── 13: Loop enabled ─────────────────────────────────────────────
            case 13:
                saveChild("transportBar", "S-19_transport_loop_on");
                invoke(CommandIDs::transportLoop);
                writeManifest();
                juce::MessageManager::callAsync([] {
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                });
                break;

            default:
                break;
        }
    }

    // ── Manifest and output dir ───────────────────────────────────────────────

    void writeManifest()
    {
        juce::String md;
        md += "# Screenshot Manifest\n\n";
        md += "Source: `" + pfkrFile.getFullPathName() + "`\n\n";
        md += "| File | Description |\n|---|---|\n";

        juce::Array<juce::File> pngs;
        outputDir.findChildFiles(pngs, juce::File::findFiles, false, "*.png");
        pngs.sort();
        for (auto& f : pngs)
            md += "| " + f.getFileName() + " | |\n";

        outputDir.getChildFile("manifest.md").replaceWithText(md);
        juce::Logger::writeToLog("ScreenshotHelper: done - "
                                 + juce::String(pngs.size()) + " screenshots.");
    }

    static juce::File defaultOutputDir()
    {
        juce::File dir = juce::File::getSpecialLocation(
                             juce::File::currentExecutableFile)
                         .getParentDirectory();
        for (int i = 0; i < 8; ++i)
        {
            if (dir.getChildFile("CMakeLists.txt").existsAsFile())
                return dir.getChildFile("docs/screenshots/auto");
            dir = dir.getParentDirectory();
        }
        return juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                          .getChildFile("PartialFKR_screenshots");
    }

    // ── State ─────────────────────────────────────────────────────────────────

    juce::DocumentWindow&            window;
    juce::ApplicationCommandManager& cm;
    juce::File                       pfkrFile;
    juce::File                       outputDir;
    int                              step = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScreenshotHelper)
};

#endif // JUCE_DEBUG