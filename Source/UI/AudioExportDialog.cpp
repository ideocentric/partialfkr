// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioExportDialog.h"

// ── Layout constants ──────────────────────────────────────────────────────────

static constexpr int kW          = 400;
static constexpr int kTabH       = 28;
static constexpr int kPad        = 12;
static constexpr int kSide       = 14;
static constexpr int kLabelH     = 14;
static constexpr int kCtrlH      = 24;
static constexpr int kSliderH    = 26;
static constexpr int kRowGap     = 6;
static constexpr int kPanelH     = 84;
static constexpr float kCornerR  = 5.0f;

// Derived
static constexpr int kOptionY    = kTabH + kPad;                 // 40
static constexpr int kSrRowY     = kOptionY + kPanelH + kPad;   // 136
static constexpr int kContentBtm = kSrRowY + kCtrlH + kPad;     // 172
static constexpr int kBtnY       = kContentBtm + 8;             // 180
static constexpr int kH          = kBtnY + kCtrlH + 12;         // 216
static constexpr int kContentH   = kContentBtm - kTabH;         // 144

// ── Colours (matches SideTabBar) ─────────────────────────────────────────────

static const juce::Colour kBg         { 0xff111111 };
static const juce::Colour kContentBg  { 0xff1a1a1a };
static const juce::Colour kInactiveBg { 0xff111111 };
static const juce::Colour kHoverBg    { 0xff1f1f1f };
static const juce::Colour kBorderCol  { 0xff3a3a3a };
static const juce::Colour kActiveText { 0xffffffff };
static const juce::Colour kInactiveText{ 0xff888888 };
static const juce::Colour kLabelCol   { 0xffaaaaaa };

// ── Per-format option panels ──────────────────────────────────────────────────

struct AudioExportDialog::PcmPanel : public juce::Component {
    juce::Label    depthLabel;
    juce::ComboBox depthBox;

    explicit PcmPanel(bool isFloat32Default = false)
    {
        depthLabel.setText("Bit Depth:", juce::dontSendNotification);
        depthLabel.setFont(juce::Font(12.5f));
        depthLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(depthLabel);

        depthBox.addItem("16-bit integer", 1);
        depthBox.addItem("24-bit integer", 2);
        depthBox.addItem("32-bit float",   3);
        depthBox.setSelectedId(isFloat32Default ? 3 : 2, juce::dontSendNotification);
        addAndMakeVisible(depthBox);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        depthLabel.setBounds(b.removeFromTop(kLabelH));
        b.removeFromTop(4);
        depthBox.setBounds(b.removeFromTop(kCtrlH).withWidth(176));
    }

    int bitDepth() const noexcept
    {
        switch (depthBox.getSelectedId())
        {
            case 1: return 16;
            case 3: return 32;
            default: return 24;
        }
    }
};

struct AudioExportDialog::FlacPanel : public juce::Component {
    juce::Label    depthLabel, comprLabel;
    juce::ComboBox depthBox;
    juce::Slider   comprSlider;

    FlacPanel()
    {
        depthLabel.setText("Bit Depth:", juce::dontSendNotification);
        depthLabel.setFont(juce::Font(12.5f));
        depthLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(depthLabel);

        depthBox.addItem("16-bit", 1);
        depthBox.addItem("24-bit", 2);
        depthBox.setSelectedId(2, juce::dontSendNotification);
        addAndMakeVisible(depthBox);

        comprLabel.setText("Compression:", juce::dontSendNotification);
        comprLabel.setFont(juce::Font(12.5f));
        comprLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(comprLabel);

        comprSlider.setRange(1.0, 8.0, 1.0);
        comprSlider.setValue(5.0, juce::dontSendNotification);
        comprSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        comprSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 28, kCtrlH);
        addAndMakeVisible(comprSlider);
    }

    void resized() override
    {
        const int rowH = kLabelH + 4 + kCtrlH;
        auto b = getLocalBounds();

        auto row1 = b.removeFromTop(rowH);
        depthLabel.setBounds(row1.removeFromTop(kLabelH));
        row1.removeFromTop(4);
        depthBox.setBounds(row1.removeFromTop(kCtrlH).withWidth(140));

        b.removeFromTop(kRowGap);

        auto row2 = b.removeFromTop(rowH);
        comprLabel.setBounds(row2.removeFromTop(kLabelH));
        row2.removeFromTop(4);
        comprSlider.setBounds(row2.removeFromTop(kCtrlH));
    }

    int bitDepth()    const noexcept { return depthBox.getSelectedId() == 1 ? 16 : 24; }
    int compression() const noexcept { return (int)comprSlider.getValue(); }
};

