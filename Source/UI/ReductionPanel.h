// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model/Project.h"
#include "Model/ReductionController.h"

#include <JuceHeader.h>

/**
 * Filter panel — reduction filter controls (Top-N, amplitude, duration, frequency, energy).
 * Changes are applied instantly so the user hears the result in real time.
 * Edit-mode toggle lives in ToolsPanel; this panel is shown on the Filters tab.
 */
class ReductionPanel : public juce::Component {
public:
    ReductionPanel(Project& project, ReductionController& controller);

    void resized() override;

    /** Total fixed height this panel requires. Parent layout uses this to allocate space. */
    static constexpr int preferredHeight()
    {
        const int pad    = 8;
        const int rowH   = 40;
        const int labelH = 18;
        const int nRows  = 6;
        return pad * 2
             + 24 + pad                   // filters title + gap
             + nRows * (labelH + (rowH - labelH) + pad / 2)  // filter rows
             + pad + 28;                  // gap + reset button
    }

private:
    void buildLayout();

    Project&             project;
    ReductionController& controller;

    juce::Label  titleLabel{"title", "Filters"};

    juce::Label  amplitudeRankLabel{"arLabel", "Top-N partials"};
    juce::Slider amplitudeRankSlider;

    juce::Label  durationLabel{"durLabel", "Min duration (s)"};
    juce::Slider durationSlider;

    juce::Label  amplitudeThreshLabel{"atLabel", "Min amplitude"};
    juce::Slider amplitudeThreshSlider;

    juce::Label  freqLowLabel{"flLabel", "Freq low (Hz)"};
    juce::Slider freqLowSlider;

    juce::Label  freqHighLabel{"fhLabel", "Freq high (Hz)"};
    juce::Slider freqHighSlider;

    juce::Label  energyLabel{"enLabel", "Min energy"};
    juce::Slider energySlider;

    juce::TextButton clearButton{"Reset"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReductionPanel)
};