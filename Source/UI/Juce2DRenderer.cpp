// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Juce2DRenderer.h"

#include <cmath>

// ── Public ────────────────────────────────────────────────────────────────────

void Juce2DRenderer::render(const RenderState& state)
{
    if (graphics == nullptr || state.partials == nullptr)
        return;

    const float w        = static_cast<float>(state.canvasWidth);
    const float totalH   = static_cast<float>(state.canvasHeight);
    const float rulerY   = totalH - static_cast<float>(state.rulerH);
    const float partialH = rulerY;  // partials draw in [0, rulerY)

    graphics->fillAll(juce::Colour(0xff0d0d0d));

    if (state.partials->empty())
    {
        graphics->setColour(juce::Colours::darkgrey);
        graphics->setFont(14.0f);
        graphics->drawText("Open a file to analyse",
                           juce::Rectangle<int>(0, 0,
                               state.canvasWidth,
                               static_cast<int>(partialH)),
                           juce::Justification::centred);
        drawRuler(state, rulerY);
        return;
    }

    const double timeRange = state.viewTimeEnd - state.viewTimeStart;
    const float  freqRange = state.viewFreqHigh - state.viewFreqLow;
    if (timeRange <= 0.0 || freqRange <= 0.0f)
    {
        drawRuler(state, rulerY);
        return;
    }

    drawFreqGrid      (state, partialH);
    drawInOutMarkers  (state, partialH, rulerY);
    drawPartials      (state, partialH);
    if (state.showBreakpoints)
        drawBreakpoints(state, partialH);
    drawPlayhead      (state, partialH);
    drawRuler         (state, rulerY);
    drawPlayheadHandle(state, rulerY);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

float Juce2DRenderer::timeToX(double t, const RenderState& s, float w) noexcept
{
    const double timeRange = s.viewTimeEnd - s.viewTimeStart;
    const float  padL = static_cast<float>(s.canvasPadLeft);
    if (timeRange <= 0.0) return padL;
    const float ew = w - padL - static_cast<float>(s.canvasPadRight);
    return padL + static_cast<float>((t - s.viewTimeStart) / timeRange) * ew;
}

// ── Private — in/out markers ─────────────────────────────────────────────────

void Juce2DRenderer::drawInOutMarkers(const RenderState& state, float partialH, float rulerY)
{
    const float  w       = static_cast<float>(state.canvasWidth);
    const double outTime = (state.outPoint >= 0.0) ? state.outPoint : state.totalDuration;

    // Full-height lines
    const float inX  = timeToX(state.inPoint, state, w);
    const float outX = timeToX(outTime,        state, w);

    // Shaded region between in and out (very subtle)
    // In line
    graphics->setColour(juce::Colour(0xff44ff88).withAlpha(0.55f));
    graphics->drawVerticalLine(static_cast<int>(inX),  0.0f, rulerY + static_cast<float>(state.rulerH));

    // Out line
    graphics->setColour(juce::Colour(0xffff4444).withAlpha(0.55f));
    graphics->drawVerticalLine(static_cast<int>(outX), 0.0f, rulerY + static_cast<float>(state.rulerH));

    // Small triangles pointing down from the ruler top edge
    const float rY   = rulerY;
    const float triH = static_cast<float>(state.rulerH) * 0.42f;
    const float triW = 5.0f;

    // In triangle (green)
    graphics->setColour(juce::Colour(0xff44ff88));
    {
        juce::Path t;
        t.addTriangle(inX - triW, rY, inX + triW, rY, inX, rY + triH);
        graphics->fillPath(t);
    }
    // Out triangle (red)
    graphics->setColour(juce::Colour(0xffff4444));
    {
        juce::Path t;
        t.addTriangle(outX - triW, rY, outX + triW, rY, outX, rY + triH);
        graphics->fillPath(t);
    }
}

// ── Private — partials area ───────────────────────────────────────────────────

void Juce2DRenderer::drawBreakpoints(const RenderState& state, float partialH)
{
    const float  w         = static_cast<float>(state.canvasWidth);
    const double timeRange = state.viewTimeEnd - state.viewTimeStart;
    const float  freqRange = state.viewFreqHigh - state.viewFreqLow;
    constexpr float kR = 2.5f;  // half-size of node square

    for (const auto& partial : *state.partials)
    {
        if (partial->muted.load(std::memory_order_relaxed))
            continue;

        const uint32_t id = partial->getId();
        for (size_t i = 0; i < partial->breakpoints.size(); ++i)
        {
            const auto& bp = partial->breakpoints[i];
            const float x  = timeToX(bp.time, state, w);
            if (x < -kR || x > w + kR) continue;
            const float y  = (1.0f - (bp.frequency - state.viewFreqLow) / freqRange) * partialH;

            const bool selected = state.bpSelection &&
                                  state.bpSelection->count({id, i}) > 0;

            juce::Rectangle<float> node(x - kR, y - kR, kR * 2.0f, kR * 2.0f);
            graphics->setColour(selected ? juce::Colours::white
                                         : juce::Colours::white.withAlpha(0.25f));
            graphics->fillRect(node);
        }
    }
}

void Juce2DRenderer::drawPartials(const RenderState& state, float partialH)
{
    const float  w         = static_cast<float>(state.canvasWidth);
    const double timeRange = state.viewTimeEnd - state.viewTimeStart;
    const float  freqRange = state.viewFreqHigh - state.viewFreqLow;

    for (const auto& partial : *state.partials)
    {
        if (partial->muted.load(std::memory_order_relaxed))
            continue;

        const auto& bps = partial->breakpoints;
        if (bps.size() < 2)
            continue;

        const bool  selected = state.selection && state.selection->isSelected(partial->getId());
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
            const float brightness = 0.15f + norm * 0.85f;
            colour = juce::Colour::fromHSV(0.33f, 0.9f, brightness, 1.0f);
            weight = 0.3f + norm * 2.2f;
        }

        graphics->setColour(colour);

        juce::Path path;
        bool started = false;
        for (const auto& bp : bps)
        {
            const float x = timeToX(bp.time, state, w);
            const float y = (1.0f - (bp.frequency - state.viewFreqLow) / freqRange) * partialH;

            if (x < -2.0f || x > w + 2.0f) { started = false; continue; }
            if (!started) { path.startNewSubPath(x, y); started = true; }
            else          path.lineTo(x, y);
        }
        graphics->strokePath(path, juce::PathStrokeType(weight));
    }
}

