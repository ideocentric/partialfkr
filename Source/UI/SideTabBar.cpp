// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SideTabBar.h"

// ── Colours ───────────────────────────────────────────────────────────────────

static const juce::Colour kContentBg   { 0xff1a1a1a };  // panel interior + active tab
static const juce::Colour kInactiveBg  { 0xff111111 };  // recessed inactive tab
static const juce::Colour kHoverBg     { 0xff1f1f1f };  // hover tint on inactive tab
static const juce::Colour kBorderCol   { 0xff3a3a3a };  // all borders
static const juce::Colour kActiveText  { 0xffffffff };
static const juce::Colour kInactiveText{ 0xff888888 };

// ── Construction ──────────────────────────────────────────────────────────────

SideTabBar::SideTabBar()
{
    setRepaintsOnMouseActivity(false);  // we handle repaint manually
}

// ── Layout ────────────────────────────────────────────────────────────────────

juce::Rectangle<int> SideTabBar::getContentBounds() const noexcept
{
    return { kInsetH, kTabH + kInsetV,
             getWidth()  - kInsetH * 2,
             getHeight() - kTabH - kInsetV * 2 };
}

int SideTabBar::tabIndexAtX(int x) const noexcept
{
    if (x < 0 || x >= getWidth()) return -1;
    return (x < getWidth() / 2) ? 0 : 1;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SideTabBar::paint(juce::Graphics& g)
{
    const float w    = (float)getWidth();
    const float h    = (float)getHeight();
    const float tabW = w * 0.5f;
    const float cr   = kCornerR;

    // Index of active / inactive tab
    const int activeIdx   = showTools ? 0 : 1;
    const float activeX   = showTools ? 0.0f  : tabW;
    const float inactiveX = showTools ? tabW   : 0.0f;

    // ── 1. Inactive tab fill (rounded top corners, slightly recessed) ─────────
    {
        const bool hovered = (hoverTab == (showTools ? 1 : 0));
        juce::Path p;
        p.addRoundedRectangle(inactiveX + 1.0f, 3.0f, tabW - 2.0f, (float)kTabH - 2.0f,
                              cr, cr, true, true, false, false);
        g.setColour(hovered ? kHoverBg : kInactiveBg);
        g.fillPath(p);
    }

    // ── 2. Content panel fill + border (full rounded rect below tab bar) ──────
    {
        const float panelY = (float)kTabH - 1.0f;  // slight overlap so no gap
        juce::Rectangle<float> panelRect(0.5f, panelY, w - 1.0f, h - panelY - 0.5f);
        g.setColour(kContentBg);
        g.fillRoundedRectangle(panelRect, cr);
        g.setColour(kBorderCol);
        g.drawRoundedRectangle(panelRect, cr, 1.0f);
    }

    // ── 3. Active tab fill (same color as content — erases content top border) -
    {
        juce::Path p;
        // Extends 2px below kTabH to fully cover the content top border
        p.addRoundedRectangle(activeX + 0.5f, 0.5f, tabW - 1.0f, (float)kTabH + 2.0f,
                              cr, cr, true, true, false, false);
        g.setColour(kContentBg);
        g.fillPath(p);
    }

    // ── 4. Active tab border: left, top, right sides only (open bottom) ───────
    drawActiveTabBorder(g, activeX, tabW);

    // ── 5. Tab labels ─────────────────────────────────────────────────────────
    g.setFont(juce::Font(11.5f));

    // Tools label (left tab)
    g.setColour(activeIdx == 0 ? kActiveText : kInactiveText);
    g.drawText("Tools",   juce::Rectangle<float>(0.5f,         0.5f, tabW - 1.0f, (float)kTabH - 1.0f),
               juce::Justification::centred);

    // Filters label (right tab)
    g.setColour(activeIdx == 1 ? kActiveText : kInactiveText);
    g.drawText("Filters", juce::Rectangle<float>(tabW + 0.5f, 0.5f, tabW - 1.0f, (float)kTabH - 1.0f),
               juce::Justification::centred);
}

void SideTabBar::drawActiveTabBorder(juce::Graphics& g, float tabX, float tabW) const
{
    const float ay  = 0.5f;
    const float ax  = tabX + 0.5f;
    const float aw  = tabW - 1.0f;
    const float ah  = (float)kTabH + 2.0f;  // same height as active fill
    const float cr  = kCornerR;

    // Path: bottom-left → up left side → top-left corner → top → top-right corner
    //       → down right side → bottom-right  (no bottom segment drawn)
    juce::Path p;
    p.startNewSubPath(ax,        ay + ah);        // bottom-left (start, no line yet)
    p.lineTo         (ax,        ay + cr);        // left side going up
    p.quadraticTo    (ax,  ay,   ax + cr,   ay);  // top-left corner
    p.lineTo         (ax + aw - cr,    ay);       // top edge
    p.quadraticTo    (ax + aw, ay, ax + aw, ay + cr); // top-right corner
    p.lineTo         (ax + aw,  ay + ah);         // right side going down

    g.setColour(kBorderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void SideTabBar::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= kTabH) return;  // click in content area, ignore
    const int tab = tabIndexAtX(e.x);
    const bool wantsTools = (tab == 0);
    if (wantsTools != showTools)
    {
        showTools = wantsTools;
        repaint();
        if (onTabChanged) onTabChanged(showTools);
    }
}

void SideTabBar::mouseMove(const juce::MouseEvent& e)
{
    const int tab = (e.y < kTabH) ? tabIndexAtX(e.x) : -1;
    if (tab != hoverTab)
    {
        hoverTab = tab;
        repaint();
    }
}

void SideTabBar::mouseExit(const juce::MouseEvent&)
{
    if (hoverTab != -1) { hoverTab = -1; repaint(); }
}

juce::MouseCursor SideTabBar::getMouseCursor()
{
    return (hoverTab != -1 && hoverTab != (showTools ? 0 : 1))
               ? juce::MouseCursor::PointingHandCursor
               : juce::MouseCursor::NormalCursor;
}