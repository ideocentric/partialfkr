// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LevelMeter.h"

LevelMeter::LevelMeter(const std::atomic<float>& l,
                       const std::atomic<float>& r)
    : levelL(l), levelR(r)
{
    startTimerHz(30);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

// ── Timer ──────────────────────────────────────────────────────────────────────

void LevelMeter::timerCallback()
{
    const float rawL = levelL.load(std::memory_order_relaxed);
    const float rawR = levelR.load(std::memory_order_relaxed);

    dispL = std::max(rawL, dispL * kDecayPerFrame);
    dispR = std::max(rawR, dispR * kDecayPerFrame);

    auto updateHold = [](float raw, float& hold, int& ticks)
    {
        if (raw >= hold) { hold = raw; ticks = kHoldFrames; }
        else if (ticks > 0) { --ticks; }
        else { hold *= kDecayPerFrame; }
    };
    updateHold(rawL, holdL, holdTickL);
    updateHold(rawR, holdR, holdTickR);

    if (rawL > 1.0f) clipL = true;
    if (rawR > 1.0f) clipR = true;

    repaint();
}

// ── Helpers ────────────────────────────────────────────────────────────────────

float LevelMeter::toDb(float linear) noexcept
{
    return (linear > 0.0f) ? 20.0f * std::log10(linear) : kMinDb;
}

float LevelMeter::dbToY(float db, float height) noexcept
{
    const float range = kMaxDb - kMinDb;
    return juce::jlimit(0.0f, height, (kMaxDb - db) / range * height);
}

// ── Drawing ────────────────────────────────────────────────────────────────────

static void drawChannelV(juce::Graphics& g, juce::Rectangle<int> col,
                         float dispLevel, float holdLevel, bool clipped)
{
    const float h   = (float) col.getHeight();
    const int   top = col.getY();
    const int   bot = col.getBottom();
    const int   x   = col.getX();
    const int   w   = col.getWidth();

    const int yYellow = top + (int) LevelMeter::dbToY(LevelMeter::kYellowDb, h);
    const int yRed    = top + (int) LevelMeter::dbToY(LevelMeter::kRedDb,    h);
    const int yDisp   = top + (int) LevelMeter::dbToY(LevelMeter::toDb(dispLevel), h);
    const int yHold   = top + (int) LevelMeter::dbToY(LevelMeter::toDb(holdLevel), h);

    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(col);

    // Clip indicator: 4 px strip at very top
    static constexpr int kClipH = 4;
    g.setColour(clipped ? juce::Colour(0xffff2200) : juce::Colour(0xff330000));
    g.fillRect(x, top, w, kClipH);

    if (dispLevel > 0.0f)
    {
        // Green: from bottom up to yYellow (clamped by dispY)
        const int greenTop = std::max(yDisp, yYellow);
        if (bot > greenTop)
        {
            g.setColour(juce::Colour(0xff00cc44));
            g.fillRect(x, greenTop, w, bot - greenTop);
        }

        // Yellow: yYellow to yRed
        if (yDisp < yYellow)
        {
            const int yellowTop = std::max(yDisp, yRed);
            if (yYellow > yellowTop)
            {
                g.setColour(juce::Colour(0xffddcc00));
                g.fillRect(x, yellowTop, w, yYellow - yellowTop);
            }
        }

        // Red: yRed to yDisp
        if (yDisp < yRed)
        {
            g.setColour(clipped ? juce::Colour(0xffff2200) : juce::Colour(0xffcc2200));
            g.fillRect(x, yDisp, w, yRed - yDisp);
        }
    }

    // Peak-hold: horizontal line across bar
    if (holdLevel > 0.0f && yHold > top + kClipH && yHold < bot)
    {
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawHorizontalLine(yHold, (float) x, (float)(x + w));
    }
}

static void drawScale(juce::Graphics& g, juce::Rectangle<int> area,
                      int meterTop, int meterHeight)
{
    static constexpr float kMarks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -60.0f };
    const float range = LevelMeter::kMaxDb - LevelMeter::kMinDb;

    // Inset so labels at 0 and -60 dB aren't clipped by the component bounds
    static constexpr int kPad = 5;
    const float effTop = (float)(meterTop + kPad);
    const float effH   = (float)(meterHeight - 2 * kPad);

    g.setFont(juce::Font(9.0f));

    for (float db : kMarks)
    {
        const int y = (int)(effTop + (LevelMeter::kMaxDb - db) / range * effH);

        // -36 dB gets a longer, slightly brighter tick as a visual divider
        const bool isMajor = (db == -36.0f);
        g.setColour(isMajor ? juce::Colour(0xff777777) : juce::Colour(0xff555555));
        g.drawHorizontalLine(y, (float) area.getX(), (float)(area.getX() + (isMajor ? 7 : 4)));

        const juce::String label = (db >= 0.0f) ? "0" : juce::String((int) db);
        g.setColour(juce::Colour(0xff888888));
        g.drawText(label, area.getX() + 5, y - 5, area.getWidth() - 5, 10,
                   juce::Justification::left, false);
    }
}

void LevelMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    auto area = getLocalBounds().reduced(1);

    // dB scale strip on the right
    const int scaleW = 28;
    const auto scaleArea = area.removeFromRight(scaleW);
    area.removeFromRight(3);  // gap between scale and bars

    // Fixed-width bars, right-aligned against the scale
    auto colR = area.removeFromRight(kBarW);
    area.removeFromRight(2);
    auto colL = area.removeFromRight(kBarW);

    drawChannelV(g, colL, dispL, holdL, clipL);
    drawChannelV(g, colR, dispR, holdR, clipR);
    drawScale(g, scaleArea, colL.getY(), colL.getHeight());
}

void LevelMeter::mouseDown(const juce::MouseEvent&)
{
    clipL = clipR = false;
    repaint();
}