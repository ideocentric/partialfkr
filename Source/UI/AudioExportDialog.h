// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Export/AudioExporter.h"

#include <JuceHeader.h>

#include <functional>

/**
 * Modal dialog for choosing audio export format and options.
 *
 * Shows a horizontal tab strip (AIFF / WAV / FLAC / Ogg / AAC-ALAC on macOS)
 * with per-format option controls below.  The caller sets onExport; when the
 * user clicks Export the callback fires with the assembled Options, then the
 * dialog closes itself.  Launched via juce::DialogWindow::LaunchOptions.
 */
class AudioExportDialog : public juce::Component {
public:
    /** projectSampleRate is used to populate the "Match source" sample-rate option. */
    explicit AudioExportDialog(double projectSampleRate);
    ~AudioExportDialog() override;

    /** Fired when the user confirms export.  The caller then opens a FileChooser. */
    std::function<void(const AudioExporter::Options&)> onExport;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    juce::MouseCursor getMouseCursor() override;

private:
    // ── Tab strip ─────────────────────────────────────────────────────────────
    int activeTab = 0;
    int hoverTab  = -1;

    void drawTabStrip(juce::Graphics& g);
    void drawActiveTabBorder(juce::Graphics& g, float tabX, float tabW) const;
    int  tabIndexAtX(int x) const noexcept;
    int  numTabs() const noexcept;
    void switchTab(int idx);

    // ── Per-format option panels ───────────────────────────────────────────────
    struct PcmPanel;
    struct FlacPanel;
    struct OggPanel;
#if JUCE_MAC
    struct AacAlacPanel;
#endif

    std::unique_ptr<PcmPanel>  aiffPanel;
    std::unique_ptr<PcmPanel>  wavPanel;
    std::unique_ptr<FlacPanel> flacPanel;
    std::unique_ptr<OggPanel>  oggPanel;
#if JUCE_MAC
    std::unique_ptr<AacAlacPanel> aacAlacPanel;
#endif

    juce::Component* panelForTab(int idx) const noexcept;

    // ── Common controls ───────────────────────────────────────────────────────
    double            projectSampleRate;
    juce::Label       sampleRateLabel;
    juce::ComboBox    sampleRateBox;
    juce::TextButton  cancelButton  { "Cancel" };
    juce::TextButton  exportButton  { "Export..." };

    AudioExporter::Options buildOptions() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioExportDialog)
};