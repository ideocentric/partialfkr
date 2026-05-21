// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PartialView.h"

PartialView::PartialView(Project& p) : project(p)
{
    project.addListener(this);
    project.getSelection().addListener(this);

    auto r = std::make_unique<Juce2DRenderer>();
    renderer2D = r.get();
    renderer   = std::move(r);

    // Horizontal scrollbar (time axis)
    addAndMakeVisible(hScrollBar);
    hScrollBar.addListener(this);
    hScrollBar.setRangeLimits(0.0, 1.0);
    hScrollBar.setCurrentRange(0.0, 1.0);
    hScrollBar.setEnabled(false);

    // Vertical scrollbar (frequency axis — inverted: thumb at top = high freqs)
    addAndMakeVisible(vScrollBar);
    vScrollBar.addListener(this);
    vScrollBar.setRangeLimits(0.0, 1.0);
    vScrollBar.setCurrentRange(0.0, 1.0);
    vScrollBar.setEnabled(false);

    // Zoom buttons
    addAndMakeVisible(hZoomIn);
    addAndMakeVisible(hZoomOut);
    addAndMakeVisible(vZoomIn);
    addAndMakeVisible(vZoomOut);

    hZoomIn.onClick  = [this] { zoomTime(0.65,        0.5f); updateScrollBars(); repaint(); };
    hZoomOut.onClick = [this] { zoomTime(1.0 / 0.65,  0.5f); updateScrollBars(); repaint(); };
    vZoomIn.onClick  = [this] { zoomFrequency(0.65);          updateScrollBars(); repaint(); };
    vZoomOut.onClick = [this] { zoomFrequency(1.0 / 0.65);    updateScrollBars(); repaint(); };

    setWantsKeyboardFocus(true);
    setOpaque(true);
}

PartialView::~PartialView()
{
    project.removeListener(this);
    project.getSelection().removeListener(this);
    hScrollBar.removeListener(this);
    vScrollBar.removeListener(this);
}

// ── Layout ────────────────────────────────────────────────────────────────────
//
//  +──────────────────────────────────+──────+
//  │                                  │  V   │
//  │           canvas                 │  scr │
//  │                                  │  oll │
//  +──────────────────────────────────+──V-──+
//  │  H scrollbar             │H- H+  │V+ V- │
//  +──────────────────────────+────────+──────+

void PartialView::resized()
{
    auto area = getLocalBounds();

    // ── Bottom strip (full width × kChrome) ──────────────────────────────────
    auto hStrip = area.removeFromBottom(kChrome);

    // Corner gap: empty kChrome square at bottom-right
    hStrip.removeFromRight(kChrome);  // empty corner — not assigned to any component

    // H zoom buttons: [H-] then [H+] reading right to left (left to right: [H+][H-][gap])
    hZoomOut.setBounds(hStrip.removeFromRight(kChrome));  // H-
    hZoomIn.setBounds (hStrip.removeFromRight(kChrome));  // H+

    // Horizontal scrollbar fills remaining left portion
    hScrollBar.setBounds(hStrip);

    // ── Right strip (kChrome × remaining height) ──────────────────────────────
    auto vStrip = area.removeFromRight(kChrome);

    // V zoom buttons at the bottom of the right strip (above the corner gap):
    //   bottom → V-,  above that → V+
    vZoomOut.setBounds(vStrip.removeFromBottom(kChrome));  // V-
    vZoomIn.setBounds (vStrip.removeFromBottom(kChrome));  // V+

    // Vertical scrollbar fills the remaining middle portion
    vScrollBar.setBounds(vStrip);
}

// ── Painting ──────────────────────────────────────────────────────────────────

