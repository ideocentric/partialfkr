// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model/Project.h"
#include "Model/Selection.h"

#include <JuceHeader.h>

/**
 * Inspector panel showing properties of the current selection:
 * partial count, frequency range, duration, peak amplitude.
 * Updates whenever the selection changes.
 */
class InspectorPanel : public juce::Component,
                       public Project::Listener,
                       public Selection::Listener {
public:
    explicit InspectorPanel(Project& project);
    ~InspectorPanel() override;

    void resized() override;

    // ── Project::Listener ────────────────────────────────────────────────────
    void partialsChanged(Project&) override;

    // ── Selection::Listener ──────────────────────────────────────────────────
    void selectionChanged(Selection&) override;

private:
    void refresh();

    Project& project;

    juce::Label titleLabel{"title", "Inspector"};
    juce::Label countLabel;
    juce::Label freqRangeLabel;
    juce::Label durationLabel;
    juce::Label amplitudeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InspectorPanel)
};