// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ReductionPanel.h"

ReductionPanel::ReductionPanel(Project& p, ReductionController& c)
    : project(p), controller(c)
{
    buildLayout();
}

void ReductionPanel::setActiveMode(bool isDirect)
{
    selectButton.setColour(juce::TextButton::buttonColourId,
        !isDirect ? juce::Colour(0xff606060) : juce::Colour(0xff1a1a1a));
    selectButton.setColour(juce::TextButton::textColourOffId,
        !isDirect ? juce::Colours::white : juce::Colours::grey);
    directSelButton.setColour(juce::TextButton::buttonColourId,
        isDirect ? juce::Colour(0xff606060) : juce::Colour(0xff1a1a1a));
    directSelButton.setColour(juce::TextButton::textColourOffId,
        isDirect ? juce::Colours::white : juce::Colours::grey);
}

void ReductionPanel::buildLayout()
{
    auto addLabelAndSlider = [this](juce::Label& label, juce::Slider& slider) {
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    };

    // Edit Mode buttons
    addAndMakeVisible(editModeLabel);
    addAndMakeVisible(selectButton);
    addAndMakeVisible(directSelButton);
    selectButton.setWantsKeyboardFocus(false);
    directSelButton.setWantsKeyboardFocus(false);
    selectButton.onClick = [this] {
        setActiveMode(false);
        if (onToolModeChanged) onToolModeChanged(false);
    };
    directSelButton.onClick = [this] {
        setActiveMode(true);
        if (onToolModeChanged) onToolModeChanged(true);
    };
    setActiveMode(false);

    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font{16.0f}.boldened());

    addLabelAndSlider(amplitudeRankLabel, amplitudeRankSlider);
    amplitudeRankSlider.setRange(1, 500, 1);
    amplitudeRankSlider.setValue(500, juce::dontSendNotification);
    amplitudeRankSlider.onValueChange = [this] {
        controller.applyAmplitudeRank(static_cast<int>(amplitudeRankSlider.getValue()));
    };

    addLabelAndSlider(durationLabel, durationSlider);
    durationSlider.setRange(0.0, 0.250, 0.001);
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
    energySlider.setRange(0.0, 0.1, 0.0001);
    energySlider.setValue(0.0, juce::dontSendNotification);
    energySlider.onValueChange = [this] {
        controller.applyEnergyThreshold(static_cast<float>(energySlider.getValue()));
    };

    addAndMakeVisible(clearButton);
    clearButton.onClick = [this] {
        controller.clearReductions();
        amplitudeRankSlider.setValue(500,   juce::dontSendNotification);
        durationSlider.setValue(0.0,        juce::dontSendNotification);
        amplitudeThreshSlider.setValue(0.0, juce::dontSendNotification);
        freqLowSlider.setValue(20.0,        juce::dontSendNotification);
        freqHighSlider.setValue(8000.0,     juce::dontSendNotification);
        energySlider.setValue(0.0,          juce::dontSendNotification);
    };

}

void ReductionPanel::resized()
{
    const int pad    = 8;
    const int rowH   = 40;
    const int labelH = 18;
    auto area = getLocalBounds().reduced(pad);

    editModeLabel.setBounds(area.removeFromTop(labelH));
    auto modeRow = area.removeFromTop(28);
    selectButton   .setBounds(modeRow.removeFromLeft(modeRow.getWidth() / 2).reduced(2, 0));
    directSelButton.setBounds(modeRow.reduced(2, 0));
    area.removeFromTop(pad);

    titleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(pad);

    auto layoutRow = [&](juce::Label& label, juce::Slider& slider) {
        label.setBounds(area.removeFromTop(labelH));
        slider.setBounds(area.removeFromTop(rowH - labelH));
        area.removeFromTop(pad / 2);
    };

    layoutRow(amplitudeRankLabel,   amplitudeRankSlider);
    layoutRow(durationLabel,        durationSlider);
    layoutRow(amplitudeThreshLabel, amplitudeThreshSlider);
    layoutRow(freqLowLabel,         freqLowSlider);
    layoutRow(freqHighLabel,        freqHighSlider);
    layoutRow(energyLabel,          energySlider);

    area.removeFromTop(pad);
    clearButton.setBounds(area.removeFromTop(28));
}