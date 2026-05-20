// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ToolsPanel.h"
#include "BinaryData.h"

static constexpr int kPad    = 6;
static constexpr int kBtnH   = 26;
static constexpr int kHdrH   = 20;
static constexpr int kBtnGap = 3;
static constexpr int kSecGap = 6;

int ToolsPanel::preferredHeight() noexcept
{
    const int editModeH = kHdrH + kBtnH + kSecGap;
    const int markersH  = kHdrH + kBtnH + kSecGap;
    const int opsH      = kHdrH
                        + kBtnH + kBtnGap   // Bridge / Xfade
                        + kBtnH + kBtnGap   // Stretch / Scale
                        + kBtnH + kBtnGap   // Normalize
                        + kBtnH;            // Fade In / Fade Out
    return kPad * 2 + editModeH + markersH + opsH;
}

ToolsPanel::ToolsPanel()
{
    // ── Edit Mode ─────────────────────────────────────────────────────────────
    addAndMakeVisible(selectBtn);  selectBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(directBtn);  directBtn.setWantsKeyboardFocus(false);
    selectBtn.onClick = [this] { setToolMode(false); if (onToolModeChanged) onToolModeChanged(false); };
    directBtn.onClick = [this] { setToolMode(true);  if (onToolModeChanged) onToolModeChanged(true); };
    selectBtn.setTooltip("Select whole partials  (V)");
    directBtn.setTooltip("Select individual breakpoints  (A)");

    // ── Markers ───────────────────────────────────────────────────────────────
    addAndMakeVisible(setInBtn);    setInBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(setOutBtn);   setOutBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(setInOutBtn); setInOutBtn.setWantsKeyboardFocus(false);
    setInBtn   .onClick = [this] { if (onSetIn)           onSetIn(); };
    setOutBtn  .onClick = [this] { if (onSetOut)          onSetOut(); };
    setInOutBtn.onClick = [this] { if (onSetInOutFromSel) onSetInOutFromSel(); };
    setInBtn   .setTooltip("Set in point to playhead  (⌥I)");
    setOutBtn  .setTooltip("Set out point to playhead  (⌥O)");
    setInOutBtn.setTooltip("Set in/out from selection bounds  (⌥S)");

    // ── Operations — DrawableButton icons ─────────────────────────────────────
    addAndMakeVisible(bridgeBtn);    bridgeBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(crossfadeBtn); crossfadeBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(stretchBtn);   stretchBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(scaleBtn);     scaleBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(normalizeBtn); normalizeBtn.setWantsKeyboardFocus(false);

    bridgeBtn   .onClick = [this] { if (onBridge)    onBridge(); };
    crossfadeBtn.onClick = [this] { if (onCrossfade) onCrossfade(); };
    stretchBtn  .onClick = [this] { if (onStretch)   onStretch(); };
    scaleBtn    .onClick = [this] { if (onScale)     onScale(); };
    normalizeBtn.onClick = [this] { if (onNormalize) onNormalize(); };

    bridgeBtn   .setTooltip("Bridge partials across a gap  (⌘J)");
    crossfadeBtn.setTooltip("Crossfade overlapping partials  (⌘⇧J)");
    stretchBtn  .setTooltip("Stretch or compress in time  (T)");
    scaleBtn    .setTooltip("Scale amplitude by dB offset");
    normalizeBtn.setTooltip("Normalize peak to 0 dBFS  (⌥N)");

    // ── Operations — fade buttons ─────────────────────────────────────────────
    addAndMakeVisible(fadeInBtn);  fadeInBtn.setWantsKeyboardFocus(false);
    addAndMakeVisible(fadeOutBtn); fadeOutBtn.setWantsKeyboardFocus(false);
    fadeInBtn .onClick = [this] { if (onFadeIn)  onFadeIn(); };
    fadeOutBtn.onClick = [this] { if (onFadeOut) onFadeOut(); };
    fadeInBtn .setTooltip("Fade in over in/out range  (⌥/)");
    fadeOutBtn.setTooltip("Fade out over in/out range  (⌥\\)");

    // ── Load SVG icons into DrawableButtons ───────────────────────────────────
    auto loadSvg = [](const void* data, int size) {
        return juce::Drawable::createFromImageData(data, (size_t)size);
    };

    if (auto d = loadSvg(BinaryData::select_svg,    BinaryData::select_svgSize))    selectBtn   .setImages(d.get());
    if (auto d = loadSvg(BinaryData::direct_svg,    BinaryData::direct_svgSize))    directBtn   .setImages(d.get());
    if (auto d = loadSvg(BinaryData::in_svg,        BinaryData::in_svgSize))        setInBtn    .setImages(d.get());
    if (auto d = loadSvg(BinaryData::out_svg,       BinaryData::out_svgSize))       setOutBtn   .setImages(d.get());
    if (auto d = loadSvg(BinaryData::sel_svg,       BinaryData::sel_svgSize))       setInOutBtn .setImages(d.get());
    if (auto d = loadSvg(BinaryData::bridge_svg,    BinaryData::bridge_svgSize))    bridgeBtn   .setImages(d.get());
    if (auto d = loadSvg(BinaryData::xfade_svg,     BinaryData::xfade_svgSize))     crossfadeBtn.setImages(d.get());
    if (auto d = loadSvg(BinaryData::stretch_svg,   BinaryData::stretch_svgSize))   stretchBtn  .setImages(d.get());
    if (auto d = loadSvg(BinaryData::scale_svg,     BinaryData::scale_svgSize))     scaleBtn    .setImages(d.get());
    if (auto d = loadSvg(BinaryData::normalize_svg, BinaryData::normalize_svgSize)) normalizeBtn.setImages(d.get());
    if (auto d = loadSvg(BinaryData::fadein_svg,    BinaryData::fadein_svgSize))    fadeInBtn   .setImages(d.get());
    if (auto d = loadSvg(BinaryData::fadeout_svg,   BinaryData::fadeout_svgSize))   fadeOutBtn  .setImages(d.get());

    updateModeButtons();
}