void PartialView::paint(juce::Graphics& g)
{
    if (renderer2D != nullptr)
        renderer2D->setGraphics(g);

    renderer->render(buildRenderState());

    if (analysisProgress >= 0.0f)
        drawAnalysisProgress(g);

    // Fill chrome strips with panel background so they don't show canvas dark color
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(getWidth() - kChrome, 0, kChrome, getHeight());  // right strip + corner
    g.fillRect(0, getHeight() - kChrome, getWidth(), kChrome);  // bottom strip

    if (isRangeSelecting)
    {
        const float canvasW = static_cast<float>(getWidth() - kChrome);

        float x0 = std::min(dragAnchor.x, dragCurrentPos.x);
        float x1 = std::max(dragAnchor.x, dragCurrentPos.x);
        const float y0 = std::min(dragAnchor.y, dragCurrentPos.y);
        const float y1 = std::max(dragAnchor.y, dragCurrentPos.y);

        if (toolMode == ToolMode::DirectSelect && canvasW > 0.0f && totalDuration > 0.0)
        {
            // Snap the time-axis edges to the nearest breakpoint time (uses screenXToTime → padded)
            const double snappedT0 = snapToBreakpointTime(screenXToTime(x0));
            const double snappedT1 = snapToBreakpointTime(screenXToTime(x1));
            const double timeRange = viewTimeEnd - viewTimeStart;
            const float  padL = static_cast<float>(kCanvasPadLeft);
            const float  padR = static_cast<float>(kCanvasPadRight);
            const float  ew  = canvasW - padL - padR;
            x0 = padL + static_cast<float>((snappedT0 - viewTimeStart) / timeRange * ew);
            x1 = padL + static_cast<float>((snappedT1 - viewTimeStart) / timeRange * ew);

            juce::Rectangle<float> rect(x0, y0, x1 - x0, y1 - y0);
            g.setColour(juce::Colour(0x1a4488ff));   // blue tint fill
            g.fillRect(rect);
            g.setColour(juce::Colour(0xcc4488ff));   // blue border
            g.drawRect(rect, 1.0f);
        }
        else
        {
            juce::Rectangle<float> rect(x0, y0, x1 - x0, y1 - y0);
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.fillRect(rect);
            g.setColour(juce::Colours::white.withAlpha(0.40f));
            g.drawRect(rect, 1.0f);
        }
    }
}

RenderState PartialView::buildRenderState() const
{
    RenderState s;
    s.partials        = &project.getPartials();
    s.selection       = &project.getSelection();
    s.viewTimeStart   = viewTimeStart;
    s.viewTimeEnd     = viewTimeEnd;
    s.viewFreqLow     = viewFreqLow;
    s.viewFreqHigh    = viewFreqHigh;
    s.playheadSeconds = playheadSeconds;
    s.totalDuration   = totalDuration;
    s.maxFrequency    = maxFrequency;
    s.canvasWidth     = getWidth()  - kChrome;
    s.canvasHeight    = getHeight() - kChrome;
    s.canvasPadLeft   = kCanvasPadLeft;
    s.canvasPadRight  = kCanvasPadRight;
    s.inPoint         = markerIn;
    s.outPoint        = markerOut;
    s.playheadForHandle = playheadSeconds;
    s.playStartPos      = playStartPos;

    if (toolMode == ToolMode::DirectSelect)
    {
        s.showBreakpoints = true;
        s.bpSelection     = &selectedBreakpoints;
    }
    s.linkHovered = linkHovered;
    s.isAnalysing = (analysisProgress >= 0.0f);
    return s;
}

void PartialView::setAnalysisProgress(float p)
{
    analysisProgress = p;
    if (p >= 0.0f)
    {
        if (!isTimerRunning())
            startTimerHz(30);
    }
    else
    {
        stopTimer();
        pulsePhase = 0.0f;
    }
    repaint();
}

void PartialView::timerCallback()
{
    if (analysisProgress == 0.0f)
    {
        pulsePhase += 0.025f;
        if (pulsePhase > 1.0f) pulsePhase -= 1.0f;
    }
    repaint();
}

