// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"
#include "Model/Selection.h"

#include <memory>
#include <set>
#include <vector>

/**
 * Plain data snapshot passed to every PartialRenderer implementation.
 * Contains no JUCE rendering types so GPU backends can consume it directly.
 */
struct RenderState {
    const std::vector<std::unique_ptr<Partial>>* partials  = nullptr;
    const Selection*                             selection  = nullptr;

    double viewTimeStart   = 0.0;
    double viewTimeEnd     = 5.0;
    float  viewFreqLow     = 0.0f;
    float  viewFreqHigh    = 8000.0f;
    double playheadSeconds = 0.0;
    double totalDuration   = 0.0;
    float  maxFrequency    = 8000.0f;

    int canvasWidth  = 1;
    int canvasHeight = 1;  ///< full canvas height including ruler
    int rulerH       = 24; ///< pixels reserved at the bottom for the time ruler
    int canvasPadLeft  = 0;  ///< left padding — wide enough to clear frequency labels
    int canvasPadRight = 0;  ///< right padding — small clearance for the playhead handle

    double inPoint  = 0.0;   ///< in marker time (green)
    double outPoint = -1.0;  ///< out marker time (red); -1 = draw at totalDuration

    double playheadForHandle = 0.0;  ///< playhead time used to draw the ruler handle
    double playStartPos      = -1.0; ///< if >= 0, draw a ghost gray line here

    bool                          showBreakpoints = false;
    const std::set<BreakpointRef>* bpSelection    = nullptr; ///< null = none selected

    bool linkHovered  = false;  ///< true when cursor hovers the empty-state "Open a file" link
    bool isAnalysing  = false;  ///< true while analysis is running — suppresses the link text
};

/**
 * Thin rendering interface for the partial canvas.
 *
 * Implementations:
 *   Juce2DRenderer   — CoreGraphics / Direct2D / Cairo via juce::Graphics (current)
 *   MetalRenderer    — macOS Metal (future)
 *   OpenGLRenderer   — Windows / Linux (future)
 *
 * PartialView owns one instance via unique_ptr and calls render() each frame.
 * No rendering types cross the interface boundary.
 */
class PartialRenderer {
public:
    virtual ~PartialRenderer() = default;
    virtual void render(const RenderState& state) = 0;
};