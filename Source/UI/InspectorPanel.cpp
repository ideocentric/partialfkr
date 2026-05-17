// SPDX-License-Identifier: AGPL-3.0-or-later
#include "InspectorPanel.h"

InspectorPanel::InspectorPanel(Project& p) : project(p)
{
    project.addListener(this);
    project.getSelection().addListener(this);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(countLabel);
    addAndMakeVisible(freqRangeLabel);
    addAndMakeVisible(durationLabel);
    addAndMakeVisible(amplitudeLabel);

    titleLabel.setFont(juce::Font{16.0f}.boldened());
    for (auto* label : {&countLabel, &freqRangeLabel, &durationLabel, &amplitudeLabel})
        label->setFont(juce::Font{13.0f});

    refresh();
}

InspectorPanel::~InspectorPanel()
{
    project.removeListener(this);
    project.getSelection().removeListener(this);
}

void InspectorPanel::resized()
{
    const int labelH = 22;
    const int pad    = 6;
    auto area = getLocalBounds().reduced(8);

    titleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(pad);

    for (auto* label : {&countLabel, &freqRangeLabel, &durationLabel, &amplitudeLabel}) {
        label->setBounds(area.removeFromTop(labelH));
        area.removeFromTop(pad / 2);
    }
}


void InspectorPanel::partialsChanged(Project&) { refresh(); }
void InspectorPanel::selectionChanged(Selection&) { refresh(); }

void InspectorPanel::refresh()
{
    const auto& selection = project.getSelection();
    const auto ids = selection.getSelectedIds();

    if (ids.empty()) {
        const size_t total = project.getPartialCount();
        countLabel.setText("Partials: " + juce::String(total), juce::dontSendNotification);
        freqRangeLabel.setText("(no selection)", juce::dontSendNotification);
        durationLabel.setText("", juce::dontSendNotification);
        amplitudeLabel.setText("", juce::dontSendNotification);
        return;
    }

    float minFreq = 1e6f, maxFreq = 0.0f;
    double minDur = 1e9, maxDur = 0.0;
    float  maxAmp = 0.0f;

    for (auto id : ids) {
        const auto* p = project.findPartialById(id);
        if (p == nullptr)
            continue;
        minFreq = std::min(minFreq, p->medianFrequency());
        maxFreq = std::max(maxFreq, p->medianFrequency());
        minDur  = std::min(minDur,  p->duration());
        maxDur  = std::max(maxDur,  p->duration());
        maxAmp  = std::max(maxAmp,  p->peakAmplitude());
    }

    countLabel.setText("Selected: " + juce::String(ids.size()), juce::dontSendNotification);
    freqRangeLabel.setText(
        juce::String(minFreq, 1) + " – " + juce::String(maxFreq, 1) + " Hz",
        juce::dontSendNotification);
    durationLabel.setText(
        juce::String(minDur, 3) + " – " + juce::String(maxDur, 3) + " s",
        juce::dontSendNotification);
    amplitudeLabel.setText(
        "Peak: " + juce::String(20.0f * std::log10(std::max(maxAmp, 1e-6f)), 1) + " dB",
        juce::dontSendNotification);
}