void Juce2DRenderer::drawPlayhead(const RenderState& state, float partialH)
{
    const float w = static_cast<float>(state.canvasWidth);

    // Ghost line — where playback started from
    if (state.playStartPos >= state.viewTimeStart && state.playStartPos <= state.viewTimeEnd)
    {
        const float gx = timeToX(state.playStartPos, state, w);
        graphics->setColour(juce::Colours::grey.withAlpha(0.35f));
        graphics->drawVerticalLine(static_cast<int>(gx), 0.0f, partialH);
    }

    if (state.playheadSeconds < state.viewTimeStart || state.playheadSeconds > state.viewTimeEnd)
        return;

    const float px = timeToX(state.playheadSeconds, state, w);
    graphics->setColour(juce::Colours::yellow.withAlpha(0.75f));
    graphics->drawVerticalLine(static_cast<int>(px), 0.0f, partialH);
}

void Juce2DRenderer::drawPlayheadHandle(const RenderState& state, float rulerY)
{
    const float w  = static_cast<float>(state.canvasWidth);
    const float px = timeToX(state.playheadForHandle, state, w);

    // Yellow line through the ruler
    graphics->setColour(juce::Colours::yellow);
    graphics->drawVerticalLine(static_cast<int>(px), rulerY, rulerY + static_cast<float>(state.rulerH));

    // Downward-pointing triangle handle (half the ruler height)
    const float triH = static_cast<float>(state.rulerH) * 0.50f;
    const float triW = 6.0f;
    juce::Path handle;
    handle.addTriangle(px - triW, rulerY, px + triW, rulerY, px, rulerY + triH);
    graphics->fillPath(handle);
}

void Juce2DRenderer::drawFreqGrid(const RenderState& state, float partialH)
{
    const float w         = static_cast<float>(state.canvasWidth);
    const float freqRange = state.viewFreqHigh - state.viewFreqLow;

    graphics->setFont(10.0f);

    const float gridFreqs[] = { 50.f, 100.f, 200.f, 500.f, 1000.f,
                                 2000.f, 4000.f, 8000.f, 16000.f };
    for (float freq : gridFreqs)
    {
        if (freq < state.viewFreqLow || freq > state.viewFreqHigh)
            continue;

        const float y = (1.0f - (freq - state.viewFreqLow) / freqRange) * partialH;

        graphics->setColour(juce::Colours::grey.withAlpha(0.25f));
        graphics->drawHorizontalLine(static_cast<int>(y), 0.0f, w);

        graphics->setColour(juce::Colours::grey.withAlpha(0.6f));
        graphics->drawText(freq < 1000.f
                               ? juce::String(static_cast<int>(freq)) + " Hz"
                               : juce::String(freq / 1000.f, 1) + " kHz",
                           4, static_cast<int>(y) - 10, 60, 12,
                           juce::Justification::left);
    }
}