void PartialView::drawAnalysisProgress(juce::Graphics& g) const
{
    const float canvasW = static_cast<float>(getWidth()  - kChrome);
    const float canvasH = static_cast<float>(getHeight() - kChrome - kRulerH);

    const float barW = std::min(300.0f, canvasW * 0.5f);
    const float barH = 8.0f;
    const float barX = (canvasW - barW) * 0.5f;
    const float barY = canvasH * 0.5f + 16.0f;

    g.setFont(juce::Font(14.0f));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("Analysing...",
               juce::Rectangle<float>(0.0f, canvasH * 0.5f - 28.0f, canvasW, 18.0f),
               juce::Justification::centred);

    // Trough
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(barX, barY, barW, barH, barH * 0.5f);

    const juce::Colour barCol = juce::Colour::fromHSV(0.33f, 0.9f, 0.6f, 1.0f);

    if (analysisProgress == 0.0f)
    {
        // Indeterminate: comet sweeps left to right
        const float cometW  = barW * 0.35f;
        const float cometX  = barX + pulsePhase * (barW + cometW) - cometW;
        const float clipX   = std::max(barX, cometX);
        const float clipW   = std::min(cometX + cometW, barX + barW) - clipX;
        if (clipW > 0.0f)
        {
            g.setColour(barCol);
            g.fillRoundedRectangle(clipX, barY, clipW, barH, barH * 0.5f);
        }
    }
    else
    {
        // Determinate: fill proportional to overall progress (0.5→1.0 maps to half→full)
        const float fillW = barW * analysisProgress;
        if (fillW > 0.0f)
        {
            g.setColour(barCol);
            g.fillRoundedRectangle(barX, barY, fillW, barH, barH * 0.5f);
        }
    }
}

juce::Rectangle<float> PartialView::getLinkBounds() const noexcept
{
    const float canvasW = static_cast<float>(getWidth()  - kChrome);
    const float canvasH = static_cast<float>(getHeight() - kChrome - kRulerH);
    juce::Font f(14.0f);
    const float textW = f.getStringWidthFloat("Open a file to analyse");
    const float textH = f.getHeight();
    constexpr float padX = 8.0f;
    constexpr float padY = 4.0f;
    const float x = (canvasW - textW) * 0.5f - padX;
    const float y = (canvasH - textH) * 0.5f - padY;
    return { x, y, textW + padX * 2.0f, textH + padY * 2.0f };
}

// ── Tool mode ─────────────────────────────────────────────────────────────────

void PartialView::clearBreakpointSelection()
{
    selectedBreakpoints.clear();
    if (onBreakpointSelectionChanged) onBreakpointSelectionChanged();
    repaint();
}

void PartialView::setToolMode(ToolMode mode)
{
    toolMode = mode;
    selectedBreakpoints.clear();
    setMouseCursor(mode == ToolMode::Selection ? juce::MouseCursor::NormalCursor
                                               : juce::MouseCursor::CrosshairCursor);
    if (onToolModeChanged) onToolModeChanged(mode == ToolMode::DirectSelect);
    if (onBreakpointSelectionChanged) onBreakpointSelectionChanged();
    repaint();
}

void PartialView::setPlayStartPos(double t)
{
    playStartPos = t;
    repaint();
}

void PartialView::setMarkers(double inPt, double outPt)
{
    markerIn  = inPt;
    markerOut = outPt;
    repaint();
}

double PartialView::screenXToTime(float x) const noexcept
{
    const float padL = static_cast<float>(kCanvasPadLeft);
    const float padR = static_cast<float>(kCanvasPadRight);
    const float ew   = static_cast<float>(getWidth() - kChrome) - padL - padR;
    if (ew <= 0.0f) return viewTimeStart;
    const double timeRange = viewTimeEnd - viewTimeStart;
    const double t = viewTimeStart + static_cast<double>((x - padL) / ew) * timeRange;
    return juce::jlimit(viewTimeStart, viewTimeEnd, t);
}

void PartialView::seekToRulerPos(float x)
{
    const double t = screenXToTime(x);
    playheadSeconds = t;
    if (onSeek) onSeek(t);
    repaint();
}

double PartialView::snapToBreakpointTime(double rawTime) const
{
    double nearest = rawTime;
    double minDist = std::numeric_limits<double>::max();
    for (const auto& p : project.getPartials())
    {
        for (const auto& bp : p->breakpoints)
        {
            const double d = std::abs(bp.time - rawTime);
            if (d < minDist) { minDist = d; nearest = bp.time; }
        }
    }
    return nearest;
}

// ── Zoom / pan helpers ────────────────────────────────────────────────────────