struct AudioExportDialog::OggPanel : public juce::Component {
    juce::Label  qualLabel, lowLabel, highLabel;
    juce::Slider qualSlider;

    OggPanel()
    {
        qualLabel.setText("Quality:", juce::dontSendNotification);
        qualLabel.setFont(juce::Font(12.5f));
        qualLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(qualLabel);

        qualSlider.setRange(0.0, 1.0);
        qualSlider.setValue(0.5, juce::dontSendNotification);
        qualSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        qualSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(qualSlider);

        lowLabel.setText("Low", juce::dontSendNotification);
        lowLabel.setFont(juce::Font(11.0f));
        lowLabel.setColour(juce::Label::textColourId, kInactiveText);
        addAndMakeVisible(lowLabel);

        highLabel.setText("High", juce::dontSendNotification);
        highLabel.setFont(juce::Font(11.0f));
        highLabel.setColour(juce::Label::textColourId, kInactiveText);
        highLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(highLabel);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        qualLabel.setBounds(b.removeFromTop(kLabelH));
        b.removeFromTop(4);
        qualSlider.setBounds(b.removeFromTop(kSliderH));
        auto endRow = b.removeFromTop(kLabelH);
        lowLabel.setBounds(endRow.removeFromLeft(40));
        highLabel.setBounds(endRow.removeFromRight(40));
    }

    float quality() const noexcept { return (float)qualSlider.getValue(); }
};

#if JUCE_MAC
struct AudioExportDialog::AacAlacPanel : public juce::Component {
    juce::TextButton aacButton  { "AAC" };
    juce::TextButton alacButton { "ALAC" };
    juce::Label      bitrateLabel, depthLabel;
    juce::ComboBox   bitrateBox, alacDepthBox;
    bool             useAlacMode = false;

    AacAlacPanel()
    {
        aacButton.setClickingTogglesState(true);
        alacButton.setClickingTogglesState(true);
        aacButton.setRadioGroupId(42);
        alacButton.setRadioGroupId(42);
        aacButton.setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(aacButton);
        addAndMakeVisible(alacButton);

        bitrateLabel.setText("Bitrate:", juce::dontSendNotification);
        bitrateLabel.setFont(juce::Font(12.5f));
        bitrateLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(bitrateLabel);

        bitrateBox.addItem("128 kbps", 1);
        bitrateBox.addItem("192 kbps", 2);
        bitrateBox.addItem("256 kbps", 3);
        bitrateBox.addItem("320 kbps", 4);
        bitrateBox.setSelectedId(3, juce::dontSendNotification);
        addAndMakeVisible(bitrateBox);

        depthLabel.setText("Bit Depth:", juce::dontSendNotification);
        depthLabel.setFont(juce::Font(12.5f));
        depthLabel.setColour(juce::Label::textColourId, kLabelCol);
        addAndMakeVisible(depthLabel);

        alacDepthBox.addItem("16-bit", 1);
        alacDepthBox.addItem("24-bit", 2);
        alacDepthBox.setSelectedId(2, juce::dontSendNotification);
        addAndMakeVisible(alacDepthBox);

        aacButton.onClick  = [this] { setAlacMode(false); };
        alacButton.onClick = [this] { setAlacMode(true);  };
        setAlacMode(false);
    }

