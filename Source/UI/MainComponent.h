// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/Analyzer.h"
#include "Model/Project.h"
#include "Model/ReductionController.h"
#include "Synthesis/PartialSynth.h"
#include "UI/InspectorPanel.h"
#include "UI/PartialView.h"
#include "UI/ReductionPanel.h"
#include "UI/TransportBar.h"

#include <JuceHeader.h>

/**
 * Root component: lays out the partial canvas, transport bar, reduction panel,
 * and inspector. Also owns the audio device and drives the analysis flow.
 */
class MainComponent : public juce::AudioAppComponent,
                      public Project::Listener {
public:
    MainComponent();
    ~MainComponent() override;

    // ── juce::AudioAppComponent ───────────────────────────────────────────────
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // ── juce::Component ───────────────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ── Project::Listener ────────────────────────────────────────────────────
    void partialsChanged(Project&) override;

private:
    void openFileAndAnalyze();
    void onAnalysisComplete(std::vector<std::unique_ptr<Partial>> partials);

    Project              project;
    Analyzer             analyzer;
    PartialSynth         synth;
    ReductionController  reductionController{project};

    PartialView     partialView{project};
    ReductionPanel  reductionPanel{project, reductionController};
    TransportBar    transportBar;
    InspectorPanel  inspectorPanel{project};

    juce::TextButton openButton{"Open..."};
    juce::Label      fileNameLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};