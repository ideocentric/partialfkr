// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PartialView.h"

PartialView::PartialView(Project& p) : project(p)
{
    project.addListener(this);
    project.getSelection().addListener(this);

    addAndMakeVisible(scrollBar);
    scrollBar.addListener(this);
    scrollBar.setRangeLimits(0.0, 1.0);
    scrollBar.setCurrentRange(0.0, 1.0);
    scrollBar.setEnabled(false);

    setOpaque(true);
}

PartialView::~PartialView()
{
    project.removeListener(this);
    project.getSelection().removeListener(this);
    scrollBar.removeListener(this);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void PartialView::resized()
{
    auto area = getLocalBounds();
    scrollBar.setBounds(area.removeFromBottom(kScrollBarH));
}

// ── Painting ──────────────────────────────────────────────────────────────────

void PartialView::paint(juce::Graphics& g)
{
    auto canvas = getLocalBounds().withTrimmedBottom(kScrollBarH).toFloat();
    const float w = canvas.getWidth();
    const float h = canvas.getHeight();

    g.fillAll(juce::Colour(0xff0d0d0d));

    const auto& partials = project.getPartials();
    if (partials.empty())
    {
        g.setColour(juce::Colours::darkgrey);
        g.setFont(14.0f);
        g.drawText("Open a file to analyse",
                   getLocalBounds().withTrimmedBottom(kScrollBarH),
                   juce::Justification::centred);
        return;
    }

    const double timeRange = viewTimeEnd - viewTimeStart;
    const float  freqRange = viewFreqHigh - viewFreqLow;
    if (timeRange <= 0.0 || freqRange <= 0.0f)
        return;

    // Map linear amplitude to a perceptually even 0–1 value via dB.
    // Floor at -80 dB (1e-4 linear), ceiling at 0 dB (1.0 linear).
    auto ampToNorm = [](float amp) -> float {
        const float db = 20.0f * std::log10(std::max(amp, 1e-4f));
        return juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f);
    };

    for (const auto& partial : partials)
    {
        if (partial->muted.load(std::memory_order_relaxed))
            continue;

        const auto& bps = partial->breakpoints;
        if (bps.size() < 2)
            continue;

        const bool  selected = project.getSelection().isSelected(partial->getId());
        const float norm     = ampToNorm(partial->peakAmplitude());

        juce::Colour colour;
        float        weight;

        if (selected)
        {
            colour = juce::Colours::white;
            weight = 1.5f;
        }
        else
        {
            // Green hue (120°), full saturation, brightness driven by dB level.
            // Quiet partials are dark; loud partials are bright.
            const float brightness = 0.15f + norm * 0.85f;
            colour = juce::Colour::fromHSV(0.33f, 0.9f, brightness, 1.0f);

            // Line weight also scales with amplitude: whisper-thin to bold.
            weight = 0.3f + norm * 2.2f;
        }

        g.setColour(colour);

        juce::Path path;
        bool started = false;
        for (const auto& bp : bps)
        {
            const float x = static_cast<float>((bp.time - viewTimeStart) / timeRange) * w;
            const float y = (1.0f - (bp.frequency - viewFreqLow) / freqRange) * h;

            if (x < -2.0f || x > w + 2.0f) { started = false; continue; }
            if (!started) { path.startNewSubPath(x, y); started = true; }
            else          path.lineTo(x, y);
        }
        g.strokePath(path, juce::PathStrokeType(weight));
    }

    // Playhead
    if (playheadSeconds >= viewTimeStart && playheadSeconds <= viewTimeEnd)
    {
        const float px = static_cast<float>((playheadSeconds - viewTimeStart) / timeRange) * w;
        g.setColour(juce::Colours::yellow.withAlpha(0.75f));
        g.drawVerticalLine(static_cast<int>(px), 0.0f, h);
    }

    // Frequency gridlines
    g.setFont(10.0f);
    const float gridFreqs[] = { 100.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f };
    for (float freq : gridFreqs)
    {
        if (freq < viewFreqLow || freq > viewFreqHigh)
            continue;
        const float y = (1.0f - (freq - viewFreqLow) / freqRange) * h;
        g.setColour(juce::Colours::grey.withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        g.setColour(juce::Colours::grey.withAlpha(0.6f));
        g.drawText(juce::String(static_cast<int>(freq)) + " Hz",
                   4, static_cast<int>(y) - 10, 60, 12,
                   juce::Justification::left);
    }
}

// ── Mouse interaction ─────────────────────────────────────────────────────────

void PartialView::mouseDown(const juce::MouseEvent& e)
{
    lastMousePos = e.position;
}

void PartialView::mouseDrag(const juce::MouseEvent& e)
{
    const float dx = e.position.x - lastMousePos.x;
    const float dy = e.position.y - lastMousePos.y;
    lastMousePos   = e.position;

    const double timeRange = viewTimeEnd - viewTimeStart;
    const double timeDelta = -static_cast<double>(dx) / getWidth() * timeRange;
    viewTimeStart = juce::jlimit(0.0, std::max(0.0, totalDuration - timeRange),
                                 viewTimeStart + timeDelta);
    viewTimeEnd = viewTimeStart + timeRange;

    const float freqRange = viewFreqHigh - viewFreqLow;
    const float freqDelta = static_cast<float>(dy) / getHeight() * freqRange;
    viewFreqLow  += freqDelta;
    viewFreqHigh += freqDelta;

    updateScrollBar();
    repaint();
}

void PartialView::mouseWheelMove(const juce::MouseEvent& e,
                                 const juce::MouseWheelDetails& wheel)
{
    // On macOS, Shift+vertical-scroll is remapped by the OS to horizontal scroll
    // (deltaX), so deltaY is 0. Use whichever axis has movement.
    const float delta = (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
                            ? wheel.deltaX
                            : wheel.deltaY;

    if (delta == 0.0f)
        return;

    const double factor = (delta > 0) ? 0.85 : 1.0 / 0.85;

    if (e.mods.isShiftDown())
    {
        // ── Vertical zoom, pinned to bottom (viewFreqLow stays at 0) ──────────
        const float maxH = maxFrequency * 1.1f;
        const float newH = juce::jlimit(200.0f, maxH,
                                        static_cast<float>(viewFreqHigh * factor));
        viewFreqLow  = 0.0f;
        viewFreqHigh = newH;
    }
    else
    {
        // ── Horizontal (time) zoom, anchored to cursor ─────────────────────────
        const double timeRange = viewTimeEnd - viewTimeStart;
        const double newRange  = juce::jlimit(0.05,
                                              totalDuration > 0.0 ? totalDuration : 60.0,
                                              timeRange * factor);
        const float  relX    = e.position.x / static_cast<float>(getWidth());
        const double cursorT = viewTimeStart + static_cast<double>(relX) * timeRange;
        viewTimeStart = juce::jlimit(0.0, std::max(0.0, totalDuration - newRange),
                                     cursorT - static_cast<double>(relX) * newRange);
        viewTimeEnd   = viewTimeStart + newRange;
        updateScrollBar();
    }

    repaint();
}

// ── ScrollBar::Listener ───────────────────────────────────────────────────────

void PartialView::scrollBarMoved(juce::ScrollBar* /*bar*/, double newRangeStart)
{
    const double viewDuration = viewTimeEnd - viewTimeStart;
    viewTimeStart = newRangeStart;
    viewTimeEnd   = viewTimeStart + viewDuration;
    repaint();
}

// ── ScrollBar sync ────────────────────────────────────────────────────────────

void PartialView::updateScrollBar()
{
    if (totalDuration <= 0.0)
    {
        scrollBar.setEnabled(false);
        return;
    }

    const double viewDuration = viewTimeEnd - viewTimeStart;
    const bool   canScroll    = viewDuration < totalDuration - 0.01;

    scrollBar.setRangeLimits(0.0, totalDuration);
    // Temporarily remove listener so our own update doesn't recurse
    scrollBar.removeListener(this);
    scrollBar.setCurrentRange(viewTimeStart, viewDuration);
    scrollBar.addListener(this);
    scrollBar.setEnabled(canScroll);
}

// ── OpenGL stubs ──────────────────────────────────────────────────────────────

void PartialView::newOpenGLContextCreated() {}
void PartialView::renderOpenGL()            {}
void PartialView::openGLContextClosing()    {}

// ── Project/Selection listeners ───────────────────────────────────────────────

void PartialView::partialsChanged(Project&)
{
    if (!project.getPartials().empty())
        resetZoom();
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
    updateScrollBar();
    repaint();
}

void PartialView::setVisibleFreqRange(float low, float high)
{
    viewFreqLow  = low;
    viewFreqHigh = high;
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

    updateScrollBar();
    repaint();
}