    void setAlacMode(bool alac)
    {
        useAlacMode = alac;
        aacButton.setToggleState(!alac, juce::dontSendNotification);
        alacButton.setToggleState(alac,  juce::dontSendNotification);
        bitrateLabel.setVisible(!alac);
        bitrateBox.setVisible(!alac);
        depthLabel.setVisible(alac);
        alacDepthBox.setVisible(alac);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto toggleRow = b.removeFromTop(kCtrlH);
        const int btnW = (toggleRow.getWidth() - 4) / 2;
        aacButton.setBounds(toggleRow.removeFromLeft(btnW));
        toggleRow.removeFromLeft(4);
        alacButton.setBounds(toggleRow);

        b.removeFromTop(kRowGap + 2);

        bitrateLabel.setBounds(b.removeFromTop(kLabelH));
        b.removeFromTop(4);
        bitrateBox.setBounds(b.removeFromTop(kCtrlH).withWidth(160));

        depthLabel.setBounds({ bitrateLabel.getX(), bitrateLabel.getY(),
                               bitrateLabel.getWidth(), bitrateLabel.getHeight() });
        alacDepthBox.setBounds({ bitrateBox.getX(), bitrateBox.getY(),
                                 bitrateBox.getWidth(), bitrateBox.getHeight() });
    }

    bool useAlac()     const noexcept { return useAlacMode; }
    int  aacBitrate()  const noexcept
    {
        const int rates[] = { 128, 192, 256, 320 };
        return rates[juce::jlimit(0, 3, bitrateBox.getSelectedId() - 1)];
    }
    int  alacBitDepth() const noexcept { return alacDepthBox.getSelectedId() == 1 ? 16 : 24; }
};
#endif

// ── AudioExportDialog ─────────────────────────────────────────────────────────

AudioExportDialog::AudioExportDialog(double projSR)
    : projectSampleRate(projSR)
{
    setSize(kW, kH);

    // ── Option panels ─────────────────────────────────────────────────────────
    aiffPanel  = std::make_unique<PcmPanel>();
    wavPanel   = std::make_unique<PcmPanel>();
    flacPanel  = std::make_unique<FlacPanel>();
    oggPanel   = std::make_unique<OggPanel>();
#if JUCE_MAC
    aacAlacPanel = std::make_unique<AacAlacPanel>();
    addAndMakeVisible(*aacAlacPanel);
#endif
    addAndMakeVisible(*aiffPanel);
    addAndMakeVisible(*wavPanel);
    addAndMakeVisible(*flacPanel);
    addAndMakeVisible(*oggPanel);

    // ── Sample rate ───────────────────────────────────────────────────────────
    sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    sampleRateLabel.setFont(juce::Font(12.5f));
    sampleRateLabel.setColour(juce::Label::textColourId, kLabelCol);
    sampleRateLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(sampleRateLabel);

    sampleRateBox.addItem("Match source", 1);
    sampleRateBox.addItem("44100 Hz",     2);
    sampleRateBox.addItem("48000 Hz",     3);
    sampleRateBox.addItem("96000 Hz",     4);
    sampleRateBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(sampleRateBox);

    // ── Buttons ───────────────────────────────────────────────────────────────
    cancelButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff2a2a2a));
    cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaaaa));
    cancelButton.onClick = [this] {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(cancelButton);

    exportButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff505050));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton.onClick = [this] {
        if (onExport) onExport(buildOptions());
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(exportButton);

    switchTab(0);
}

AudioExportDialog::~AudioExportDialog() = default;

// ── Tab helpers ───────────────────────────────────────────────────────────────

int AudioExportDialog::numTabs() const noexcept
{
#if JUCE_MAC
    return 5;
#else
    return 4;
#endif
}