void PartialView::zoomTime(double factor, float relX)
{
    const double timeRange = viewTimeEnd - viewTimeStart;
    const double newRange  = juce::jlimit(0.05,
                                          totalDuration > 0.0 ? totalDuration : 60.0,
                                          timeRange * factor);
    const double cursorT   = viewTimeStart + static_cast<double>(relX) * timeRange;
    viewTimeStart = juce::jlimit(0.0, std::max(0.0, totalDuration - newRange),
                                 cursorT - static_cast<double>(relX) * newRange);
    viewTimeEnd   = viewTimeStart + newRange;
}

void PartialView::zoomFrequency(double factor)
{
    const float maxH = maxFrequency * 1.1f;
    viewFreqLow  = 0.0f;
    viewFreqHigh = juce::jlimit(200.0f, maxH,
                                static_cast<float>(viewFreqHigh * factor));
}

void PartialView::panTime(double deltaSeconds)
{
    const double range = viewTimeEnd - viewTimeStart;
    viewTimeStart = juce::jlimit(0.0, std::max(0.0, totalDuration - range),
                                 viewTimeStart + deltaSeconds);
    viewTimeEnd   = viewTimeStart + range;
}

void PartialView::panFrequency(float deltaHz)
{
    const float range = viewFreqHigh - viewFreqLow;
    const float maxH  = maxFrequency * 1.1f;
    viewFreqLow  = juce::jlimit(0.0f, maxH - range, viewFreqLow + deltaHz);
    viewFreqHigh = viewFreqLow + range;
}

// ── Mouse interaction ─────────────────────────────────────────────────────────

void PartialView::mouseMove(const juce::MouseEvent& e)
{
    if (project.getPartials().empty())
    {
        const bool over = getLinkBounds().contains(e.position);
        if (over != linkHovered)
        {
            linkHovered = over;
            repaint();
        }
    }
    else if (linkHovered)
    {
        linkHovered = false;
        repaint();
    }
}

void PartialView::mouseExit(const juce::MouseEvent&)
{
    if (linkHovered)
    {
        linkHovered = false;
        repaint();
    }
}

juce::MouseCursor PartialView::getMouseCursor()
{
    if (linkHovered)
        return juce::MouseCursor::PointingHandCursor;
    return juce::MouseCursor::NormalCursor;
}

void PartialView::mouseDown(const juce::MouseEvent& e)
{
    // Empty-state link click
    if (project.getPartials().empty() && getLinkBounds().contains(e.position))
    {
        if (onAnalyzeRequested) onAnalyzeRequested();
        return;
    }

    grabKeyboardFocus();

    const int   partialAreaH = getHeight() - kChrome - kRulerH;
    const float rulerTop     = static_cast<float>(partialAreaH);
    const float scrollTop    = static_cast<float>(getHeight() - kChrome);

    // Ruler strip: hit-test whether the click landed on the yellow playhead handle
    // (±8 px in X, upper half of ruler in Y).  Only then enter scrub mode.
    const bool inRuler = (e.position.y >= rulerTop && e.position.y < scrollTop);
    if (inRuler)
    {
        const float ew       = static_cast<float>(getWidth() - kChrome - kCanvasPadLeft - kCanvasPadRight);
        const double timeRange = viewTimeEnd - viewTimeStart;
        const float handleX  = static_cast<float>(kCanvasPadLeft)
                               + (timeRange > 0.0 ? static_cast<float>((playheadSeconds - viewTimeStart) / timeRange) * ew : 0.0f);
        const float midRuler = rulerTop + static_cast<float>(kRulerH) * 0.5f;

        isViewScrubbing = (std::abs(e.position.x - handleX) <= 8.0f)
                       && (e.position.y < midRuler);
        if (isViewScrubbing && onScrubStart)
            onScrubStart(screenXToTime(e.position.x));
    }

    // Ruler strip click (not on handle) → seek
    isRulerDragging = inRuler && !isViewScrubbing;
    if (isRulerDragging)
    {
        seekToRulerPos(e.position.x);
        return;
    }

    mouseDownPos         = e.position;
    dragAnchor           = e.position;
    dragCurrentPos       = e.position;
    didDrag              = false;
    isRangeSelecting     = false;

    const bool inPartialArea = (e.position.y < rulerTop);
    dragStartedOnPartial = inPartialArea && (hitTestPartial(e.position) != UINT32_MAX);
}

