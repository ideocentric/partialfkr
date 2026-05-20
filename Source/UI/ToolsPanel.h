// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * Tool panel — edit-mode toggles, marker shortcuts, and one-shot edit operations
 * grouped by section with painted separator lines.
 *
 * All behaviour is driven via std::function callbacks wired by MainComponent,
 * following the same pattern as PartialView and TransportBar.
 */
class ToolsPanel : public juce::Component
{
public:
    ToolsPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setToolMode(bool isDirect);

    /** Returns the fixed height this panel requires. */
    static int preferredHeight() noexcept;

    // ── Callbacks wired by MainComponent ─────────────────────────────────────

    /** Fired when the user clicks a mode button. true = DirectSelect. */
    std::function<void(bool)> onToolModeChanged;

    // Marker operations
    std::function<void()> onSetIn;
    std::function<void()> onSetOut;
    std::function<void()> onSetInOutFromSel;

    // Edit operations
    std::function<void()> onBridge;
    std::function<void()> onCrossfade;
    std::function<void()> onStretch;
    std::function<void()> onScale;
    std::function<void()> onNormalize;
    std::function<void()> onFadeIn;
    std::function<void()> onFadeOut;

private:
    bool isDirectMode = false;

    // ── Edit Mode ─────────────────────────────────────────────────────────────
    juce::DrawableButton selectBtn { "select", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton directBtn { "direct", juce::DrawableButton::ImageOnButtonBackground };

    // ── Markers ───────────────────────────────────────────────────────────────
    juce::DrawableButton setInBtn    { "in",  juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton setOutBtn   { "out", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton setInOutBtn { "sel", juce::DrawableButton::ImageOnButtonBackground };

    // ── Operations ────────────────────────────────────────────────────────────
    juce::DrawableButton bridgeBtn    { "bridge",    juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton crossfadeBtn { "xfade",     juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton stretchBtn   { "stretch",   juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton scaleBtn     { "scale",     juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton normalizeBtn { "normalize", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton fadeInBtn  { "fadein",  juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton fadeOutBtn { "fadeout", juce::DrawableButton::ImageOnButtonBackground };

    // Y positions of section headers, set in resized() for use in paint()
    int yEditModeHeader = 0;
    int yMarkersHeader  = 0;
    int yOpsHeader      = 0;

    void updateModeButtons();
    void drawSectionHeader(juce::Graphics& g, const juce::String& label, int y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolsPanel)
};