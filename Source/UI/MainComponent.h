// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/Analyzer.h"
#include "Model/AmplitudeOps.h"
#include "Export/CsoundExporter.h"
#include "Export/SuperColliderExporter.h"
#include "Export/JsonExporter.h"
#include "Export/MidiExporter.h"
#include "Export/MidiPackageExporter.h"
#include "Export/SdifExporter.h"
#include "Model/Project.h"
#include "Model/ReductionController.h"
#include "Model/Selection.h"
#include "Synthesis/PartialSynth.h"
#include "Model/PfkrFormat.h"
#include "UI/CommandIDs.h"
#include "UI/InspectorPanel.h"
#include "UI/LevelMeter.h"
#include "UI/MacProxyIcon.h"
#include "UI/PartialView.h"
#include "UI/ReductionPanel.h"
#include "UI/TransportBar.h"

#include <JuceHeader.h>

/**
 * Root component: lays out the partial canvas, transport bar, reduction panel,
 * and inspector. Also owns the audio device and drives the analysis flow.
 */
class MainComponent : public juce::AudioAppComponent,
                      public juce::ApplicationCommandTarget,
                      public Project::Listener,
                      public Selection::Listener {
public:
    MainComponent();
    ~MainComponent() override;

    // ── juce::ApplicationCommandTarget ────────────────────────────────────────
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID commandID,
                        juce::ApplicationCommandInfo& result) override;
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& info) override;

    // ── juce::AudioAppComponent ───────────────────────────────────────────────
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // ── juce::Component ───────────────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // ── Project::Listener ────────────────────────────────────────────────────
    void partialsChanged(Project&) override;

    // ── Selection::Listener ──────────────────────────────────────────────────
    void selectionChanged(Selection&) override;

    void openFileAndAnalyze();  ///< renamed internally to analyzeAudio; kept for call sites
    void analyzeAudio() { openFileAndAnalyze(); }
    void performNormalize();
    void performScaleAmplitude();

    // ── Save / Load ───────────────────────────────────────────────────────────
    /** Load a .pfkr project into this window. */
    void loadProject(const juce::File& pfkrFile);
    /** Save to currentFile (sync). Returns true on success. */
    bool saveCurrentFile();
    /** Show a Save As dialog then save. Calls onComplete(success) when done. */
    void saveProjectAs(std::function<void(bool)> onComplete);
    /** Save to currentFile if set, otherwise falls through to saveProjectAs. */
    void saveProject() { if (currentFile.existsAsFile()) saveCurrentFile();
                         else saveProjectAs(nullptr); }

    [[nodiscard]] bool         isDirty()         const noexcept { return dirty; }
    [[nodiscard]] bool         hasSourceAudio()  const noexcept { return project.hasSourceAudio(); }
    [[nodiscard]] juce::File   getCurrentFile()  const noexcept { return currentFile; }
    [[nodiscard]] juce::String getDisplayName()  const;

    /** Wired by PartialFKRApplication to trigger a native menu bar rebuild whenever
     *  edit-relevant state changes (selection, partials, clipboard). */
    std::function<void()> onMenuStateChanged;

    /** Wired by PartialFKRApplication to request that this window close (with dirty check). */
    std::function<void()> onCloseRequested;

    // State queries used by AppMenuBarModel to build the Edit menu
    [[nodiscard]] bool hasPartials()      const noexcept { return !project.getPartials().empty(); }
    [[nodiscard]] bool hasSelection()     const noexcept { return !project.getSelection().isEmpty(); }
    [[nodiscard]] bool hasClipboard()     const noexcept { return !clipboard.empty(); }
    [[nodiscard]] bool canUndo()          noexcept { return project.getEditHistory().canUndo(); }
    [[nodiscard]] bool canRedo()          noexcept { return project.getEditHistory().canRedo(); }
    [[nodiscard]] bool getIsNormalizing() const noexcept { return isNormalizing; }
    [[nodiscard]] bool hasValidFadeRange() const noexcept { return outPoint >= 0.0 && outPoint > inPoint; }

private:
    void setWindowTitle(const juce::String& title);
    void setDirtyFlag(bool d);
    PfkrFormat::SaveData buildSaveData() const;
    void onAnalysisComplete(std::vector<std::unique_ptr<Partial>> partials);

    // ── Edit operations ───────────────────────────────────────────────────────
    void editCopy();
    void editCut();
    void editPaste();
    void editDelete();
    void editUndo();
    void editRedo();
    void editBreakpointDelete();
    void editStretch();
    void editStretchPartials(double factor);
    void editStretchBreakpoints(double factor);

    // ── Transport ─────────────────────────────────────────────────────────────
    void handlePlayPauseToggle();
    void handlePlay();
    void handleResume();
    void handlePause();
    void handleStop();
    void handlePlayComplete();

    double inPoint              = 0.0;
    double outPoint             = -1.0;  ///< -1 = end of file
    bool   playStartedWithinRange = true;

    void   updateMarkers(double in, double out);
    double totalDuration() const noexcept;

    // ── Scrub ─────────────────────────────────────────────────────────────────
    void handleScrubStart(double t);
    void handleScrubUpdate(double t);
    void handleScrubEnd();

    bool                  scrubWasPlaying  = false;
    bool                  isNormalizing    = false;
    bool                  dirty            = false;
    bool                  rightPanelVisible = true;
    juce::File            currentFile;  ///< the .pfkr file this project is saved to (empty = unsaved)
    std::vector<uint32_t> scrubSavedSoloIds;  ///< solo state saved at scrub start, restored on end

    // ── Join operations ───────────────────────────────────────────────────────
    void performBridgePartials();
    void performCrossfadeOverlap();
    void performFade(bool fadeIn);

    // ── Export ────────────────────────────────────────────────────────────────
    void exportMidi();
    void exportMidiPackage();
    void exportCsound();
    void exportSuperCollider();
    void exportSdif();
    void exportJson();

    /** Clipboard: breakpoints + colour per copied partial.
     *  Static so copy in one window is visible to all other open windows. */
    struct PartialClip {
        std::vector<Breakpoint> breakpoints;
        juce::Colour            colour;
    };
    static std::vector<PartialClip> clipboard;

    // Peak levels written by the audio thread, read by LevelMeter's timer.
    std::atomic<float> levelL{0.0f};
    std::atomic<float> levelR{0.0f};

    Project              project;
    Analyzer             analyzer;
    PartialSynth         synth;
    ReductionController  reductionController{project};

    PartialView     partialView{project};
    ReductionPanel  reductionPanel{project, reductionController};
    TransportBar    transportBar;
    InspectorPanel  inspectorPanel{project};
    LevelMeter      levelMeter{levelL, levelR};

    juce::Slider           gainKnob{juce::Slider::LinearVertical,
                                   juce::Slider::NoTextBox};
    juce::Label            gainLabel;          ///< live dB readout above the fader
    juce::Rectangle<int>   gainScaleArea;      ///< painted in paint(), updated in resized()


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};