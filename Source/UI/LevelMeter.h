// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>
#include <atomic>

/**
 * Vertical stereo peak meter with peak-hold and clip indicator.
 *
 * The audio thread writes linear peak levels into the two atomics every block.
 * A 30 fps timer reads them, applies peak-hold ballistics, and repaints.
 * Clicking the component resets the clip indicators.
 *
 * Internal layout (left to right): L bar | R bar | dB scale strip.
 */
class LevelMeter : public juce::Component,
                   private juce::Timer {
public:
    LevelMeter(const std::atomic<float>& leftLevel,
               const std::atomic<float>& rightLevel);
    ~LevelMeter() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;  ///< resets clip indicators

    static float toDb(float linear) noexcept;
    static float dbToY(float db, float height) noexcept;  ///< 0 = top (0 dBFS), height = bottom (kMinDb)

    static constexpr float kMinDb    = -60.0f;
    static constexpr float kMaxDb    =   0.0f;
    static constexpr float kYellowDb =  -6.0f;
    static constexpr float kRedDb    =  -3.0f;

    static constexpr int kBarW          = 33;   ///< width of each channel bar
    static constexpr int kScaleW        = 28;   ///< width of the dB scale strip
    /// Total component width that exactly fits both bars + scale with no dead space
    static constexpr int kPreferredWidth = 2 * kBarW + 2 + 3 + kScaleW + 2;

private:
    void timerCallback() override;

    const std::atomic<float>& levelL;
    const std::atomic<float>& levelR;

    float dispL = 0.0f, dispR = 0.0f;
    float holdL = 0.0f, holdR = 0.0f;
    int   holdTickL = 0, holdTickR = 0;
    bool  clipL = false, clipR = false;

    static constexpr float kDecayPerFrame = 0.85f;
    static constexpr int   kHoldFrames    = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};