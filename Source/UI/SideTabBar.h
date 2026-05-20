// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <JuceHeader.h>

/**
 * Tab bar + panel frame for the right-side panel.
 *
 * Draws a classic "manila folder" tab chrome: the active tab has rounded top
 * corners and an open bottom that merges seamlessly into the surrounding panel
 * rectangle. The inactive tab is slightly recessed.
 *
 * This component handles only chrome and click detection. Content panels are
 * sized by the parent (MainComponent) using getContentBounds().
 */
class SideTabBar : public juce::Component
{
public:
    static constexpr int kTabH   = 28;  ///< tab bar height in pixels
    static constexpr int kInsetH =  8;  ///< horizontal padding inside panel border (left/right)
    static constexpr int kInsetV =  4;  ///< vertical padding inside panel border (top/bottom)

    SideTabBar();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    juce::MouseCursor getMouseCursor() override;

    /**
     * Returns the inner bounds (in this component's local coordinates) where
     * content panels should be positioned.
     */
    [[nodiscard]] juce::Rectangle<int> getContentBounds() const noexcept;

    /** Height this component should be given contentH pixels of panel content. */
    static int preferredHeight(int contentH) noexcept
    {
        return kTabH + contentH + kInsetV * 2;
    }

    bool isShowingTools() const noexcept { return showTools; }

    /** Called with true = Tools tab, false = Filters tab. */
    std::function<void(bool)> onTabChanged;

private:
    bool showTools = true;
    int  hoverTab  = -1;   // -1 = none, 0 = Tools, 1 = Filters

    static constexpr float kCornerR = 5.0f;

    void drawActiveTabBorder  (juce::Graphics& g, float tabX, float tabW) const;
    int  tabIndexAtX          (int x) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideTabBar)
};