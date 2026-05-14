// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ReductionPanel.h"

ReductionPanel::ReductionPanel(Project& p, ReductionController& c)
    : project(p), controller(c)
{
    buildLayout();
}

void ReductionPanel::buildLayout()
{
    auto addLabelAndSlider = [this](juce::Label& label, juce::Slider& slider) {
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    };

    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font{16.0f}.boldened());

    addLabelAndSlider(amplitudeRankLabel, amplitudeRankSlider);
    amplitudeRankSlider.setRange(1, 500, 1);
    amplitudeRankSlider.setValue(500, juce::dontSendNotification);
    amplitudeRankSlider.onValueChange = [this] {
        controller.applyAmplitudeRank(static_cast<int>(amplitudeRankSlider.getValue()));
    };

    addLabelAndSlider(durationLabel, durationSlider);
    durationSlider.setRange(0.0, 2.0, 0.001);
    durationSlider.setValue(0.0, juce::dontSendNotification);
    durationSlider.onValueChange = [this] {
        controller.applyDurationThreshold(durationSlider.getValue());
    };

    addLabelAndSlider(amplitudeThreshLabel, amplitudeThreshSlider);
    amplitudeThreshSlider.setRange(0.0, 1.0, 0.001);
    amplitudeThreshSlider.setValue(0.0, juce::dontSendNotification);
    amplitudeThreshSlider.onValueChange = [this] {
        controller.applyAmplitudeThreshold(static_cast<float>(amplitudeThreshSlider.getValue()));
    };

    addLabelAndSlider(freqLowLabel, freqLowSlider);
    freqLowSlider.setRange(20.0, 20000.0, 1.0);
    freqLowSlider.setValue(20.0, juce::dontSendNotification);
    freqLowSlider.setSkewFactorFromMidPoint(1000.0);
    freqLowSlider.onValueChange = [this] {
        controller.applyFrequencyBand(static_cast<float>(freqLowSlider.getValue()),
                                      static_cast<float>(freqHighSlider.getValue()));
    };

    addLabelAndSlider(freqHighLabel, freqHighSlider);
    freqHighSlider.setRange(20.0, 20000.0, 1.0);
    freqHighSlider.setValue(8000.0, juce::dontSendNotification);
    freqHighSlider.setSkewFactorFromMidPoint(1000.0);
    freqHighSlider.onValueChange = [this] {
        controller.applyFrequencyBand(static_cast<float>(freqLowSlider.getValue()),
                                      static_cast<float>(freqHighSlider.getValue()));
    };

    addLabelAndSlider(energyLabel, energySlider);
    energySlider.setRange(0.0, 1.0, 0.001);
    energySlider.setValue(0.0, juce::dontSendNotification);
    energySlider.onValueChange = [this] {
        controller.applyEnergyThreshold(static_cast<float>(energySlider.getValue()));
    };

    addAndMakeVisible(clearButton);
    clearButton.onClick = [this] {
        controller.clearReductions();
        amplitudeRankSlider.setValue(500, juce::dontSendNotification);
        durationSlider.setValue(0.0, juce::dontSendNotification);
        amplitudeThreshSlider.setValue(0.0, juce::dontSendNotification);
        freqLowSlider.setValue(20.0, juce::dontSendNotification);
        freqHighSlider.setValue(8000.0, juce::dontSendNotification);
        energySlider.setValue(0.0, juce::dontSendNotification);
    };
}

void ReductionPanel::resized()
{
    const int rowH  = 40;
    const int labelH = 18;
    const int pad   = 8;
    auto      area  = getLocalBounds().reduced(pad);

    titleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(pad);

    auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
        label.setBounds(area.removeFromTop(labelH));
        slider.setBounds(area.removeFromTop(rowH - labelH));
        area.removeFromTop(pad / 2);
    };

    layoutRow(amplitudeRankLabel, amplitudeRankSlider);
    layoutRow(durationLabel, durationSlider);
    layoutRow(amplitudeThreshLabel, amplitudeThreshSlider);
    layoutRow(freqLowLabel, freqLowSlider);
    layoutRow(freqHighLabel, freqHighSlider);
    layoutRow(energyLabel, energySlider);

    area.removeFromTop(pad);
    clearButton.setBounds(area.removeFromTop(28));
}