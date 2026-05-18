// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Juce2DRenderer.h"
#include "PartialRenderer.h"
#include "ZoomButton.h"
#include "Model/Project.h"
#include "Model/Selection.h"

#include <JuceHeader.h>

#include <memory>
#include <set>

/**
 * Canvas component for the partial display.
 *
 * Input map:
 *   V                      → Selection tool (whole-partial select)
 *   A                      → Direct Select tool (breakpoint-level select, snaps to nodes)
 *   Option + scroll        → horizontal (time) zoom, anchored to cursor
 *   Shift  + scroll        → vertical (freq) zoom, pinned to 0 Hz
 *   Unmodified vert scroll → pan frequency axis
 *   Unmodified horiz scroll / trackpad swipe → pan time axis
 *   Arrow L/R              → pan time by 10% of view
 *   Arrow U/D              → pan frequency by 10% of view
 *   Page Up/Down           → pan time by one full view width
 *   Home / End             → jump to file start / end (keep zoom)
 *   H+/H- buttons          → horizontal zoom in / out (centred)
 *   V+/V- buttons          → vertical zoom in / out
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
    void mouseUp  (const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // ── juce::OpenGLRenderer (stubs until Metal/GL milestone) ────────────────
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    // ── juce::ScrollBar::Listener ────────────────────────────────────────────
    void scrollBarMoved(juce::ScrollBar* bar, double newRangeStart) override;

    // ── Project::Listener ────────────────────────────────────────────────────
    void partialsChanged(Project&) override;

    // ── Selection::Listener ──────────────────────────────────────────────────
    void selectionChanged(Selection&) override;

    // ── Playhead & markers ───────────────────────────────────────────────────
    void setPlayheadTime(double seconds);
    void setMarkers(double inPt, double outPt);
    void setPlayStartPos(double t);   ///< show ghost line; pass -1 to hide

    // ── Edit callbacks (wired by MainComponent) ───────────────────────────────
    std::function<void(double)> onSeek;  ///< ruler drag → MainComponent updates synth + transport

    enum class ToolMode { Selection, DirectSelect };
    [[nodiscard]] ToolMode getToolMode() const noexcept { return toolMode; }

    /** Fired after a tool mode change (V/A keys or setToolMode call). Argument: true = DirectSelect. */
    std::function<void(bool)> onToolModeChanged;

    /** Fired whenever the breakpoint selection changes (insert, clear, or tool mode switch). */
    std::function<void()> onBreakpointSelectionChanged;

    void setToolMode(ToolMode mode);

    /** Called by MainComponent to delete the currently selected breakpoints. */
    [[nodiscard]] const std::set<BreakpointRef>& getSelectedBreakpoints() const noexcept
    {
        return selectedBreakpoints;
    }
    void clearBreakpointSelection();

    // ── Edit callbacks (wired by MainComponent) ───────────────────────────────
    std::function<void()> onCut;
    std::function<void()> onCopy;
    std::function<void()> onPaste;
    std::function<void()> onDelete;
    std::function<void()> onBreakpointDelete;  ///< fired in DirectSelect mode instead of onDelete
    std::function<void()> onStretch;           ///< T key — stretch/compress selected material
    std::function<void()> onPlayPauseToggle;   ///< Space bar — play / pause / resume

    // ── Scrub (Option+drag) ───────────────────────────────────────────────────
    std::function<void(double)> onScrubStart;   ///< Option mouseDown: time under cursor
    std::function<void(double)> onScrubUpdate;  ///< Option mouseDrag: new time
    std::function<void()>       onScrubEnd;     ///< mouseUp: release scrub
    std::function<void()> onUndo;
    std::function<void()> onRedo;

    /** Time position set by the last background click — used as paste insertion point. */
    [[nodiscard]] double getPasteTime() const noexcept { return pasteTimeSeconds; }

    // ── View transform ───────────────────────────────────────────────────────
    void setVisibleTimeRange(double startSeconds, double endSeconds);
    void setVisibleFreqRange(float lowHz, float highHz);
    void resetZoom();
    void zoomIn()  { zoomTime(0.65,       0.5f); updateScrollBars(); repaint(); }
    void zoomOut() { zoomTime(1.0/0.65,   0.5f); updateScrollBars(); repaint(); }

    [[nodiscard]] double getViewTimeStart() const noexcept { return viewTimeStart; }
    [[nodiscard]] double getViewTimeEnd()   const noexcept { return viewTimeEnd; }
    [[nodiscard]] float  getViewFreqLow()   const noexcept { return viewFreqLow; }
    [[nodiscard]] float  getViewFreqHigh()  const noexcept { return viewFreqHigh; }

