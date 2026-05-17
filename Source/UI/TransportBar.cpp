// SPDX-License-Identifier: AGPL-3.0-or-later
#include "TransportBar.h"

TransportBar::TransportBar()
{
    addAndMakeVisible(stopButton);
    addAndMakeVisible(playPauseButton);
    addAndMakeVisible(loopButton);
    addAndMakeVisible(timeLabel);

    // Prevent buttons from consuming the space key so it propagates to MainComponent
    stopButton     .setWantsKeyboardFocus(false);
    playPauseButton.setWantsKeyboardFocus(false);
    loopButton     .setWantsKeyboardFocus(false);

    stopButton.onClick = [this] {
        if (onStop) onStop();
    };

    playPauseButton.onClick = [this] {
        if (onPlayPauseToggle) onPlayPauseToggle();
    };

    timeLabel.setJustificationType(juce::Justification::centred);
    timeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    updateTimeDisplay();
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(4);

    const int btnW = 36;
    stopButton     .setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(4);
    playPauseButton.setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(4);
    loopButton     .setBounds(area.removeFromLeft(btnW));
    area.removeFromLeft(8);

    timeLabel.setBounds(area.removeFromRight(80));
}

// ── State control ─────────────────────────────────────────────────────────────

void TransportBar::setState(State s)
{
    state = s;

    switch (s)
    {
        case State::Playing:
            playPauseButton.setStyle(TransportButton::Style::Pause);
            startTimer(30);
            break;

        case State::Paused:
            playPauseButton.setStyle(TransportButton::Style::Play);
            stopTimer();
            break;

        case State::Stopped:
            playPauseButton.setStyle(TransportButton::Style::Play);
            stopTimer();
            break;
    }
}

void TransportBar::setDuration(double seconds)
{
    duration = seconds;
    updateTimeDisplay();
}

void TransportBar::setPlayheadPosition(double seconds)
{
    playheadPos = seconds;
    updateTimeDisplay();
}

void TransportBar::setPlayRange(double startSeconds, double endSeconds)
{
    playRangeStart = startSeconds;
    playEnd        = endSeconds;
}

void TransportBar::clearPlayRange()
{
    playRangeStart = 0.0;
    playEnd        = -1.0;
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void TransportBar::timerCallback()
{
    if (state != State::Playing || !getPosition)
        return;

    playheadPos = getPosition();

    // Check end-of-range: either explicit play range OR end of file
    const double limit = (playEnd > 0.0) ? playEnd : duration;
    if (limit > 0.0 && playheadPos >= limit)
    {
        if (onPlayComplete)
            onPlayComplete();
        return;   // MainComponent drives state after this
    }

    setPlayheadPosition(playheadPos);
    if (onPositionChanged)
        onPositionChanged(playheadPos);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void TransportBar::updateTimeDisplay()
{
    const int    totalSec = static_cast<int>(playheadPos);
    const int    mins     = totalSec / 60;
    const int    secs     = totalSec % 60;
    const int    ms       = static_cast<int>((playheadPos - totalSec) * 100);
    timeLabel.setText(juce::String::formatted("%d:%02d.%02d", mins, secs, ms),
                      juce::dontSendNotification);
}