void PartialView::mouseDrag(const juce::MouseEvent& e)
{
    // Scrub handle drag — update chord position and move the visual playhead
    if (isViewScrubbing)
    {
        const double t = screenXToTime(e.position.x);
        if (onScrubUpdate) onScrubUpdate(t);
        playheadSeconds = t;
        repaint();
        return;
    }

    if (isRulerDragging)
    {
        seekToRulerPos(e.position.x);
        return;
    }

    // Use dragAnchor (original mouseDown pos) for threshold — measures total displacement
    const float dx = e.position.x - dragAnchor.x;
    const float dy = e.position.y - dragAnchor.y;

    if (!didDrag && std::sqrt(dx * dx + dy * dy) < kDragThresh)
        return;

    if (!didDrag)
    {
        didDrag = true;
        if (dragStartedOnPartial)
            mouseDownPos = e.position;  // pan: restart delta to avoid jump
        else
            isRangeSelecting = true;    // background drag: rubber-band select
        return;
    }

    if (isRangeSelecting)
    {
        dragCurrentPos = e.position;
        repaint();
        return;
    }

    // Pan mode
    const float ddx = e.position.x - mouseDownPos.x;
    const float ddy = e.position.y - mouseDownPos.y;
    mouseDownPos    = e.position;

    const double timeRange = viewTimeEnd - viewTimeStart;
    panTime(-static_cast<double>(ddx) / (getWidth() - kChrome) * timeRange);

    const float freqRange = viewFreqHigh - viewFreqLow;
    panFrequency(static_cast<float>(ddy) / (getHeight() - kChrome) * freqRange);

    updateScrollBars();
    repaint();
}

void PartialView::mouseUp(const juce::MouseEvent& e)
{
    // End scrub audio when mouse is released
    if (isViewScrubbing)
    {
        isViewScrubbing = false;
        if (onScrubEnd) onScrubEnd();
    }

    if (isRulerDragging)
    {
        isRulerDragging = false;
        seekToRulerPos(e.position.x);
        return;
    }

    if (didDrag && isRangeSelecting)
    {
        isRangeSelecting = false;

        const float canvasW = static_cast<float>(getWidth() - kChrome);
        const float canvasH = static_cast<float>(getHeight() - kChrome - kRulerH);

        if (canvasW > 0.0f && canvasH > 0.0f)
        {
            const float rx0 = std::min(dragAnchor.x, dragCurrentPos.x);
            const float rx1 = std::max(dragAnchor.x, dragCurrentPos.x);
            const float ry0 = std::min(dragAnchor.y, dragCurrentPos.y);
            const float ry1 = std::max(dragAnchor.y, dragCurrentPos.y);

            const double timeRange = viewTimeEnd  - viewTimeStart;
            const float  freqRange = viewFreqHigh - viewFreqLow;

            double selTimeStart = screenXToTime(rx0);
            double selTimeEnd   = screenXToTime(rx1);
            const float selFreqLow  = viewFreqLow + (1.0f - ry1 / canvasH) * freqRange;
            const float selFreqHigh = viewFreqLow + (1.0f - ry0 / canvasH) * freqRange;

            if (toolMode == ToolMode::DirectSelect)
            {
                // Snap time bounds to nearest breakpoint times
                selTimeStart = snapToBreakpointTime(selTimeStart);
                selTimeEnd   = snapToBreakpointTime(selTimeEnd);

                if (!e.mods.isCommandDown())
                    selectedBreakpoints.clear();

                for (const auto& p : project.getPartials())
                {
                    for (size_t i = 0; i < p->breakpoints.size(); ++i)
                    {
                        const auto& bp = p->breakpoints[i];
                        if (bp.time      >= selTimeStart && bp.time      <= selTimeEnd &&
                            bp.frequency >= selFreqLow   && bp.frequency <= selFreqHigh)
                        {
                            selectedBreakpoints.insert({p->getId(), i});
                        }
                    }
                }
                if (onBreakpointSelectionChanged) onBreakpointSelectionChanged();
            }
            else
            {
                auto& sel = project.getSelection();
                if (!e.mods.isCommandDown())
                    sel.clear();

                for (const auto& p : project.getPartials())
                {
                    if (p->endTime() < selTimeStart || p->startTime() > selTimeEnd)
                        continue;
                    for (const auto& bp : p->breakpoints)
                    {
                        if (bp.time      >= selTimeStart && bp.time      <= selTimeEnd  &&
                            bp.frequency >= selFreqLow   && bp.frequency <= selFreqHigh)
                        {
                            sel.select(p->getId());
                            break;
                        }
                    }
                }
            }
        }

        repaint();
        return;
    }

    isRangeSelecting = false;

    if (didDrag)
        return;  // was a pan gesture, not a click

    // Ignore clicks in the ruler zone
    const int partialAreaH = getHeight() - kChrome - kRulerH;
    if (e.position.y >= static_cast<float>(partialAreaH))
        return;

    const uint32_t hitId = hitTestPartial(e.position);
    auto& sel = project.getSelection();

    if (hitId == UINT32_MAX)
    {
        sel.clear();
        // Store click time as paste insertion point
        pasteTimeSeconds = screenXToTime(e.position.x);
    }
    else if (e.mods.isCommandDown())
    {
        sel.toggle(hitId);
    }
    else
    {
        sel.clear();
        sel.select(hitId);
    }
}