private:
    // ── Tool mode ─────────────────────────────────────────────────────────────

    // ── Zoom / pan helpers ────────────────────────────────────────────────────
    void zoomTime(double factor, float relX = 0.5f);
    void zoomFrequency(double factor);
    void panTime(double deltaSeconds);
    void panFrequency(float deltaHz);

    void        updateScrollBars();
    RenderState buildRenderState() const;

    /** Returns the partial ID nearest to screenPos, or UINT32_MAX if none within threshold. */
    uint32_t hitTestPartial(juce::Point<float> screenPos) const;

    /** Snaps rawTime to the nearest breakpoint time across all visible partials. */
    double snapToBreakpointTime(double rawTime) const;

    /** Converts a screen x pixel (within the canvas) to a time value. */
    [[nodiscard]] double screenXToTime(float x) const noexcept;

    /** Seeks the playhead to the time at the given screen x; fires onSeek. */
    void seekToRulerPos(float x);

    Project& project;

    std::unique_ptr<PartialRenderer> renderer;
    Juce2DRenderer*                  renderer2D = nullptr;

    juce::OpenGLContext openGLContext;

    // Scrollbars
    juce::ScrollBar hScrollBar{false};  // horizontal — time axis
    juce::ScrollBar vScrollBar{true};   // vertical   — frequency axis (inverted)

    // Zoom buttons (vector-drawn) — tool mode buttons moved to ReductionPanel
    ZoomButton hZoomIn {ZoomButton::Sign::Plus};
    ZoomButton hZoomOut{ZoomButton::Sign::Minus};
    ZoomButton vZoomIn {ZoomButton::Sign::Plus};
    ZoomButton vZoomOut{ZoomButton::Sign::Minus};

    double totalDuration   = 0.0;
    float  maxFrequency    = 8000.0f;
    double playheadSeconds = 0.0;

    double viewTimeStart = 0.0;
    double viewTimeEnd   = 5.0;
    float  viewFreqLow   = 0.0f;
    float  viewFreqHigh  = 8000.0f;

    static constexpr int   kChrome      = 18;
    static constexpr int   kRulerH      = 24;
    static constexpr int   kCanvasPadLeft  = 72;  ///< left padding clears frequency labels (~64px wide)
    static constexpr int   kCanvasPadRight =  8;  ///< right padding clears playhead handle
    static constexpr float kDragThresh  = 4.0f;
    static constexpr float kHitThreshPx = 6.0f;

    ToolMode toolMode = ToolMode::Selection;
    std::set<BreakpointRef> selectedBreakpoints;

    double markerIn  = 0.0;
    double markerOut = -1.0;  ///< -1 = draw at totalDuration
    double playStartPos    = -1.0;  ///< ghost line; -1 = hidden
    bool   isRulerDragging  = false;
    bool   isViewScrubbing  = false;  ///< Option+drag scrub in progress

    juce::Point<float> mouseDownPos;
    juce::Point<float> dragAnchor;       ///< original mouseDown position; rect corner for range select
    juce::Point<float> dragCurrentPos;   ///< updated each mouseDrag when range-selecting
    bool               didDrag              = false;
    bool               dragStartedOnPartial = false;
    bool               isRangeSelecting     = false;
    double             pasteTimeSeconds = 0.0; ///< time set by last background click

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PartialView)
};