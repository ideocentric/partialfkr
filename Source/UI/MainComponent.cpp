// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"

MainComponent::MainComponent()
{
    project.addListener(this);

    addAndMakeVisible(partialView);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(reductionPanel);
    addAndMakeVisible(inspectorPanel);
    addAndMakeVisible(openButton);
    addAndMakeVisible(fileNameLabel);

    openButton.onClick = [this] { openFileAndAnalyze(); };

    fileNameLabel.setText("No file loaded", juce::dontSendNotification);
    fileNameLabel.setFont(juce::Font(13.0f));
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    fileNameLabel.setJustificationType(juce::Justification::centredLeft);

    transportBar.onPlay = [this] {
        synth.setSolo(project.hasAnySolo());
        synth.setPlaying(true);
    };
    transportBar.onStop = [this] {
        synth.setPlaying(false);
    };
    transportBar.onSeek = [this](double t) {
        synth.setPlayheadTime(t);
        partialView.setPlayheadTime(t);
    };
    transportBar.getPosition = [this] { return synth.getPlayheadSeconds(); };

    setAudioChannels(0, 2);
    setSize(1200, 800);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
    project.removeListener(this);
}

void MainComponent::prepareToPlay(int samplesPerBlock, double sr)
{
    synth.setProject(&project);
    synth.prepareToPlay(sr, samplesPerBlock);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    synth.renderNextBlock(*bufferToFill.buffer,
                          bufferToFill.startSample,
                          bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    synth.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Transport bar across the bottom
    transportBar.setBounds(area.removeFromBottom(48));

    // Right panel column
    const int panelW = 220;
    auto rightColumn = area.removeFromRight(panelW);

    // Open button + filename at the top of the right column
    auto fileRow = rightColumn.removeFromTop(30);
    openButton.setBounds(fileRow.removeFromLeft(80));
    fileRow.removeFromLeft(6);
    fileNameLabel.setBounds(fileRow);

    // Divider line (cosmetic — just leave a gap)
    rightColumn.removeFromTop(4);

    // Stack reduction and inspector panels in the remaining right column space
    const int inspectorH = 160;
    inspectorPanel.setBounds(rightColumn.removeFromBottom(inspectorH));
    reductionPanel.setBounds(rightColumn);

    // Partial canvas fills everything left
    partialView.setBounds(area);
}

void MainComponent::partialsChanged(Project&)
{
    const auto& partials = project.getPartials();
    double duration = partials.empty() ? 0.0 : partials.back()->endTime();
    transportBar.setDuration(duration);
    repaint();
}

void MainComponent::openFileAndAnalyze()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Open audio file", juce::File{}, "*.wav;*.aif;*.aiff");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto results = fc.getResults();
                             if (results.isEmpty())
                                 return;

                             fileNameLabel.setText("Analysing...",
                                                   juce::dontSendNotification);

                             project.loadSourceAudio(results[0]);

                             analyzer.analyzeAsync(
                                 project.getSourceAudio(),
                                 project.getSampleRate(),
                                 [](float /*progress*/) {},
                                 [this](auto partials) {
                                     onAnalysisComplete(std::move(partials));
                                 });
                         });
}

void MainComponent::onAnalysisComplete(std::vector<std::unique_ptr<Partial>> partials)
{
    fileNameLabel.setText(project.getSourceFile().getFileName(),
                          juce::dontSendNotification);

    project.setPartials(std::move(partials));
    synth.setPlaying(false);
    synth.setProject(&project);
    synth.setPlayheadTime(0.0);
    synth.setSolo(project.hasAnySolo());
    transportBar.setPlaying(false);
    transportBar.setPlayheadPosition(0.0);
}