// ── Private — time ruler ──────────────────────────────────────────────────────

void Juce2DRenderer::drawRuler(const RenderState& state, float rulerY)
{
    const float  w         = static_cast<float>(state.canvasWidth);
    const float  rH        = static_cast<float>(state.rulerH);
    const double timeRange = state.viewTimeEnd - state.viewTimeStart;

    // Background
    graphics->setColour(juce::Colour(0xff181818));
    graphics->fillRect(0.0f, rulerY, w, rH);

    // Top border
    graphics->setColour(juce::Colour(0xff3a3a3a));
    graphics->drawHorizontalLine(static_cast<int>(rulerY), 0.0f, w);

    if (timeRange <= 0.0 || w <= 0.0f)
        return;

    // ── Adaptive tick selection ───────────────────────────────────────────────
    // Candidates in seconds; smallest interval = Loris hop time (5 ms)
    static const double kCandidates[] = {
        0.005, 0.010, 0.025, 0.050, 0.100, 0.250, 0.500,
        1.0, 2.0, 5.0, 10.0, 30.0, 60.0, 300.0, 600.0
    };
    static const int kNumCandidates = static_cast<int>(std::size(kCandidates));

    const float kMinLabelPx = 60.0f;
    int majorIdx = kNumCandidates - 1;
    for (int i = 0; i < kNumCandidates; ++i)
    {
        const float px = static_cast<float>(kCandidates[i] / timeRange) * w;
        if (px >= kMinLabelPx) { majorIdx = i; break; }
    }

    const double majorInterval = kCandidates[majorIdx];
    // Minor ticks: use the previous candidate if it gives >= 3px spacing
    double minorInterval = majorInterval / 5.0;
    if (majorIdx > 0)
    {
        const double prev = kCandidates[majorIdx - 1];
        const float prevPx = static_cast<float>(prev / timeRange) * w;
        if (prevPx >= 3.0f)
            minorInterval = prev;
    }

    // ── Draw minor ticks ──────────────────────────────────────────────────────
    const double firstMinor = std::floor(state.viewTimeStart / minorInterval) * minorInterval;
    graphics->setColour(juce::Colour(0xff404040));
    for (double t = firstMinor; t <= state.viewTimeEnd + minorInterval; t += minorInterval)
    {
        if (t < 0.0) continue;
        const float x = timeToX(t, state, w);
        if (x < 0.0f || x > w) continue;
        graphics->drawVerticalLine(static_cast<int>(x),
                                   rulerY + rH * 0.55f, rulerY + rH);
    }

    // ── Draw major ticks and labels ───────────────────────────────────────────
    const double firstMajor = std::floor(state.viewTimeStart / majorInterval) * majorInterval;
    graphics->setFont(10.0f);

    for (double t = firstMajor; t <= state.viewTimeEnd + majorInterval; t += majorInterval)
    {
        if (t < -0.0001) continue;
        const float x = timeToX(t, state, w);
        if (x < -1.0f || x > w + 1.0f) continue;

        graphics->setColour(juce::Colour(0xff585858));
        graphics->drawVerticalLine(static_cast<int>(x), rulerY, rulerY + rH);

        const juce::String label = formatTime(t);
        graphics->setColour(juce::Colour(0xffb0b0b0));
        graphics->drawText(label,
                           static_cast<int>(x) + 2,
                           static_cast<int>(rulerY) + 2,
                           55, static_cast<int>(rH) - 4,
                           juce::Justification::centredLeft);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

float Juce2DRenderer::ampToNorm(float amp) noexcept
{
    const float db = 20.0f * std::log10(std::max(amp, 1e-4f));
    return juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f);
}

juce::String Juce2DRenderer::formatTime(double seconds)
{
    if (seconds < 0.0001)
        return "0";

    if (seconds < 1.0)
    {
        const int ms = static_cast<int>(std::round(seconds * 1000.0));
        return juce::String(ms) + "ms";
    }

    if (seconds < 60.0)
    {
        // Show decimal only when needed
        if (std::fmod(seconds, 1.0) < 0.005)
            return juce::String(static_cast<int>(std::round(seconds))) + "s";
        return juce::String(seconds, 1) + "s";
    }

    const int    mins = static_cast<int>(seconds) / 60;
    const double secs = seconds - mins * 60.0;
    if (secs < 0.05)
        return juce::String(mins) + "m";
    return juce::String(mins) + ":" + juce::String::formatted("%02d", static_cast<int>(secs));
}