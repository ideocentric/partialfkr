// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <functional>

// ── Transport icon button ─────────────────────────────────────────────────────

/**
 * Small vector-drawn button for transport controls.
 * Style can be changed at runtime so Play ↔ Pause can share one button.
 */
class TransportButton : public juce::Button {
public:
    enum class Style { Play, Pause, Stop, Loop };

    explicit TransportButton(Style s)
        : juce::Button({}), style(s)
    {
        if (s == Style::Loop)
            setClickingTogglesState(true);
    }

    void setStyle(Style s) noexcept { style = s; repaint(); }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto  b   = getLocalBounds().toFloat().reduced(2.0f);
        const float cx  = b.getCentreX();
        const float cy  = b.getCentreY();
        const float dim = std::min(b.getWidth(), b.getHeight());

        // Background
        g.setColour(down       ? juce::Colour(0xff484848)
                   : highlighted ? juce::Colour(0xff2e2e2e)
                                 : juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(b, 3.0f);

        // Icon colour — loop lights up green when toggled on
        const bool loopOn = (style == Style::Loop && getToggleState());
        if (!isEnabled())
            g.setColour(juce::Colour(0xff484848));
        else if (loopOn)
            g.setColour(juce::Colour(0xff44cc44));
        else
            g.setColour(highlighted || down ? juce::Colours::white
                                            : juce::Colour(0xffcccccc));

        switch (style)
        {
            case Style::Play:  drawPlay (g, cx, cy, dim); break;
            case Style::Pause: drawPause(g, cx, cy, dim); break;
            case Style::Stop:  drawStop (g, cx, cy, dim); break;
            case Style::Loop:  drawLoop (g, cx, cy, dim); break;
        }
    }

private:
    Style style;

    static void drawPlay(juce::Graphics& g, float cx, float cy, float dim)
    {
        const float r = dim * 0.30f;
        juce::Path tri;
        tri.addTriangle(cx - r * 0.55f, cy - r,
                        cx - r * 0.55f, cy + r,
                        cx + r,         cy);
        g.fillPath(tri);
    }

    static void drawPause(juce::Graphics& g, float cx, float cy, float dim)
    {
        const float bW  = dim * 0.11f;
        const float bH  = dim * 0.44f;
        const float gap = dim * 0.11f;
        g.fillRect(cx - gap * 0.5f - bW, cy - bH * 0.5f, bW, bH);
        g.fillRect(cx + gap * 0.5f,       cy - bH * 0.5f, bW, bH);
    }

    static void drawStop(juce::Graphics& g, float cx, float cy, float dim)
    {
        const float r = dim * 0.27f;
        g.fillRect(cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    static void drawLoop(juce::Graphics& g, float cx, float cy, float dim)
    {
        const float r  = dim * 0.26f;
        const float sw = dim * 0.09f;
        using M = juce::MathConstants<float>;

        // Open arc: from ~30° to ~300° (270° sweep, gap at lower-right for arrowhead)
        const float startA = M::pi * (1.0f / 6.0f);   // 30°
        const float endA   = M::pi * (10.0f / 6.0f);  // 300°

        juce::Path arc;
        arc.addArc(cx - r, cy - r, r * 2.0f, r * 2.0f, startA, endA, true);
        g.strokePath(arc, juce::PathStrokeType(sw, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

        // Arrowhead at end of arc (pointing in direction of travel)
        const float ax = cx + r * std::cos(endA);
        const float ay = cy + r * std::sin(endA);
        const float tx = -std::sin(endA);   // tangent
        const float ty =  std::cos(endA);
        const float nx =  std::cos(endA);   // outward normal
        const float ny =  std::sin(endA);
        const float as = sw * 2.4f;

        juce::Path arrowHead;
        arrowHead.addTriangle(ax + tx * as * 1.4f,                 ay + ty * as * 1.4f,
                              ax - tx * as * 0.6f + nx * as,       ay - ty * as * 0.6f + ny * as,
                              ax - tx * as * 0.6f - nx * as,       ay - ty * as * 0.6f - ny * as);
        g.fillPath(arrowHead);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportButton)
};

// ── TransportBar ──────────────────────────────────────────────────────────────

/**
 * Transport controls: stop / play-pause toggle / loop, position slider, time display.
 *
 * State machine:  Stopped ↔ Playing ↔ Paused
 *
 * MainComponent wires the callbacks and calls setState() to drive the display.
 * The timer polls getPosition() and fires onPlayComplete when the play range end
 * is reached.
 */
class TransportBar : public juce::Component,
                     private juce::Timer {
public:
    enum class State { Stopped, Playing, Paused };

    TransportBar();

    void resized() override;

    // ── State control (called by MainComponent) ───────────────────────────────
    void setState(State s);
    void setDuration(double seconds);
    void setPlayheadPosition(double seconds);

    /** Constrain playback to [startSeconds, endSeconds]. */
    void setPlayRange(double startSeconds, double endSeconds);
    void clearPlayRange();

    [[nodiscard]] State  getState()            const noexcept { return state; }
    [[nodiscard]] bool   isLoopEnabled()       const noexcept { return loopButton.getToggleState(); }
    void setLoopEnabled(bool on) { loopButton.setToggleState(on, juce::sendNotification); }
    [[nodiscard]] double getPlayheadPosition() const noexcept { return playheadPos; }
    [[nodiscard]] double getPlayRangeStart()   const noexcept { return playRangeStart; }
    [[nodiscard]] bool   hasPlayRange()        const noexcept { return playEnd > 0.0; }

    // ── Callbacks → wired by MainComponent ───────────────────────────────────
    std::function<void()>       onPlayPauseToggle;
    std::function<void()>       onStop;
    std::function<void()>       onPlayComplete;      ///< fired when playEnd is reached
    std::function<void(double)> onPositionChanged;   ///< fired each timer tick; use to drive PartialView
    std::function<double()>     getPosition;

private:
    void timerCallback() override;
    void updateTimeDisplay();

    TransportButton stopButton      { TransportButton::Style::Stop };
    TransportButton playPauseButton { TransportButton::Style::Play };
    TransportButton loopButton      { TransportButton::Style::Loop };
    juce::Label     timeLabel;

    State  state          = State::Stopped;
    double duration       = 0.0;
    double playheadPos    = 0.0;
    double playEnd        = -1.0;   // -1 = no limit
    double playRangeStart = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};