void PartialView::mouseWheelMove(const juce::MouseEvent& e,
                                 const juce::MouseWheelDetails& wheel)
{
    const float absX = std::abs(wheel.deltaX);
    const float absY = std::abs(wheel.deltaY);

    if (e.mods.isShiftDown())
    {
        // Vertical (frequency) zoom — macOS converts Shift+scroll to deltaX
        const float delta = (absX > absY) ? wheel.deltaX : wheel.deltaY;
        if (delta == 0.0f) return;
        zoomFrequency((delta > 0) ? 0.85 : 1.0 / 0.85);
        updateScrollBars();
    }
    else if (e.mods.isAltDown())
    {
        // Horizontal (time) zoom anchored to cursor
        const float delta = (absX > absY) ? wheel.deltaX : wheel.deltaY;
        if (delta == 0.0f) return;
        const float relX = e.position.x / static_cast<float>(getWidth() - kChrome);
        zoomTime((delta > 0) ? 0.85 : 1.0 / 0.85, relX);
        updateScrollBars();
    }
    else
    {
        // Unmodified: pan
        if (absX >= absY)
        {
            // Horizontal swipe → pan time axis
            const double timeRange = viewTimeEnd - viewTimeStart;
            panTime(-static_cast<double>(wheel.deltaX) * timeRange * 0.3);
            updateScrollBars();
        }
        else
        {
            // Vertical scroll → pan frequency axis
            const float freqRange = viewFreqHigh - viewFreqLow;
            panFrequency(wheel.deltaY * freqRange * 0.3f);
            updateScrollBars();
        }
    }

    repaint();
}