int AudioExportDialog::tabIndexAtX(int x) const noexcept
{
    const float tabW = (float)getWidth() / (float)numTabs();
    const int idx = (int)((float)x / tabW);
    return juce::jlimit(0, numTabs() - 1, idx);
}

juce::Component* AudioExportDialog::panelForTab(int idx) const noexcept
{
    switch (idx)
    {
        case 0: return aiffPanel.get();
        case 1: return wavPanel.get();
        case 2: return flacPanel.get();
        case 3: return oggPanel.get();
#if JUCE_MAC
        case 4: return aacAlacPanel.get();
#endif
        default: return nullptr;
    }
}

void AudioExportDialog::switchTab(int idx)
{
    for (int i = 0; i < numTabs(); ++i)
        if (auto* p = panelForTab(i))
            p->setVisible(i == idx);

    activeTab = idx;
    repaint();
    resized();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void AudioExportDialog::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    drawTabStrip(g);
}

void AudioExportDialog::drawTabStrip(juce::Graphics& g)
{
    const float w    = (float)getWidth();
    const int   n    = numTabs();
    const float tabW = w / (float)n;
    const float cr   = kCornerR;

    // 1. Inactive tab fills
    for (int i = 0; i < n; ++i)
    {
        if (i == activeTab) continue;
        const float tx = (float)i * tabW;
        const bool  hov = (i == hoverTab);
        juce::Path p;
        p.addRoundedRectangle(tx + 1.0f, 3.0f, tabW - 2.0f, (float)kTabH - 2.0f,
                              cr, cr, true, true, false, false);
        g.setColour(hov ? kHoverBg : kInactiveBg);
        g.fillPath(p);
    }

    // 2. Content panel fill + border
    {
        const float panelY = (float)kTabH - 1.0f;
        juce::Rectangle<float> panel(0.5f, panelY, w - 1.0f, (float)kContentH + 1.5f);
        g.setColour(kContentBg);
        g.fillRoundedRectangle(panel, cr);
        g.setColour(kBorderCol);
        g.drawRoundedRectangle(panel, cr, 1.0f);
    }

    // 3. Active tab fill (same colour as content, covers top border)
    {
        const float tx = (float)activeTab * tabW;
        juce::Path p;
        p.addRoundedRectangle(tx + 0.5f, 0.5f, tabW - 1.0f, (float)kTabH + 2.0f,
                              cr, cr, true, true, false, false);
        g.setColour(kContentBg);
        g.fillPath(p);
    }

    // 4. Active tab border (U-shape: left, top, right — no bottom)
    drawActiveTabBorder(g, (float)activeTab * tabW, tabW);

    // 5. Tab labels
    const char* const tabNames[] = { "AIFF", "WAV", "FLAC", "Ogg", "AAC/ALAC" };
    g.setFont(juce::Font(11.5f));
    for (int i = 0; i < n; ++i)
    {
        const float tx = (float)i * tabW;
        g.setColour(i == activeTab ? kActiveText : kInactiveText);
        g.drawText(tabNames[i],
                   juce::Rectangle<float>(tx + 0.5f, 0.5f, tabW - 1.0f, (float)kTabH - 1.0f),
                   juce::Justification::centred);
    }
}

void AudioExportDialog::drawActiveTabBorder(juce::Graphics& g, float tabX, float tabW) const
{
    const float ay = 0.5f;
    const float ax = tabX + 0.5f;
    const float aw = tabW - 1.0f;
    const float ah = (float)kTabH + 2.0f;
    const float cr = kCornerR;

    juce::Path p;
    p.startNewSubPath(ax,         ay + ah);
    p.lineTo         (ax,         ay + cr);
    p.quadraticTo    (ax, ay,     ax + cr,       ay);
    p.lineTo         (ax + aw - cr,              ay);
    p.quadraticTo    (ax + aw, ay, ax + aw, ay + cr);
    p.lineTo         (ax + aw,    ay + ah);

    g.setColour(kBorderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));
}

