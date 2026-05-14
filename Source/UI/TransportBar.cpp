// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TransportBar.h"

TransportBar::TransportBar()
{
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(positionSlider);
    addAndMakeVisible(timeLabel);

    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    positionSlider.setRange(0.0, 1.0);
    positionSlider.onValueChange = [this] {
        if (onSeek)
            onSeek(positionSlider.getValue() * duration);
    };

    playButton.onClick = [this] {
        isPlaying = true;
        playButton.setButtonText("Pause");
        startTimer(50);
        if (onPlay)
            onPlay();
    };

    stopButton.onClick = [this] {
        isPlaying = false;
        playButton.setButtonText("Play");
        stopTimer();
        if (onStop)
            onStop();
    };

    timeLabel.setJustificationType(juce::Justification::centred);
    updateTimeDisplay();
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(4);

    playButton.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(4);
    stopButton.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(4);
    loopButton.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(8);

    timeLabel.setBounds(area.removeFromRight(90));
    positionSlider.setBounds(area);
}

void TransportBar::setDuration(double seconds)
{
    duration = seconds;
    positionSlider.setRange(0.0, seconds > 0.0 ? seconds : 1.0);
    updateTimeDisplay();
}

void TransportBar::setPlayheadPosition(double seconds)
{
    playheadPos = seconds;
    positionSlider.setValue(seconds, juce::dontSendNotification);
    updateTimeDisplay();
}

void TransportBar::setPlaying(bool playing)
{
    isPlaying = playing;
    playButton.setButtonText(playing ? "Pause" : "Play");
}

void TransportBar::timerCallback()
{
    // Poll the synth's actual playhead position so the display stays in sync.
    if (getPosition)
    {
        playheadPos = getPosition();
        if (duration > 0.0 && playheadPos >= duration)
        {
            if (loopButton.getToggleState())
                playheadPos = 0.0;
            else
            {
                isPlaying = false;
                playButton.setButtonText("Play");
                stopTimer();
                if (onStop) onStop();
            }
        }
        setPlayheadPosition(playheadPos);
    }
}

void TransportBar::updateTimeDisplay()
{
    const int totalSec = static_cast<int>(playheadPos);
    const int mins     = totalSec / 60;
    const int secs     = totalSec % 60;
    const int ms       = static_cast<int>((playheadPos - totalSec) * 100);
    timeLabel.setText(juce::String::formatted("%d:%02d.%02d", mins, secs, ms),
                      juce::dontSendNotification);
}