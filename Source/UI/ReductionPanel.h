// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model/Project.h"
#include "Model/ReductionController.h"

#include <JuceHeader.h>

/**
 * Side panel with edit-mode toggle and filter controls.
 * Changes are applied instantly so the user hears the result in real time.
 */
class ReductionPanel : public juce::Component {
public:
    ReductionPanel(Project& project, ReductionController& controller);

    void resized() override;

    /** Update the highlighted button to match the current tool mode. */
    void setActiveMode(bool isDirect);

    /** Fired when the user clicks a mode button. Argument: true = DirectSelect. */
    std::function<void(bool)> onToolModeChanged;

    /** Total fixed height this panel requires. Parent layout uses this to allocate space. */
    static constexpr int preferredHeight()
    {
        const int pad    = 8;
        const int rowH   = 40;
        const int labelH = 18;
        const int nRows  = 6;
        return pad * 2
             + labelH + 28 + pad          // edit mode: label + buttons + gap
             + 24 + pad                   // filters title + gap
             + nRows * (labelH + (rowH - labelH) + pad / 2)  // filter rows
             + pad + 28;                  // gap + reset button
    }

private:
    void buildLayout();

    Project&             project;
    ReductionController& controller;

    juce::Label      editModeLabel  {"emLabel", "Edit Mode"};
    juce::TextButton selectButton   {"Select"};
    juce::TextButton directSelButton{"Direct Select"};

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