bool PartialView::keyPressed(const juce::KeyPress& key)
{
    const double timeStep = (viewTimeEnd - viewTimeStart) * 0.1;
    const float  freqStep = (viewFreqHigh - viewFreqLow) * 0.1f;
    const float  freqPage = viewFreqHigh - viewFreqLow;

    if (key == juce::KeyPress::spaceKey)
    {
        if (onPlayPauseToggle) { onPlayPauseToggle(); return true; }
    }
    else if (key == juce::KeyPress::leftKey)
        panTime(-timeStep);
    else if (key == juce::KeyPress::rightKey)
        panTime(timeStep);
    else if (key == juce::KeyPress::upKey)
        panFrequency(freqStep);
    else if (key == juce::KeyPress::downKey)
        panFrequency(-freqStep);
    else if (key == juce::KeyPress::pageUpKey)
        panFrequency(freqPage);        // scroll frequency view up one page
    else if (key == juce::KeyPress::pageDownKey)
        panFrequency(-freqPage);       // scroll frequency view down one page
    else if (key == juce::KeyPress::homeKey)
    {
        const double range = viewTimeEnd - viewTimeStart;
        viewTimeStart = 0.0;
        viewTimeEnd   = range;
    }
    else if (key == juce::KeyPress::endKey)
    {
        const double range = viewTimeEnd - viewTimeStart;
        viewTimeEnd   = totalDuration;
        viewTimeStart = std::max(0.0, totalDuration - range);
    }
    else if (key == juce::KeyPress('0', juce::ModifierKeys::commandModifier, 0))
    {
        resetZoom();
        return true;
    }
    else if (key == juce::KeyPress('v', 0, 0))
    {
        setToolMode(ToolMode::Selection);
        return true;
    }
    else if (key == juce::KeyPress('a', 0, 0))
    {
        setToolMode(ToolMode::DirectSelect);
        return true;
    }
    else if (key == juce::KeyPress('t', 0, 0))
    {
        if (onStretch) { onStretch(); return true; }
    }
    else if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (onUndo) { onUndo(); return true; }
    }
    else if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier
                                          | juce::ModifierKeys::shiftModifier, 0))
    {
        if (onRedo) { onRedo(); return true; }
    }
    else if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (onCopy) { onCopy(); return true; }
    }
    else if (key == juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0))
    {
        if (onCut) { onCut(); return true; }
    }
    else if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (onPaste) { onPaste(); return true; }
    }
    else if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (toolMode == ToolMode::DirectSelect && !selectedBreakpoints.empty())
        {
            if (onBreakpointDelete) { onBreakpointDelete(); return true; }
        }
        else if (onDelete) { onDelete(); return true; }
    }
    else if (key == juce::KeyPress('i', juce::ModifierKeys::commandModifier
                                              | juce::ModifierKeys::shiftModifier, 0))
    {
        // Cmd+Shift+I — invert selection (Option+I is a macOS dead key)
        std::vector<uint32_t> allIds;
        for (const auto& p : project.getPartials())
            allIds.push_back(p->getId());
        project.getSelection().invert(allIds);
        return true;
    }
    else
        return false;

    updateScrollBars();
    repaint();
    return true;
}

// ── Scrollbars ────────────────────────────────────────────────────────────────

uint32_t PartialView::hitTestPartial(juce::Point<float> pos) const
{
    const float  canvasW  = static_cast<float>(getWidth() - kChrome);
    const float  canvasH  = static_cast<float>(getHeight() - kChrome - kRulerH);
    if (canvasW <= 0.0f || canvasH <= 0.0f) return UINT32_MAX;

    const double timeRange = viewTimeEnd - viewTimeStart;
    const float  freqRange = viewFreqHigh - viewFreqLow;
    if (timeRange <= 0.0 || freqRange <= 0.0f) return UINT32_MAX;

    // Screen position → (time, frequency)
    const double clickTime = screenXToTime(pos.x);
    const float  clickFreq = viewFreqLow + (1.0f - pos.y / canvasH) * freqRange;

    uint32_t bestId     = UINT32_MAX;
    float    bestDistPx = kHitThreshPx;

    for (const auto& partial : project.getPartials())
    {
        if (!partial->containsTime(clickTime))
            continue;

        // Interpolate partial frequency at clickTime
        const auto& bps = partial->breakpoints;
        float interpFreq = 0.0f;
        for (size_t i = 0; i + 1 < bps.size(); ++i)
        {
            if (bps[i].time <= clickTime && clickTime <= bps[i + 1].time)
            {
                const double span  = bps[i + 1].time - bps[i].time;
                const float  alpha = span > 0.0 ? static_cast<float>((clickTime - bps[i].time) / span) : 0.0f;
                interpFreq = bps[i].frequency + alpha * (bps[i + 1].frequency - bps[i].frequency);
                break;
            }
        }

        // Convert interpolated frequency to screen Y and measure distance
        const float screenY  = (1.0f - (interpFreq - viewFreqLow) / freqRange) * canvasH;
        const float distPx   = std::abs(pos.y - screenY);

        if (distPx < bestDistPx)
        {
            bestDistPx = distPx;
            bestId     = partial->getId();
        }
    }

    return bestId;
}

void PartialView::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScrollBar)
    {
        const double viewDuration = viewTimeEnd - viewTimeStart;
        viewTimeStart = newRangeStart;
        viewTimeEnd   = viewTimeStart + viewDuration;
    }
    else if (bar == &vScrollBar)
    {
        // Vertical scrollbar is inverted: position 0 = top of canvas = high freqs.
        // newRangeStart is distance from the TOP, so:
        //   viewFreqHigh = totalFreqRange - newRangeStart
        const float totalFreqRange = maxFrequency * 1.1f;
        const float viewRange      = viewFreqHigh - viewFreqLow;
        viewFreqHigh = totalFreqRange - static_cast<float>(newRangeStart);
        viewFreqLow  = viewFreqHigh - viewRange;
    }

    repaint();
}

