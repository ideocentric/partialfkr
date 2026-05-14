// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model/Project.h"
#include "Model/ReductionController.h"

#include <JuceHeader.h>

/**
 * Side panel with sliders/controls for each reduction operation.
 * Changes are applied instantly so the user hears the result in real time.
 */
class ReductionPanel : public juce::Component {
public:
    ReductionPanel(Project& project, ReductionController& controller);

    void resized() override;

private:
    void buildLayout();

    Project&             project;
    ReductionController& controller;

    juce::Label  titleLabel{"title", "Reduction"};

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

    juce::TextButton clearButton{"Clear reductions"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReductionPanel)
};