// ── Public ────────────────────────────────────────────────────────────────────

void ToolsPanel::setToolMode(bool isDirect)
{
    isDirectMode = isDirect;
    updateModeButtons();
}

// ── Private ───────────────────────────────────────────────────────────────────

void ToolsPanel::updateModeButtons()
{
    const auto active   = juce::Colour(0xff606060);
    const auto inactive = juce::Colour(0xff1a1a1a);

    selectBtn.setColour(juce::TextButton::buttonColourId,  !isDirectMode ? active : inactive);
    selectBtn.setColour(juce::TextButton::textColourOffId, !isDirectMode ? juce::Colours::white : juce::Colours::grey);
    directBtn.setColour(juce::TextButton::buttonColourId,   isDirectMode ? active : inactive);
    directBtn.setColour(juce::TextButton::textColourOffId,  isDirectMode ? juce::Colours::white : juce::Colours::grey);
}

void ToolsPanel::drawSectionHeader(juce::Graphics& g, const juce::String& label, int y) const
{
    const int lineY  = y + kHdrH / 2;
    const int labelX = kPad + 4;
    const int labelH = 12;
    const int labelW = 90;

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawHorizontalLine(lineY, (float)kPad, (float)(getWidth() - kPad));

    // Background cutout so the label appears to break the line
    g.setColour(juce::Colour(0xff121212));
    g.fillRect(labelX - 1, lineY - labelH / 2 - 1, labelW + 4, labelH + 2);

    g.setFont(juce::Font(10.0f));
    g.setColour(juce::Colour(0xff888888));
    g.drawText(label, labelX, lineY - labelH / 2, labelW, labelH,
               juce::Justification::centredLeft);
}

void ToolsPanel::paint(juce::Graphics& g)
{
    drawSectionHeader(g, "Edit Mode",  yEditModeHeader);
    drawSectionHeader(g, "Markers",    yMarkersHeader);
    drawSectionHeader(g, "Operations", yOpsHeader);
}

void ToolsPanel::resized()
{
    const int w = getWidth();
    int y = kPad;

    auto placeTwoCol = [&](juce::Component& a, juce::Component& b) {
        int hw = (w - kPad * 2 - kBtnGap) / 2;
        a.setBounds(kPad, y, hw, kBtnH);
        b.setBounds(kPad + hw + kBtnGap, y, w - kPad * 2 - hw - kBtnGap, kBtnH);
        y += kBtnH;
    };

    auto placeThreeCol = [&](juce::Component& a, juce::Component& b, juce::Component& c) {
        int tw = (w - kPad * 2 - kBtnGap * 2) / 3;
        a.setBounds(kPad, y, tw, kBtnH);
        b.setBounds(kPad + tw + kBtnGap, y, tw, kBtnH);
        c.setBounds(kPad + (tw + kBtnGap) * 2, y, w - kPad * 2 - (tw + kBtnGap) * 2, kBtnH);
        y += kBtnH;
    };

    auto placeFullWidth = [&](juce::Component& btn) {
        btn.setBounds(kPad, y, w - kPad * 2, kBtnH);
        y += kBtnH;
    };

    // Edit Mode
    yEditModeHeader = y;
    y += kHdrH;
    placeTwoCol(selectBtn, directBtn);
    y += kSecGap;

    // Markers
    yMarkersHeader = y;
    y += kHdrH;
    placeThreeCol(setInBtn, setOutBtn, setInOutBtn);
    y += kSecGap;

    // Operations
    yOpsHeader = y;
    y += kHdrH;
    placeTwoCol(bridgeBtn, crossfadeBtn);
    y += kBtnGap;
    placeTwoCol(stretchBtn, scaleBtn);
    y += kBtnGap;
    placeFullWidth(normalizeBtn);
    y += kBtnGap;
    placeTwoCol(fadeInBtn, fadeOutBtn);
}