void PartialView::updateScrollBars()
{
    // Horizontal scrollbar
    if (totalDuration > 0.0)
    {
        const double viewDuration = viewTimeEnd - viewTimeStart;
        hScrollBar.setRangeLimits(0.0, totalDuration);
        hScrollBar.removeListener(this);
        hScrollBar.setCurrentRange(viewTimeStart, viewDuration);
        hScrollBar.addListener(this);
        hScrollBar.setEnabled(viewDuration < totalDuration - 0.01);
    }
    else
    {
        hScrollBar.setEnabled(false);
    }

    // Vertical scrollbar (inverted: position = totalFreqRange - viewFreqHigh)
    const float totalFreqRange = maxFrequency * 1.1f;
    if (totalFreqRange > 0.0f)
    {
        const float viewRange = viewFreqHigh - viewFreqLow;
        const float vPos      = totalFreqRange - viewFreqHigh;  // inverted position
        vScrollBar.setRangeLimits(0.0, static_cast<double>(totalFreqRange));
        vScrollBar.removeListener(this);
        vScrollBar.setCurrentRange(static_cast<double>(vPos),
                                   static_cast<double>(viewRange));
        vScrollBar.addListener(this);
        vScrollBar.setEnabled(viewRange < totalFreqRange - 1.0f);
    }
    else
    {
        vScrollBar.setEnabled(false);
    }
}

// ── OpenGL stubs ──────────────────────────────────────────────────────────────

void PartialView::newOpenGLContextCreated() {}
void PartialView::renderOpenGL()            {}
void PartialView::openGLContextClosing()    {}

// ── Listeners ─────────────────────────────────────────────────────────────────

void PartialView::partialsChanged(Project&)
{
    const auto& partials = project.getPartials();
    const bool  firstLoad = (totalDuration <= 0.0);

    if (partials.empty())
    {
        totalDuration = 0.0;
        maxFrequency  = 100.0f;
        updateScrollBars();
        repaint();
        return;
    }

    if (firstLoad)
    {
        resetZoom();
        return;
    }

    // Edit operation (delete, paste…): recompute data bounds but keep the
    // current view transform so the user doesn't lose their zoom/pan state.
    double newDuration = 0.0;
    float  newMaxFreq  = 100.0f;
    for (const auto& p : partials)
    {
        newDuration = std::max(newDuration, p->endTime());
        for (const auto& bp : p->breakpoints)
            newMaxFreq = std::max(newMaxFreq, static_cast<float>(bp.frequency));
    }
    totalDuration = newDuration;
    maxFrequency  = newMaxFreq;
    updateScrollBars();
    repaint();
}

void PartialView::selectionChanged(Selection&) { repaint(); }

// ── Public control ────────────────────────────────────────────────────────────

void PartialView::setPlayheadTime(double seconds)
{
    playheadSeconds = seconds;
    repaint();
}

void PartialView::setVisibleTimeRange(double start, double end)
{
    viewTimeStart = start;
    viewTimeEnd   = end;
    updateScrollBars();
    repaint();
}

void PartialView::setVisibleFreqRange(float low, float high)
{
    viewFreqLow  = low;
    viewFreqHigh = high;
    updateScrollBars();
    repaint();
}

void PartialView::resetZoom()
{
    const auto& partials = project.getPartials();
    if (partials.empty())
        return;

    totalDuration = 0.0;
    maxFrequency  = 100.0f;
    for (const auto& p : partials)
    {
        totalDuration = std::max(totalDuration, p->endTime());
        for (const auto& bp : p->breakpoints)
            maxFrequency = std::max(maxFrequency, bp.frequency);
    }

    viewTimeStart = 0.0;
    viewTimeEnd   = totalDuration * 1.02;
    viewFreqLow   = 0.0f;
    viewFreqHigh  = maxFrequency * 1.1f;

    updateScrollBars();
    repaint();
}