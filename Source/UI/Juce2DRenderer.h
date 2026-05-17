// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PartialRenderer.h"

#include <JuceHeader.h>

/**
 * PartialRenderer implementation using JUCE's 2D software/CoreGraphics renderer.
 *
 * Before each call to render(), PartialView calls setGraphics() to hand over
 * the juce::Graphics context for that paint() invocation. GPU backends don't
 * need this step — it's specific to the 2D path.
 */
class Juce2DRenderer : public PartialRenderer {
public:
    /** Called by PartialView::paint() before render(). */
    void setGraphics(juce::Graphics& g) noexcept { graphics = &g; }

    void render(const RenderState& state) override;

private:
    void drawFreqGrid      (const RenderState& state, float partialH);
    void drawPartials      (const RenderState& state, float partialH);
    void drawBreakpoints   (const RenderState& state, float partialH);
    void drawInOutMarkers  (const RenderState& state, float partialH, float rulerY);
    void drawPlayhead      (const RenderState& state, float partialH);
    void drawRuler         (const RenderState& state, float rulerY);
    void drawPlayheadHandle(const RenderState& state, float rulerY);

    /** Maps a time value to a canvas x pixel, respecting canvasPadX. */
    static float timeToX(double t, const RenderState& s, float w) noexcept;

    static float      ampToNorm(float amp) noexcept;
    static juce::String formatTime(double seconds);

    juce::Graphics* graphics = nullptr;
};