// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <functional>

/**
 * Transport controls: play/stop/loop, position slider, and time display.
 * Notifies listeners via callbacks to keep it decoupled from Project.
 */
class TransportBar : public juce::Component,
                     private juce::Timer {
public:
    TransportBar();

    void resized() override;

    // Callbacks wired up by MainComponent
    std::function<void()>         onPlay;
    std::function<void()>         onStop;
    std::function<void(double)>   onSeek;       ///< seconds
    std::function<double()>       getPosition;  ///< returns current synth position in seconds

    void setDuration(double seconds);
    void setPlayheadPosition(double seconds);
    void setPlaying(bool isPlaying);

private:
    void timerCallback() override;
    void updateTimeDisplay();

    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::ToggleButton loopButton{"Loop"};
    juce::Slider      positionSlider;
    juce::Label       timeLabel;

    double duration      = 0.0;
    double playheadPos   = 0.0;
    bool   isPlaying     = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};