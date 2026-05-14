// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Model/Project.h"
#include "Model/Selection.h"

#include <JuceHeader.h>

/**
 * Canvas displaying all partials as frequency-vs-time coloured lines.
 *
 * Supports pan (drag) and time-axis zoom (scroll wheel). A horizontal
 * ScrollBar at the bottom is active when the view is zoomed in and the
 * full audio duration exceeds the visible window; it is shown but disabled
 * when the entire file fits in view.
 *
 * Rendering uses JUCE's 2D software renderer until the OpenGL VBO milestone.
 */
class PartialView : public juce::Component,
                    public juce::OpenGLRenderer,
                    public juce::ScrollBar::Listener,
                    public Project::Listener,
                    public Selection::Listener {
public:
    explicit PartialView(Project& project);
    ~PartialView() override;

    // ── juce::Component ───────────────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // ── juce::OpenGLRenderer (stubs until VBO milestone) ─────────────────────
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    // ── juce::ScrollBar::Listener ────────────────────────────────────────────
    void scrollBarMoved(juce::ScrollBar* bar, double newRangeStart) override;

    // ── Project::Listener ────────────────────────────────────────────────────
    void partialsChanged(Project&) override;

    // ── Selection::Listener ──────────────────────────────────────────────────
    void selectionChanged(Selection&) override;

    // ── Playhead ─────────────────────────────────────────────────────────────
    void setPlayheadTime(double seconds);

    // ── View transform ───────────────────────────────────────────────────────
    void setVisibleTimeRange(double startSeconds, double endSeconds);
    void setVisibleFreqRange(float lowHz, float highHz);
    void resetZoom();

private:
    /** Sync scrollbar range and enabled state to current view transform. */
    void updateScrollBar();

    Project&  project;

    // OpenGLContext intentionally not attached — 2D renderer until VBO milestone
    juce::OpenGLContext openGLContext;

    juce::ScrollBar scrollBar{false};  // false = horizontal

    double totalDuration    = 0.0;   ///< full file length, set in resetZoom()
    float  maxFrequency     = 8000.0f; ///< highest partial frequency, set in resetZoom()
    double playheadSeconds  = 0.0;

    // View transform (time and frequency extents currently visible).
    // viewFreqLow is always pinned to 0; vertical zoom scales only viewFreqHigh.
    double viewTimeStart  = 0.0;
    double viewTimeEnd    = 5.0;
    float  viewFreqLow    = 0.0f;
    float  viewFreqHigh   = 8000.0f;

    static constexpr int kScrollBarH = 14;

    juce::Point<float> lastMousePos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PartialView)
};