// ── Layout ────────────────────────────────────────────────────────────────────

void AudioExportDialog::resized()
{
    const int innerW = getWidth() - kSide * 2;

    // Option panels (one visible at a time)
    const juce::Rectangle<int> optionArea { kSide, kOptionY, innerW, kPanelH };
    for (int i = 0; i < numTabs(); ++i)
        if (auto* p = panelForTab(i))
            p->setBounds(optionArea);

    // Sample rate row
    const int srLabelW = 96;
    const int srComboW = 164;
    sampleRateLabel.setBounds(kSide, kSrRowY, srLabelW, kCtrlH);
    sampleRateBox.setBounds(kSide + srLabelW + 8, kSrRowY, srComboW, kCtrlH);

    // Buttons — right-aligned
    const int cancelW = 76;
    const int exportW = 96;
    const int btnRight = getWidth() - kSide;
    exportButton.setBounds(btnRight - exportW,            kBtnY, exportW, kCtrlH);
    cancelButton.setBounds(btnRight - exportW - 8 - cancelW, kBtnY, cancelW, kCtrlH);
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void AudioExportDialog::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= kTabH) return;
    const int tab = tabIndexAtX(e.x);
    if (tab != activeTab)
        switchTab(tab);
}

void AudioExportDialog::mouseMove(const juce::MouseEvent& e)
{
    const int tab = (e.y < kTabH) ? tabIndexAtX(e.x) : -1;
    if (tab != hoverTab)
    {
        hoverTab = tab;
        repaint(0, 0, getWidth(), kTabH);
    }
}

void AudioExportDialog::mouseExit(const juce::MouseEvent&)
{
    if (hoverTab != -1) { hoverTab = -1; repaint(0, 0, getWidth(), kTabH); }
}

juce::MouseCursor AudioExportDialog::getMouseCursor()
{
    return (hoverTab != -1 && hoverTab != activeTab)
               ? juce::MouseCursor::PointingHandCursor
               : juce::MouseCursor::NormalCursor;
}

// ── Options assembly ──────────────────────────────────────────────────────────

AudioExporter::Options AudioExportDialog::buildOptions() const
{
    AudioExporter::Options opts;

    // Format
    switch (activeTab)
    {
        case 0: opts.format = AudioExporter::Format::AIFF;      break;
        case 1: opts.format = AudioExporter::Format::WAV;       break;
        case 2: opts.format = AudioExporter::Format::FLAC;      break;
        case 3: opts.format = AudioExporter::Format::OggVorbis; break;
#if JUCE_MAC
        case 4:
            opts.format = aacAlacPanel->useAlac() ? AudioExporter::Format::ALAC
                                                  : AudioExporter::Format::AAC;
            break;
#endif
        default: opts.format = AudioExporter::Format::AIFF; break;
    }

    // Sample rate
    {
        const int id = sampleRateBox.getSelectedId();
        const double rates[] = { projectSampleRate, 44100.0, 48000.0, 96000.0 };
        const double sr = rates[juce::jlimit(0, 3, id - 1)];
        opts.sampleRate = (sr > 0.0) ? sr : 44100.0;
    }

    opts.numChannels = 2;

    // Format-specific options
    switch (activeTab)
    {
        case 0: opts.bitDepth = aiffPanel->bitDepth(); break;
        case 1: opts.bitDepth = wavPanel->bitDepth();  break;
        case 2:
            opts.bitDepth        = flacPanel->bitDepth();
            opts.flacCompression = flacPanel->compression();
            break;
        case 3:
            opts.oggQuality = oggPanel->quality();
            break;
#if JUCE_MAC
        case 4:
            opts.alac = aacAlacPanel->useAlac();
            if (opts.alac)
                opts.bitDepth       = aacAlacPanel->alacBitDepth();
            else
                opts.aacBitrateKbps = aacAlacPanel->aacBitrate();
            break;
#endif
        default: break;
    }

    return opts;
}