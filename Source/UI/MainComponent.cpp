// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "CommandIDs.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// Shared clipboard — static so copies in one window are visible to all windows.
std::vector<MainComponent::PartialClip> MainComponent::clipboard;

// ── Export helper ─────────────────────────────────────────────────────────────

/** Copies non-muted partials into a new vector suitable for the export APIs. */
static std::vector<std::unique_ptr<Partial>>
filterMuted(const std::vector<std::unique_ptr<Partial>>& src)
{
    std::vector<std::unique_ptr<Partial>> out;
    out.reserve(src.size());
    for (const auto& p : src)
    {
        if (p->muted.load(std::memory_order_relaxed))
            continue;
        auto copy = std::make_unique<Partial>(p->getId(), p->getColour());
        copy->breakpoints = p->breakpoints;
        out.push_back(std::move(copy));
    }
    return out;
}

// ── Undo action helpers ───────────────────────────────────────────────────────

struct PartialSnapshot {
    uint32_t                 id;
    juce::Colour             colour;
    std::vector<Breakpoint>  breakpoints;
};

static PartialSnapshot snapshotOf(const Partial& p)
{
    return { p.getId(), p.getColour(), p.breakpoints };
}

static std::unique_ptr<Partial> restoreFrom(const PartialSnapshot& s)
{
    auto p = std::make_unique<Partial>(s.id, s.colour);
    p->breakpoints = s.breakpoints;
    return p;
}

/** Undoable deletion of a set of partials. */
struct DeletePartialsAction : public juce::UndoableAction {
    Project&                      project;
    std::vector<uint32_t>         ids;
    std::vector<PartialSnapshot>  snapshots;

    DeletePartialsAction(Project& p, const std::vector<uint32_t>& idsToDelete)
        : project(p), ids(idsToDelete)
    {
        for (auto id : ids)
            if (const auto* partial = p.findPartialById(id))
                snapshots.push_back(snapshotOf(*partial));
    }

    bool perform() override
    {
        project.getSelection().clear();
        project.deletePartials(ids);
        return true;
    }

    bool undo() override
    {
        std::vector<std::unique_ptr<Partial>> restored;
        restored.reserve(snapshots.size());
        for (const auto& s : snapshots)
            restored.push_back(restoreFrom(s));
        project.addPartials(std::move(restored));
        return true;
    }

    int getSizeInUnits() override
    {
        int n = 0;
        for (const auto& s : snapshots)
            n += static_cast<int>(s.breakpoints.size()) * static_cast<int>(sizeof(Breakpoint));
        return std::max(1, n);
    }
};

/**
 * Undoable time stretch/compress. Stores both the original and stretched partial
 * snapshots so perform/undo are symmetric.
 */
struct StretchAction : public juce::UndoableAction {
    Project&                     project;
    std::vector<PartialSnapshot> originalSnapshots;
    std::vector<PartialSnapshot> stretchedSnapshots;

    StretchAction(Project& p,
                  std::vector<PartialSnapshot> originals,
                  std::vector<PartialSnapshot> stretched)
        : project(p),
          originalSnapshots(std::move(originals)),
          stretchedSnapshots(std::move(stretched))
    {}

    bool perform() override
    {
        std::vector<std::unique_ptr<Partial>> replacements;
        replacements.reserve(stretchedSnapshots.size());
        for (const auto& s : stretchedSnapshots)
            replacements.push_back(restoreFrom(s));
        project.replacePartials(std::move(replacements));
        return true;
    }

    bool undo() override
    {
        std::vector<std::unique_ptr<Partial>> replacements;
        replacements.reserve(originalSnapshots.size());
        for (const auto& s : originalSnapshots)
            replacements.push_back(restoreFrom(s));
        project.replacePartials(std::move(replacements));
        return true;
    }

    int getSizeInUnits() override
    {
        int n = 0;
        for (const auto& s : originalSnapshots)
            n += static_cast<int>(s.breakpoints.size()) * static_cast<int>(sizeof(Breakpoint));
        return std::max(1, n * 2);  // original + stretched
    }
};

/**
 * Undoable breakpoint deletion: captures the original partials before the edit,
 * and the IDs of the replacement partials created by the trim/split.
 */
struct DeleteBreakpointsAction : public juce::UndoableAction {
    Project&                      project;
    std::vector<PartialSnapshot>  originalSnapshots;  // full state of affected partials
    std::vector<uint32_t>         originalIds;         // IDs to delete in perform()
    std::unordered_map<uint32_t, std::vector<size_t>> deletions;  // what to remove
    std::vector<uint32_t>         newIds;              // created by first perform(); deleted in undo()

    DeleteBreakpointsAction(Project& p,
                            std::unordered_map<uint32_t, std::vector<size_t>> del)
        : project(p), deletions(std::move(del))
    {
        for (const auto& [id, _] : deletions)
        {
            originalIds.push_back(id);
            if (const auto* partial = p.findPartialById(id))
                originalSnapshots.push_back(snapshotOf(*partial));
        }
    }

    bool perform() override
    {
        newIds = project.applyBreakpointDeletion(deletions);
        return true;
    }

    bool undo() override
    {
        // Remove the replacement partials created by perform()
        project.deletePartials(newIds);

        // Re-add the original partials
        std::vector<std::unique_ptr<Partial>> restored;
        restored.reserve(originalSnapshots.size());
        for (const auto& s : originalSnapshots)
            restored.push_back(restoreFrom(s));
        project.addPartials(std::move(restored));
        return true;
    }

    int getSizeInUnits() override
    {
        int n = 0;
        for (const auto& s : originalSnapshots)
            n += static_cast<int>(s.breakpoints.size()) * static_cast<int>(sizeof(Breakpoint));
        return std::max(1, n);
    }
};

/** Undoable addition of a set of partials (used by paste). */
struct AddPartialsAction : public juce::UndoableAction {
    Project&                      project;
    std::vector<PartialSnapshot>  snapshots;
    std::vector<uint32_t>         ids;

    AddPartialsAction(Project& p, std::vector<PartialSnapshot> snaps)
        : project(p), snapshots(std::move(snaps))
    {
        for (const auto& s : snapshots)
            ids.push_back(s.id);
    }

    bool perform() override
    {
        std::vector<std::unique_ptr<Partial>> newPartials;
        newPartials.reserve(snapshots.size());
        for (const auto& s : snapshots)
            newPartials.push_back(restoreFrom(s));
        project.addPartials(std::move(newPartials));
        return true;
    }

    bool undo() override
    {
        project.getSelection().clear();
        project.deletePartials(ids);
        return true;
    }

    int getSizeInUnits() override
    {
        int n = 0;
        for (const auto& s : snapshots)
            n += static_cast<int>(s.breakpoints.size()) * static_cast<int>(sizeof(Breakpoint));
        return std::max(1, n);
    }
};

/** Undoable join of two partials into one (shared by bridge and crossfade). */
struct JoinPartialsAction : public juce::UndoableAction {
    Project&         project;
    PartialSnapshot  snapA, snapB;   // originals — restored on undo
    PartialSnapshot  merged;         // result    — restored on redo

    JoinPartialsAction(Project& p,
                       PartialSnapshot a, PartialSnapshot b, PartialSnapshot m)
        : project(p), snapA(std::move(a)), snapB(std::move(b)), merged(std::move(m)) {}

    bool perform() override
    {
        project.deletePartials({ snapA.id, snapB.id });
        std::vector<std::unique_ptr<Partial>> v;
        v.push_back(restoreFrom(merged));
        project.addPartials(std::move(v));
        return true;
    }

    bool undo() override
    {
        project.deletePartials({ merged.id });
        std::vector<std::unique_ptr<Partial>> v;
        v.push_back(restoreFrom(snapA));
        v.push_back(restoreFrom(snapB));
        project.addPartials(std::move(v));
        return true;
    }

    int getSizeInUnits() override
    {
        const int n = static_cast<int>(
            (snapA.breakpoints.size() + snapB.breakpoints.size() + merged.breakpoints.size())
            * sizeof(Breakpoint));
        return std::max(1, n);
    }
};

// ── Join math helpers ─────────────────────────────────────────────────────────

static constexpr double kJoinInterval = 0.005;  // 5 ms — matches Loris hop time

/** Linear interpolation of all Breakpoint fields at time t within [t0, t1]. Phase is zeroed. */
static Breakpoint lerpBP(const Breakpoint& a, const Breakpoint& b,
                          double t, double t0, double t1) noexcept
{
    const float alpha = (t1 > t0) ? static_cast<float>((t - t0) / (t1 - t0)) : 0.0f;
    return { t,
             a.frequency + alpha * (b.frequency - a.frequency),
             a.amplitude + alpha * (b.amplitude - a.amplitude),
             0.0f,
             a.bandwidth + alpha * (b.bandwidth - a.bandwidth) };
}

/** Evaluate a partial's breakpoint fields at an arbitrary time via linear interp. */
static Breakpoint evaluateAt(const Partial& p, double t) noexcept
{
    const auto& bps = p.breakpoints;
    if (bps.empty())          return { t, 0.0f, 0.0f, 0.0f, 0.0f };
    if (t <= bps.front().time) { auto bp = bps.front(); bp.time = t; bp.phase = 0.0f; return bp; }
    if (t >= bps.back().time)  { auto bp = bps.back();  bp.time = t; bp.phase = 0.0f; return bp; }
    for (size_t i = 0; i + 1 < bps.size(); ++i)
        if (t >= bps[i].time && t <= bps[i + 1].time)
            return lerpBP(bps[i], bps[i + 1], t, bps[i].time, bps[i + 1].time);
    { auto bp = bps.back(); bp.time = t; bp.phase = 0.0f; return bp; }
}

MainComponent::MainComponent()
{
    project.addListener(this);
    project.getSelection().addListener(this);

    addAndMakeVisible(partialView);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(reductionPanel);
    addAndMakeVisible(inspectorPanel);
    addAndMakeVisible(levelMeter);

    partialView.setComponentID("partialView");
    transportBar.setComponentID("transportBar");
    reductionPanel.setComponentID("reductionPanel");
    inspectorPanel.setComponentID("inspectorPanel");
    levelMeter.setComponentID("levelMeter");
    gainKnob.setComponentID("gainKnob");

    gainKnob.setRange(-40.0, 6.0, 0.1);
    gainKnob.setValue(-20.0, juce::dontSendNotification);
    gainKnob.setDoubleClickReturnValue(true, -20.0);
    gainKnob.onValueChange = [this] {
        synth.setOutputGain(juce::Decibels::decibelsToGain((float) gainKnob.getValue()));
        gainLabel.setText(juce::String(gainKnob.getValue(), 1) + " dB",
                          juce::dontSendNotification);
    };
    gainLabel.setText("-20.0 dB", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setFont(juce::Font(10.0f));
    gainLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    gainLabel.setColour(juce::Label::outlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible(gainKnob);
    addAndMakeVisible(gainLabel);

    partialView.onCopy   = [this] { editCopy(); };
    partialView.onCut    = [this] { editCut(); };
    partialView.onPaste  = [this] { editPaste(); };
    partialView.onDelete = [this] { editDelete(); };
    partialView.onUndo             = [this] { editUndo(); };
    partialView.onRedo             = [this] { editRedo(); };
    partialView.onBreakpointDelete = [this] { editBreakpointDelete(); };
    partialView.onStretch          = [this] { editStretch(); };
    partialView.onToolModeChanged              = [this](bool isDirect) { reductionPanel.setActiveMode(isDirect); };
    partialView.onBreakpointSelectionChanged   = [this] { if (onMenuStateChanged) onMenuStateChanged(); };

    reductionPanel.onToolModeChanged = [this](bool isDirect) {
        partialView.setToolMode(isDirect ? PartialView::ToolMode::DirectSelect
                                         : PartialView::ToolMode::Selection);
    };

    transportBar.onPlayPauseToggle = [this] { handlePlayPauseToggle(); };
    transportBar.onStop            = [this] { handleStop(); };
    transportBar.onPlayComplete    = [this] { handlePlayComplete(); };
    transportBar.getPosition        = [this] { return synth.getPlayheadSeconds(); };
    transportBar.onPositionChanged  = [this](double t) { partialView.setPlayheadTime(t); };

    partialView.onPlayPauseToggle = [this] { handlePlayPauseToggle(); };
    partialView.onSeek = [this](double t) {
        synth.setPlayheadTime(t);
        transportBar.setPlayheadPosition(t);
    };
    partialView.onScrubStart  = [this](double t) { handleScrubStart(t); };
    partialView.onScrubUpdate = [this](double t) { handleScrubUpdate(t); };
    partialView.onScrubEnd    = [this]()          { handleScrubEnd(); };

    setWantsKeyboardFocus(true);
    setAudioChannels(0, 2);
    setSize(1200, 830);
}

MainComponent::~MainComponent()
{
    project.getSelection().removeListener(this);
    shutdownAudio();
    project.removeListener(this);
}

void MainComponent::prepareToPlay(int samplesPerBlock, double sr)
{
    synth.setProject(&project);
    synth.prepareToPlay(sr, samplesPerBlock);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    synth.renderNextBlock(*bufferToFill.buffer,
                          bufferToFill.startSample,
                          bufferToFill.numSamples);

    // Measure peak per channel with exponential decay (~1.5 s time constant at 44.1 kHz)
    const int   n   = bufferToFill.numSamples;
    const int   s   = bufferToFill.startSample;
    const auto& buf = *bufferToFill.buffer;

    auto measurePeak = [&](int channel) -> float {
        const float* data = buf.getReadPointer(juce::jmin(channel, buf.getNumChannels() - 1), s);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max(peak, std::abs(data[i]));
        return peak;
    };

    const float newL = measurePeak(0);
    const float newR = (buf.getNumChannels() > 1) ? measurePeak(1) : newL;

    // Fast attack (instant jump to new peak), slow decay (~1.5 s across a block)
    const float sr    = static_cast<float>(synth.getSampleRate());
    const float decay = (sr > 0.0f) ? std::exp(-static_cast<float>(n) / (sr * 1.5f)) : 0.95f;
    levelL.store(std::max(newL, levelL.load(std::memory_order_relaxed) * decay),
                 std::memory_order_relaxed);
    levelR.store(std::max(newR, levelR.load(std::memory_order_relaxed) * decay),
                 std::memory_order_relaxed);
}

void MainComponent::releaseResources()
{
    synth.releaseResources();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    // dB scale strip to the left of the gain fader.
    // Track extremes are inset by the thumb radius (12px for our fader width).
    // Thumb radius: JUCE computes min(12, sliderWidth/2); fader is 20px wide → 10
    const int   kThumbR  = std::min(12, gainKnob.getWidth() / 2);
    static constexpr float kFaderMin = -40.0f;
    static constexpr float kFaderMax =  6.0f;
    static constexpr float kRange    = kFaderMax - kFaderMin;
    static constexpr float kMarks[]  = { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f };

    const float trackH   = (float)(gainScaleArea.getHeight() - 2 * kThumbR);
    const float trackTop = (float)(gainScaleArea.getY() + kThumbR);
    const int   right    = gainScaleArea.getRight();

    g.setFont(juce::Font(9.0f));

    for (float db : kMarks)
    {
        const float proportion = (db - kFaderMin) / kRange;   // 0 = bottom, 1 = top
        const float y = trackTop + (1.0f - proportion) * trackH;

        g.setColour(juce::Colour(0xff555555));
        g.drawHorizontalLine((int) y, (float)(right - 4), (float) right);

        const juce::String label = (db > 0.0f) ? ("+" + juce::String((int) db))
                                               : juce::String((int) db);
        g.setColour(juce::Colour(0xff888888));
        g.drawText(label, gainScaleArea.getX(), (int) y - 5,
                   gainScaleArea.getWidth() - 6, 10,
                   juce::Justification::right);
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        handlePlayPauseToggle();
        return true;
    }

    // I — set in point to current playhead position
    if (key == juce::KeyPress('i', 0, 0))
    {
        updateMarkers(transportBar.getPlayheadPosition(), outPoint);
        return true;
    }

    // O — set out point to current playhead position
    if (key == juce::KeyPress('o', 0, 0))
    {
        updateMarkers(inPoint, transportBar.getPlayheadPosition());
        return true;
    }

    // Ctrl+S — set in/out from selected partial bounds
    if (key == juce::KeyPress('s', juce::ModifierKeys::ctrlModifier, 0))
    {
        const auto& sel = project.getSelection();
        if (!sel.isEmpty())
        {
            double earliest = std::numeric_limits<double>::max();
            double latest   = std::numeric_limits<double>::lowest();
            for (const auto& p : project.getPartials())
            {
                if (!sel.isSelected(p->getId())) continue;
                if (!p->breakpoints.empty())
                {
                    earliest = std::min(earliest, p->breakpoints.front().time);
                    latest   = std::max(latest,   p->breakpoints.back().time);
                }
            }
            if (earliest < std::numeric_limits<double>::max())
                updateMarkers(earliest, latest);
        }
        return true;
    }

    return false;
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    // Transport bar across the bottom
    transportBar.setBounds(area.removeFromBottom(48));

    // Right panel column — hidden entirely when rightPanelVisible is false
    if (rightPanelVisible)
    {
        const int panelW = 220;
        auto rightColumn = area.removeFromRight(panelW);

        // Gain fader + level meter: vertical unit at top of right column
        rightColumn.removeFromTop(8);
        const int unitH = 200;
        auto unit = rightColumn.removeFromTop(unitH);
        rightColumn.removeFromTop(8);

        // Center the fader + meter group within the panel
        static constexpr int faderW   = 46;  // scale(24) + gap(2) + fader(20)
        static constexpr int groupGap = 6;
        const int meterW   = LevelMeter::kPreferredWidth;
        const int groupW   = faderW + groupGap + meterW;
        const int leftPad  = (panelW - groupW) / 2;

        unit.removeFromLeft(leftPad);
        auto faderCol = unit.removeFromLeft(faderW);
        unit.removeFromLeft(groupGap);
        levelMeter.setBounds(unit.removeFromLeft(meterW));

        gainLabel.setBounds(faderCol.removeFromTop(18));
        gainScaleArea = faderCol.removeFromLeft(24);
        faderCol.removeFromLeft(2);
        gainKnob.setBounds(faderCol);

        reductionPanel.setBounds(rightColumn.removeFromTop(ReductionPanel::preferredHeight()));
        inspectorPanel.setBounds(rightColumn.removeFromTop(160));
        // remainder of rightColumn is blank space that grows with the window
    }
    else
    {
        gainKnob.setBounds({});
        gainLabel.setBounds({});
        gainScaleArea = {};
        levelMeter.setBounds({});
        reductionPanel.setBounds({});
        inspectorPanel.setBounds({});
    }

    // Partial canvas fills everything left
    partialView.setBounds(area);
}

void MainComponent::partialsChanged(Project&)
{
    const auto& partials = project.getPartials();
    double duration = partials.empty() ? 0.0 : partials.back()->endTime();
    transportBar.setDuration(duration);
    if (!partials.empty()) setDirtyFlag(true);  // any structural change dirtifies the project
    repaint();
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::setWindowTitle(const juce::String& title)
{
    if (auto* dw = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent()))
        dw->setName(title);
}

void MainComponent::openFileAndAnalyze()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Open audio file", juce::File{}, "*.wav;*.aif;*.aiff");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto results = fc.getResults();
                             if (results.isEmpty())
                                 return;

                             setWindowTitle("PartialFKR - Analysing...");

                             project.loadSourceAudio(results[0]);

                             analyzer.analyzeAsync(
                                 project.getSourceAudio(),
                                 project.getSampleRate(),
                                 [](float /*progress*/) {},
                                 [this](auto partials) {
                                     onAnalysisComplete(std::move(partials));
                                 });
                         });
}

void MainComponent::onAnalysisComplete(std::vector<std::unique_ptr<Partial>> partials)
{
    setWindowTitle("PartialFKR - " + project.getSourceFile().getFileName());
    MacProxyIcon::set(getTopLevelComponent(), project.getSourceFile());

    project.setOriginalPartials(partials);   // deep-copy before move
    project.setPartials(std::move(partials));
    currentFile = juce::File{};              // new analysis — not yet saved to a .pfkr
    synth.setPlaying(false);
    synth.setProject(&project);
    synth.setPlayheadTime(0.0);
    synth.setSolo(project.hasAnySolo());

    // Set in/out markers to cover the full file
    const double dur = project.getPartials().empty() ? 0.0
                     : project.getPartials().back()->endTime();
    updateMarkers(0.0, dur);

    transportBar.clearPlayRange();
    transportBar.setState(TransportBar::State::Stopped);
    transportBar.setPlayheadPosition(0.0);
    partialView.setPlayheadTime(0.0);
    setDirtyFlag(true);
    grabKeyboardFocus();
}

// ── Save / Load ───────────────────────────────────────────────────────────────

void MainComponent::setDirtyFlag(bool d)
{
    dirty = d;
    if (onMenuStateChanged) onMenuStateChanged();
}

juce::String MainComponent::getDisplayName() const
{
    if (currentFile != juce::File{})
        return currentFile.getFileNameWithoutExtension();
    if (project.getSourceFile() != juce::File{})
        return project.getSourceFile().getFileNameWithoutExtension();
    return "Untitled";
}

PfkrFormat::SaveData MainComponent::buildSaveData() const
{
    PfkrFormat::SaveData data;
    data.inPoint       = inPoint;
    data.outPoint      = outPoint;
    data.viewTimeStart     = partialView.getViewTimeStart();
    data.viewTimeEnd       = partialView.getViewTimeEnd();
    data.viewFreqLow       = partialView.getViewFreqLow();
    data.viewFreqHigh      = partialView.getViewFreqHigh();
    data.rightPanelVisible = rightPanelVisible;
    data.loopEnabled       = transportBar.isLoopEnabled();
    if (const auto* dw = dynamic_cast<const juce::DocumentWindow*>(getTopLevelComponent()))
        data.windowBounds = dw->getBounds();
    return data;
}

bool MainComponent::saveCurrentFile()
{
    const juce::String json = PfkrFormat::serialize(project, buildSaveData());
    if (!currentFile.replaceWithText(json))
        return false;

    setDirtyFlag(false);
    setWindowTitle("PartialFKR - " + currentFile.getFileNameWithoutExtension());
    MacProxyIcon::set(getTopLevelComponent(), currentFile);
    if (onMenuStateChanged) onMenuStateChanged();
    return true;
}

void MainComponent::saveProjectAs(std::function<void(bool)> onComplete)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save project", juce::File{}, "*.pfkr");

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, onComplete](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty())
            {
                if (onComplete) onComplete(false);
                return;
            }
            currentFile = results[0].withFileExtension("pfkr");
            const bool ok = saveCurrentFile();
            if (onComplete) onComplete(ok);
        });
}

void MainComponent::loadProject(const juce::File& pfkrFile)
{
    const juce::String json = pfkrFile.loadFileAsString();
    if (json.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, "Open failed",
            "Could not read " + pfkrFile.getFileName());
        return;
    }

    auto result = PfkrFormat::deserialize(json, project);
    if (!result.success)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon, "Open failed",
            result.errorMessage);
        return;
    }

    currentFile = pfkrFile;

    inPoint  = result.data.inPoint;
    outPoint = result.data.outPoint;
    partialView.setMarkers(inPoint, outPoint);
    partialView.setVisibleTimeRange(result.data.viewTimeStart, result.data.viewTimeEnd);
    partialView.setVisibleFreqRange(result.data.viewFreqLow,   result.data.viewFreqHigh);

    rightPanelVisible = result.data.rightPanelVisible;
    transportBar.setLoopEnabled(result.data.loopEnabled);
    resized();
    if (onMenuStateChanged) onMenuStateChanged();

    if (!result.data.windowBounds.isEmpty())
        if (auto* dw = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent()))
            dw->setBounds(result.data.windowBounds);

    const double dur = project.getPartials().empty() ? 0.0
                     : project.getPartials().back()->endTime();
    transportBar.setDuration(dur);
    transportBar.clearPlayRange();
    transportBar.setState(TransportBar::State::Stopped);
    transportBar.setPlayheadPosition(0.0);
    partialView.setPlayheadTime(0.0);

    synth.setPlaying(false);
    synth.setProject(&project);
    synth.setPlayheadTime(0.0);
    synth.setSolo(project.hasAnySolo());

    setWindowTitle("PartialFKR - " + pfkrFile.getFileNameWithoutExtension());
    MacProxyIcon::set(getTopLevelComponent(), pfkrFile);
    setDirtyFlag(false);
    grabKeyboardFocus();
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::selectionChanged(Selection& sel)
{
    // When partials are selected, solo them so only selected partials play back.
    // Clearing the selection returns to full playback.
    const auto& partials = project.getPartials();
    const bool  hasSelection = !sel.isEmpty();

    for (const auto& p : partials)
        p->soloed.store(hasSelection && sel.isSelected(p->getId()),
                        std::memory_order_relaxed);

    synth.setSolo(hasSelection);
    if (onMenuStateChanged) onMenuStateChanged();
}

// ── Edit operations ───────────────────────────────────────────────────────────

void MainComponent::editCopy()
{
    clipboard.clear();
    const auto& sel = project.getSelection();
    for (const auto& p : project.getPartials())
    {
        if (!sel.isSelected(p->getId()))
            continue;
        PartialClip clip;
        clip.breakpoints = p->breakpoints;
        clip.colour      = p->getColour();
        clipboard.push_back(std::move(clip));
    }
    if (onMenuStateChanged) onMenuStateChanged();  // Paste now enabled
}

void MainComponent::editCut()
{
    editCopy();
    editDelete();
}

void MainComponent::editUndo()
{
    project.getEditHistory().undo();
    synth.setProject(&project);
}

void MainComponent::editRedo()
{
    project.getEditHistory().redo();
    synth.setProject(&project);
}

void MainComponent::editStretch()
{
    const bool inDirectSelect = (partialView.getToolMode() == PartialView::ToolMode::DirectSelect);
    const auto& bpSel     = partialView.getSelectedBreakpoints();
    const auto& partialSel = project.getSelection();

    // Must have something selected
    const bool hasMaterial = inDirectSelect ? !bpSel.empty() : !partialSel.isEmpty();
    if (!hasMaterial)
        return;

    // Dialog: ask for stretch factor as a percentage
    auto* dialog = new juce::AlertWindow(
        "Stretch / Compress",
        "Enter a stretch factor.\n"
        "1.0 = no change, 2.0 = twice as long, 0.5 = half as long",
        juce::MessageBoxIconType::NoIcon,
        getTopLevelComponent());

    dialog->addTextEditor("factor", "1.0", "Factor:");
    dialog->addButton("Apply",  1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dialog, inDirectSelect](int result) {
            if (result != 1) return;

            const double factor = dialog->getTextEditorContents("factor").getDoubleValue();
            if (factor <= 0.0) return;

            if (inDirectSelect)
                editStretchBreakpoints(factor);
            else
                editStretchPartials(factor);
        }),
        true /* deleteWhenDismissed */);
}

void MainComponent::editStretchPartials(double factor)
{
    const auto& sel     = project.getSelection();
    const auto& partials = project.getPartials();

    // Collect selected partials and find the time anchor (earliest start time)
    std::vector<const Partial*> targets;
    double anchor = std::numeric_limits<double>::max();
    for (const auto& p : partials)
    {
        if (!sel.isSelected(p->getId())) continue;
        targets.push_back(p.get());
        if (!p->breakpoints.empty())
            anchor = std::min(anchor, p->breakpoints.front().time);
    }
    if (targets.empty() || anchor == std::numeric_limits<double>::max()) return;

    std::vector<PartialSnapshot> originals, stretched;
    originals.reserve(targets.size());
    stretched.reserve(targets.size());

    for (const auto* p : targets)
    {
        originals.push_back(snapshotOf(*p));

        PartialSnapshot s = snapshotOf(*p);
        for (auto& bp : s.breakpoints)
            bp.time = anchor + (bp.time - anchor) * factor;
        stretched.push_back(std::move(s));
    }

    project.getEditHistory().perform(
        new StretchAction(project, std::move(originals), std::move(stretched)),
        "Stretch");
    synth.setProject(&project);
}

void MainComponent::editStretchBreakpoints(double factor)
{
    const auto& bpSel   = partialView.getSelectedBreakpoints();
    const auto& partials = project.getPartials();

    // Collect affected partials and find the global selection time bounds
    std::unordered_set<uint32_t> affectedIds;
    for (const auto& ref : bpSel) affectedIds.insert(ref.partialId);

    double selStart = std::numeric_limits<double>::max();
    double selEnd   = std::numeric_limits<double>::lowest();
    for (const auto& ref : bpSel)
    {
        if (const auto* p = project.findPartialById(ref.partialId))
            if (ref.bpIndex < p->breakpoints.size())
            {
                const double t = p->breakpoints[ref.bpIndex].time;
                selStart = std::min(selStart, t);
                selEnd   = std::max(selEnd,   t);
            }
    }
    if (selStart > selEnd) return;

    // Tail breakpoints shift right by the amount the body expands
    const double expansion = (selEnd - selStart) * (factor - 1.0);

    std::vector<PartialSnapshot> originals, stretched;

    for (const auto& p : partials)
    {
        if (!affectedIds.count(p->getId())) continue;
        originals.push_back(snapshotOf(*p));

        PartialSnapshot s = snapshotOf(*p);
        for (auto& bp : s.breakpoints)
        {
            if (bp.time <= selStart)
            {
                // Head — before the selection, no change
            }
            else if (bp.time <= selEnd)
            {
                // Body — scale from selStart anchor
                bp.time = selStart + (bp.time - selStart) * factor;
            }
            else
            {
                // Tail — shift right to accommodate the expanded body
                bp.time += expansion;
            }
        }
        stretched.push_back(std::move(s));
    }

    partialView.clearBreakpointSelection();
    project.getEditHistory().perform(
        new StretchAction(project, std::move(originals), std::move(stretched)),
        "Stretch Breakpoints");
    synth.setProject(&project);
}

void MainComponent::editBreakpointDelete()
{
    const auto& bpSel = partialView.getSelectedBreakpoints();
    if (bpSel.empty())
        return;

    // Group by partialId
    std::unordered_map<uint32_t, std::vector<size_t>> deletions;
    for (const auto& ref : bpSel)
        deletions[ref.partialId].push_back(ref.bpIndex);

    partialView.clearBreakpointSelection();
    project.getEditHistory().perform(
        new DeleteBreakpointsAction(project, std::move(deletions)),
        "Delete Breakpoints");
    synth.setProject(&project);
}

void MainComponent::editDelete()
{
    const auto ids = project.getSelection().getSelectedIds();
    if (ids.empty())
        return;
    project.getEditHistory().perform(new DeletePartialsAction(project, ids), "Delete");
    synth.setProject(&project);
}

void MainComponent::editPaste()
{
    if (clipboard.empty())
        return;

    double earliestTime = std::numeric_limits<double>::max();
    for (const auto& clip : clipboard)
        if (!clip.breakpoints.empty())
            earliestTime = std::min(earliestTime, clip.breakpoints.front().time);

    if (earliestTime == std::numeric_limits<double>::max())
        return;

    const double timeOffset = partialView.getPasteTime() - earliestTime;
    uint32_t     nextId     = project.nextPartialId();

    std::vector<PartialSnapshot> snapshots;
    snapshots.reserve(clipboard.size());

    for (const auto& clip : clipboard)
    {
        PartialSnapshot s;
        s.id         = nextId++;
        s.colour     = clip.colour;
        s.breakpoints = clip.breakpoints;
        for (auto& bp : s.breakpoints)
            bp.time += timeOffset;
        snapshots.push_back(std::move(s));
    }

    project.getSelection().clear();
    project.getEditHistory().perform(new AddPartialsAction(project, std::move(snapshots)), "Paste");
    synth.setProject(&project);
}

// ── Transport ─────────────────────────────────────────────────────────────────

void MainComponent::updateMarkers(double in, double out)
{
    inPoint  = in;
    outPoint = out;
    partialView.setMarkers(in, out);
}

double MainComponent::totalDuration() const noexcept
{
    const auto& p = project.getPartials();
    return p.empty() ? 0.0 : p.back()->endTime();
}

void MainComponent::handlePlayPauseToggle()
{
    switch (transportBar.getState())
    {
        case TransportBar::State::Stopped: handlePlay();   break;
        case TransportBar::State::Playing: handlePause();  break;
        case TransportBar::State::Paused:  handleResume(); break;
    }
}

void MainComponent::handlePlay()
{
    const double pos    = transportBar.getPlayheadPosition();
    const double effOut = (outPoint >= 0.0) ? outPoint : totalDuration();
    playStartedWithinRange = (pos < effOut);

    if (playStartedWithinRange)
        transportBar.setPlayRange(inPoint, effOut);  // loop-back target = inPoint
    else
        transportBar.clearPlayRange();               // play to end of file

    partialView.setPlayStartPos(pos);   // ghost line at play-start position
    synth.setSolo(project.hasAnySolo());
    synth.setPlayheadTime(pos);
    synth.setPlaying(true);
    transportBar.setState(TransportBar::State::Playing);
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::handleResume()
{
    // Re-evaluate from current position in case user scrubbed while paused
    const double pos    = transportBar.getPlayheadPosition();
    const double effOut = (outPoint >= 0.0) ? outPoint : totalDuration();
    playStartedWithinRange = (pos < effOut);

    if (playStartedWithinRange)
        transportBar.setPlayRange(inPoint, effOut);
    else
        transportBar.clearPlayRange();

    synth.setSolo(project.hasAnySolo());
    synth.setPlayheadTime(pos);
    synth.setPlaying(true);
    transportBar.setState(TransportBar::State::Playing);
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::handlePause()
{
    synth.setPlaying(false);
    transportBar.setState(TransportBar::State::Paused);
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::handleStop()
{
    synth.setPlaying(false);
    synth.setPlayheadTime(inPoint);
    transportBar.clearPlayRange();
    transportBar.setState(TransportBar::State::Stopped);
    transportBar.setPlayheadPosition(inPoint);
    partialView.setPlayheadTime(inPoint);
    partialView.setPlayStartPos(-1.0);   // hide ghost line
    if (onMenuStateChanged) onMenuStateChanged();
}

void MainComponent::handlePlayComplete()
{
    if (transportBar.isLoopEnabled() && playStartedWithinRange)
    {
        synth.setPlayheadTime(inPoint);
        transportBar.setPlayheadPosition(inPoint);
        partialView.setPlayheadTime(inPoint);
        // State stays Playing — timer continues
    }
    else
    {
        handleStop();
    }
}

// ── Scrub ─────────────────────────────────────────────────────────────────────

void MainComponent::handleScrubStart(double t)
{
    scrubWasPlaying = synth.getIsPlaying();
    if (scrubWasPlaying)
        synth.setPlaying(false);

    // Save current solo state so we can restore it on scrub end
    scrubSavedSoloIds.clear();
    for (const auto& p : project.getPartials())
        if (p->soloed.load(std::memory_order_relaxed))
            scrubSavedSoloIds.push_back(p->getId());

    // If partials are selected, solo only those; otherwise hear all
    const auto selectedIds = project.getSelection().getSelectedIds();
    if (!selectedIds.empty())
    {
        // Clear existing solos, then apply selection as temporary solos
        for (const auto& p : project.getPartials())
            p->soloed.store(false, std::memory_order_relaxed);
        for (uint32_t id : selectedIds)
            if (auto* p = project.findPartialById(id))
                p->soloed.store(true, std::memory_order_relaxed);
        synth.setSolo(true);
    }
    else
    {
        synth.setSolo(false);   // hear all non-muted partials
    }

    synth.setScrubPosition(t);
    synth.setScrubbing(true);
    transportBar.setPlayheadPosition(t);
}

void MainComponent::handleScrubUpdate(double t)
{
    synth.setScrubPosition(t);
    transportBar.setPlayheadPosition(t);
}

void MainComponent::handleScrubEnd()
{
    synth.setScrubbing(false);

    // Restore solo state that was active before scrubbing
    for (const auto& p : project.getPartials())
        p->soloed.store(false, std::memory_order_relaxed);
    for (uint32_t id : scrubSavedSoloIds)
        if (auto* p = project.findPartialById(id))
            p->soloed.store(true, std::memory_order_relaxed);
    synth.setSolo(project.hasAnySolo());

    // Leave playhead where the cursor stopped
    const double pos = synth.getPlayheadSeconds();
    if (scrubWasPlaying)
        synth.setPlaying(true);

    transportBar.setPlayheadPosition(pos);
    partialView.setPlayheadTime(pos);

    if (!scrubWasPlaying)
        transportBar.setState(TransportBar::State::Stopped);
}

// ── ApplicationCommandTarget ──────────────────────────────────────────────────

juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget()
{
    return juce::JUCEApplication::getInstance();
}

void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    commands.add(CommandIDs::fileClose);
    commands.add(CommandIDs::fileAnalyzeAudio);
    commands.add(CommandIDs::fileSave);
    commands.add(CommandIDs::fileSaveAs);
    commands.add(CommandIDs::exportMidi);
    commands.add(CommandIDs::exportMidiPackage);
    commands.add(CommandIDs::exportCsound);
    commands.add(CommandIDs::exportSuperCollider);
    commands.add(CommandIDs::exportSdif);
    commands.add(CommandIDs::exportJson);
    commands.add(juce::StandardApplicationCommandIDs::undo);
    commands.add(juce::StandardApplicationCommandIDs::redo);
    commands.add(juce::StandardApplicationCommandIDs::cut);
    commands.add(juce::StandardApplicationCommandIDs::copy);
    commands.add(juce::StandardApplicationCommandIDs::paste);
    commands.add(juce::StandardApplicationCommandIDs::del);
    commands.add(juce::StandardApplicationCommandIDs::selectAll);
    commands.add(juce::StandardApplicationCommandIDs::deselectAll);
    commands.add(CommandIDs::invertSelection);
    commands.add(CommandIDs::normalize);
    commands.add(CommandIDs::scaleAmplitude);
    commands.add(CommandIDs::editBridgePartials);
    commands.add(CommandIDs::editCrossfadeOverlap);
    commands.add(CommandIDs::editStretch);
    commands.add(CommandIDs::viewZoomIn);
    commands.add(CommandIDs::viewZoomOut);
    commands.add(CommandIDs::viewZoomFit);
    commands.add(CommandIDs::viewTogglePanel);
    commands.add(CommandIDs::transportPlayPause);
    commands.add(CommandIDs::transportStop);
    commands.add(CommandIDs::transportLoop);
}

void MainComponent::getCommandInfo(juce::CommandID commandID,
                                   juce::ApplicationCommandInfo& result)
{
    const bool hasPartials  = !project.getPartials().empty();
    const bool hasSelection = !project.getSelection().isEmpty();

    switch (commandID)
    {
        case CommandIDs::fileClose:
            result.setInfo("Close", "Close the current window", "File", 0);
            result.addDefaultKeypress('w', juce::ModifierKeys::commandModifier);
            result.setActive(true);
            break;

        case CommandIDs::fileAnalyzeAudio:
            result.setInfo("Analyze Audio...", "Load an audio file for sinusoidal analysis", "File", 0);
            result.setActive(!project.hasSourceAudio());
            break;

        case CommandIDs::fileSave:
            result.setInfo("Save", "Save the project", "File", 0);
            result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
            result.setActive(hasPartials || currentFile.existsAsFile());
            break;

        case CommandIDs::fileSaveAs:
            result.setInfo("Save As...", "Save the project to a new file", "File", 0);
            result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier
                                          | juce::ModifierKeys::shiftModifier);
            result.setActive(hasPartials);
            break;

        case CommandIDs::exportMidi:
            result.setInfo("Export as MIDI/MPE...", "Export partials as a single MPE MIDI file", "File", 0);
            result.setActive(hasPartials);
            break;
        case CommandIDs::exportMidiPackage:
            result.setInfo("Export MIDI Package...", "Export one MIDI file per partial with BOM and readme", "File", 0);
            result.setActive(hasPartials);
            break;
        case CommandIDs::exportCsound:
            result.setInfo("Export as Csound CSD...", "Export partials as a self-contained Csound CSD file", "File", 0);
            result.setActive(hasPartials);
            break;
        case CommandIDs::exportSuperCollider:
            result.setInfo("Export as SuperCollider...", "Export partials as a SuperCollider .scd file", "File", 0);
            result.setActive(hasPartials);
            break;
        case CommandIDs::exportSdif:
            result.setInfo("Export as SDIF...", "Export partials as an SDIF file", "File", 0);
            result.setActive(hasPartials);
            break;
        case CommandIDs::exportJson:
            result.setInfo("Export as JSON...", "Export partials as a JSON file", "File", 0);
            result.setActive(hasPartials);
            break;

        case juce::StandardApplicationCommandIDs::undo:
            result.setInfo("Undo", project.getEditHistory().getUndoDescription(), "Edit", 0);
            result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
            result.setActive(project.getEditHistory().canUndo());
            break;
        case juce::StandardApplicationCommandIDs::redo:
            result.setInfo("Redo", project.getEditHistory().getRedoDescription(), "Edit", 0);
            result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier
                                           | juce::ModifierKeys::shiftModifier);
            result.setActive(project.getEditHistory().canRedo());
            break;
        case juce::StandardApplicationCommandIDs::cut:
            result.setInfo("Cut", "Cut selected partials to clipboard", "Edit", 0);
            result.addDefaultKeypress('x', juce::ModifierKeys::commandModifier);
            result.setActive(hasSelection);
            break;
        case juce::StandardApplicationCommandIDs::copy:
            result.setInfo("Copy", "Copy selected partials to clipboard", "Edit", 0);
            result.addDefaultKeypress('c', juce::ModifierKeys::commandModifier);
            result.setActive(hasSelection);
            break;
        case juce::StandardApplicationCommandIDs::paste:
            result.setInfo("Paste", "Paste partials from clipboard", "Edit", 0);
            result.addDefaultKeypress('v', juce::ModifierKeys::commandModifier);
            result.setActive(!clipboard.empty());
            break;
        case juce::StandardApplicationCommandIDs::del:
            result.setInfo("Delete", "Delete selected partials", "Edit", 0);
            result.setActive(hasSelection);
            break;
        case juce::StandardApplicationCommandIDs::selectAll:
            result.setInfo("Select All", "Select all partials", "Edit", 0);
            result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier);
            result.setActive(hasPartials);
            break;
        case juce::StandardApplicationCommandIDs::deselectAll:
            result.setInfo("Deselect All", "Clear the selection", "Edit", 0);
            result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier
                                           | juce::ModifierKeys::shiftModifier);
            result.setActive(hasSelection);
            break;

        case CommandIDs::invertSelection:
            result.setInfo("Invert Selection", "Select all unselected partials and deselect selected ones", "Edit", 0);
            result.addDefaultKeypress('i', juce::ModifierKeys::commandModifier
                                           | juce::ModifierKeys::shiftModifier);
            result.setActive(hasPartials);
            break;

        case CommandIDs::editBridgePartials:
        {
            result.setInfo("Bridge Partials", "Interpolate across the gap between two partial fragments", "Edit", 0);
            result.addDefaultKeypress('j', juce::ModifierKeys::commandModifier);
            bool canBridge = false;
            if (partialView.getToolMode() == PartialView::ToolMode::Selection)
            {
                const auto ids = project.getSelection().getSelectedIds();
                if (ids.size() == 2)
                {
                    const auto* a = project.findPartialById(ids[0]);
                    const auto* b = project.findPartialById(ids[1]);
                    if (a && b)
                        canBridge = (a->endTime() <= b->startTime() ||
                                     b->endTime() <= a->startTime());
                }
            }
            else if (partialView.getToolMode() == PartialView::ToolMode::DirectSelect)
            {
                const auto& bpSel = partialView.getSelectedBreakpoints();
                if (bpSel.size() == 2)
                {
                    auto it = bpSel.begin();
                    const uint32_t idX = it->partialId; ++it;
                    const uint32_t idY = it->partialId;
                    if (idX != idY)
                    {
                        const auto* x = project.findPartialById(idX);
                        const auto* y = project.findPartialById(idY);
                        if (x && y)
                            canBridge = (x->endTime() <= y->startTime() ||
                                         y->endTime() <= x->startTime());
                    }
                }
            }
            result.setActive(canBridge);
            break;
        }
        case CommandIDs::editCrossfadeOverlap:
        {
            result.setInfo("Crossfade Overlap", "Crossfade two overlapping partials into one", "Edit", 0);
            result.addDefaultKeypress('j', juce::ModifierKeys::commandModifier
                                          | juce::ModifierKeys::shiftModifier);
            bool canCrossfade = false;
            if (partialView.getToolMode() == PartialView::ToolMode::Selection)
            {
                const auto ids = project.getSelection().getSelectedIds();
                if (ids.size() == 2)
                {
                    const auto* a = project.findPartialById(ids[0]);
                    const auto* b = project.findPartialById(ids[1]);
                    if (a && b)
                        canCrossfade = (a->startTime() < b->endTime() &&
                                        b->startTime() < a->endTime());
                }
            }
            result.setActive(canCrossfade);
            break;
        }

        case CommandIDs::editStretch:
        {
            result.setInfo("Stretch / Compress...", "Time-scale selected partials or breakpoints", "Edit", 0);
            result.addDefaultKeypress('t', 0);
            bool canStretch = false;
            if (partialView.getToolMode() == PartialView::ToolMode::Selection)
                canStretch = !project.getSelection().isEmpty();
            else if (partialView.getToolMode() == PartialView::ToolMode::DirectSelect)
                canStretch = !partialView.getSelectedBreakpoints().empty();
            result.setActive(canStretch);
            break;
        }

        case CommandIDs::normalize:
            result.setInfo("Normalize", "Scale all unmuted partials so the peak output = 0 dBFS", "Edit", 0);
            result.setActive(hasPartials && !isNormalizing);
            break;

        case CommandIDs::scaleAmplitude:
            result.setInfo("Scale Amplitude...", "Multiply partial amplitudes by a dB offset or linear factor", "Edit", 0);
            result.setActive(hasPartials && !isNormalizing);
            break;

        case CommandIDs::viewZoomIn:
            result.setInfo("Zoom In", "Zoom in on the time axis", "View", 0);
            result.addDefaultKeypress('=', juce::ModifierKeys::commandModifier);
            result.setActive(true);
            break;
        case CommandIDs::viewZoomOut:
            result.setInfo("Zoom Out", "Zoom out on the time axis", "View", 0);
            result.addDefaultKeypress('-', juce::ModifierKeys::commandModifier);
            result.setActive(true);
            break;
        case CommandIDs::viewZoomFit:
            result.setInfo("Zoom to Fit", "Reset zoom to show all content", "View", 0);
            result.addDefaultKeypress('0', juce::ModifierKeys::commandModifier);
            result.setActive(hasPartials);
            break;
        case CommandIDs::viewTogglePanel:
            result.setInfo(rightPanelVisible ? "Hide Panel" : "Show Panel",
                           "Show or hide the right-hand controls panel", "View", 0);
            result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier
                                          | juce::ModifierKeys::altModifier);
            result.setActive(true);
            break;

        case CommandIDs::transportPlayPause:
        {
            const bool playing = transportBar.getState() == TransportBar::State::Playing;
            result.setInfo(playing ? "Pause" : "Play",
                           playing ? "Pause playback" : "Play / resume playback",
                           "Transport", 0);
            result.setActive(true);
            break;
        }
        case CommandIDs::transportStop:
            result.setInfo("Stop", "Stop playback and return to in-point", "Transport", 0);
            result.setActive(transportBar.getState() != TransportBar::State::Stopped);
            break;

        case CommandIDs::transportLoop:
            result.setInfo("Loop", "Toggle looped playback", "Transport", 0);
            result.setTicked(transportBar.isLoopEnabled());
            result.setActive(true);
            break;

        default: break;
    }
}

bool MainComponent::perform(const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::fileClose:        if (onCloseRequested) onCloseRequested(); return true;
        case CommandIDs::fileAnalyzeAudio: openFileAndAnalyze(); return true;
        case CommandIDs::fileSave:         saveProject();        return true;
        case CommandIDs::fileSaveAs:       saveProjectAs(nullptr); return true;
        case CommandIDs::exportMidi:        exportMidi();        return true;
        case CommandIDs::exportMidiPackage: exportMidiPackage(); return true;
        case CommandIDs::exportCsound:         exportCsound();         return true;
        case CommandIDs::exportSuperCollider:  exportSuperCollider();  return true;
        case CommandIDs::exportSdif:   exportSdif();         return true;
        case CommandIDs::exportJson:   exportJson();         return true;

        case juce::StandardApplicationCommandIDs::undo:      editUndo();   return true;
        case juce::StandardApplicationCommandIDs::redo:      editRedo();   return true;
        case juce::StandardApplicationCommandIDs::cut:   editCut();    return true;
        case juce::StandardApplicationCommandIDs::copy:  editCopy();   return true;
        case juce::StandardApplicationCommandIDs::paste: editPaste();  return true;
        case juce::StandardApplicationCommandIDs::del:       editDelete(); return true;

        case juce::StandardApplicationCommandIDs::selectAll:
        {
            std::vector<uint32_t> ids;
            for (const auto& p : project.getPartials())
                ids.push_back(p->getId());
            project.getSelection().selectAll(ids);
            return true;
        }
        case juce::StandardApplicationCommandIDs::deselectAll:
            project.getSelection().clear();
            return true;

        case CommandIDs::invertSelection:
        {
            std::vector<uint32_t> allIds;
            for (const auto& p : project.getPartials())
                allIds.push_back(p->getId());
            project.getSelection().invert(allIds);
            return true;
        }

        case CommandIDs::normalize:            performNormalize();        return true;
        case CommandIDs::scaleAmplitude:       performScaleAmplitude();   return true;
        case CommandIDs::editBridgePartials:   performBridgePartials();   return true;
        case CommandIDs::editCrossfadeOverlap: performCrossfadeOverlap(); return true;
        case CommandIDs::editStretch:          editStretch();             return true;

        case CommandIDs::viewZoomIn:  partialView.zoomIn();   return true;
        case CommandIDs::viewZoomOut: partialView.zoomOut();  return true;
        case CommandIDs::viewZoomFit: partialView.resetZoom(); return true;
        case CommandIDs::viewTogglePanel:
            rightPanelVisible = !rightPanelVisible;
            resized();
            if (onMenuStateChanged) onMenuStateChanged();
            return true;

        case CommandIDs::transportPlayPause: handlePlayPauseToggle();                                    return true;
        case CommandIDs::transportStop:      handleStop();                                               return true;
        case CommandIDs::transportLoop:      transportBar.setLoopEnabled(!transportBar.isLoopEnabled()); return true;

        default: return false;
    }
}

// ── Join operations ───────────────────────────────────────────────────────────

void MainComponent::performBridgePartials()
{
    const Partial* pA      = nullptr;
    const Partial* pB      = nullptr;
    size_t         bpIdxA  = 0;
    size_t         bpIdxB  = 0;

    if (partialView.getToolMode() == PartialView::ToolMode::Selection)
    {
        const auto ids = project.getSelection().getSelectedIds();
        if (ids.size() != 2) return;
        const auto* x = project.findPartialById(ids[0]);
        const auto* y = project.findPartialById(ids[1]);
        if (!x || !y) return;
        if (x->endTime() <= y->startTime())      { pA = x; pB = y; }
        else if (y->endTime() <= x->startTime()) { pA = y; pB = x; }
        else return;
        bpIdxA = pA->breakpoints.size() - 1;
        bpIdxB = 0;
    }
    else  // DirectSelect
    {
        const auto& bpSel = partialView.getSelectedBreakpoints();
        if (bpSel.size() != 2) return;
        auto it = bpSel.begin();
        const uint32_t idX = it->partialId; const size_t xiX = it->bpIndex; ++it;
        const uint32_t idY = it->partialId; const size_t xiY = it->bpIndex;
        if (idX == idY) return;
        const auto* x = project.findPartialById(idX);
        const auto* y = project.findPartialById(idY);
        if (!x || !y) return;
        if (x->endTime() > y->startTime() && y->endTime() > x->startTime()) return; // overlap
        if (x->breakpoints[xiX].time <= y->breakpoints[xiY].time)
            { pA = x; bpIdxA = xiX; pB = y; bpIdxB = xiY; }
        else
            { pA = y; bpIdxA = xiY; pB = x; bpIdxB = xiX; }
    }

    const Breakpoint& bpStart = pA->breakpoints[bpIdxA];
    const Breakpoint& bpEnd   = pB->breakpoints[bpIdxB];
    if (bpStart.time >= bpEnd.time) return;

    std::vector<Breakpoint> mergedBps;
    for (size_t i = 0; i <= bpIdxA; ++i)
        mergedBps.push_back(pA->breakpoints[i]);

    // Fill the gap at 5 ms intervals
    if ((bpEnd.time - bpStart.time) > kJoinInterval * 1.5)
        for (double t = bpStart.time + kJoinInterval;
             t < bpEnd.time - kJoinInterval * 0.5;
             t += kJoinInterval)
            mergedBps.push_back(lerpBP(bpStart, bpEnd, t, bpStart.time, bpEnd.time));

    for (size_t i = bpIdxB; i < pB->breakpoints.size(); ++i)
        mergedBps.push_back(pB->breakpoints[i]);

    project.getEditHistory().perform(
        new JoinPartialsAction(project,
                               snapshotOf(*pA), snapshotOf(*pB),
                               PartialSnapshot{ pA->getId(), pA->getColour(), mergedBps }),
        "Bridge Partials");

    project.getSelection().clear();
    partialView.clearBreakpointSelection();
}

void MainComponent::performCrossfadeOverlap()
{
    const auto ids = project.getSelection().getSelectedIds();
    if (ids.size() != 2) return;
    const auto* x = project.findPartialById(ids[0]);
    const auto* y = project.findPartialById(ids[1]);
    if (!x || !y) return;

    // A starts first
    const Partial* pA = (x->startTime() <= y->startTime()) ? x : y;
    const Partial* pB = (pA == x) ? y : x;

    const double overlapStart = pB->startTime();
    const double overlapEnd   = pA->endTime();
    if (overlapStart >= overlapEnd) return;

    std::vector<Breakpoint> mergedBps;

    for (const auto& bp : pA->breakpoints)
        if (bp.time < overlapStart)
            mergedBps.push_back(bp);

    const double overlapLen = overlapEnd - overlapStart;
    for (double t = overlapStart; t <= overlapEnd + kJoinInterval * 0.1; t += kJoinInterval)
    {
        const float      w   = static_cast<float>((t - overlapStart) / overlapLen);
        const Breakpoint bA  = evaluateAt(*pA, t);
        const Breakpoint bB  = evaluateAt(*pB, t);
        mergedBps.push_back({ t,
            bA.frequency + w * (bB.frequency - bA.frequency),
            bA.amplitude + w * (bB.amplitude - bA.amplitude),
            0.0f,
            bA.bandwidth + w * (bB.bandwidth - bA.bandwidth) });
    }

    for (const auto& bp : pB->breakpoints)
        if (bp.time > overlapEnd)
            mergedBps.push_back(bp);

    if (mergedBps.size() < 2) return;

    project.getEditHistory().perform(
        new JoinPartialsAction(project,
                               snapshotOf(*pA), snapshotOf(*pB),
                               PartialSnapshot{ pA->getId(), pA->getColour(), mergedBps }),
        "Crossfade Overlap");

    project.getSelection().clear();
}

// ── Amplitude operations ──────────────────────────────────────────────────────

void MainComponent::performNormalize()
{
    if (project.getPartials().empty() || isNormalizing)
        return;

    // Collect unmuted partials on the message thread — safe snapshot for background use
    std::vector<AmplitudeOps::SynthData> data;
    std::vector<uint32_t> unmutedIds;

    for (const auto& p : project.getPartials())
    {
        if (!p->muted.load(std::memory_order_relaxed))
        {
            data.push_back({ p->breakpoints });
            unmutedIds.push_back(p->getId());
        }
    }

    if (data.empty())
        return;

    isNormalizing = true;
    setWindowTitle("PartialFKR - Normalizing...");
    if (onMenuStateChanged) onMenuStateChanged();

    // Offline synthesis on a background thread; apply result on the message thread
    std::thread([this,
                 data      = std::move(data),
                 unmutedIds = std::move(unmutedIds)]() mutable
    {
        const float peak = AmplitudeOps::computePeak(data);

        juce::MessageManager::callAsync([this, peak, unmutedIds = std::move(unmutedIds)]()
        {
            if (peak > 0.0f)
                AmplitudeOps::scale(project, unmutedIds, 1.0f / peak);

            isNormalizing = false;
            setWindowTitle("PartialFKR - " + project.getSourceFile().getFileName());
            if (onMenuStateChanged) onMenuStateChanged();
        });
    }).detach();
}

void MainComponent::performScaleAmplitude()
{
    if (project.getPartials().empty())
        return;

    // Determine which partials to scale (selection, or all if nothing selected)
    std::vector<uint32_t> ids;
    const auto& sel = project.getSelection();
    if (!sel.isEmpty())
        ids = sel.getSelectedIds();
    else
        for (const auto& p : project.getPartials())
            ids.push_back(p->getId());

    if (ids.empty())
        return;

    const juce::String scopeDesc = sel.isEmpty()
                                 ? "all partials"
                                 : juce::String(ids.size()) + " selected partial(s)";

    // Use raw new — JUCE's ModalComponentManager deletes the window via deleteWhenDismissed.
    // make_shared causes a crash because the fused allocation gives an interior pointer
    // that free() rejects as "not allocated".
    auto* dialog = new juce::AlertWindow(
        "Scale Amplitude",
        "Adjust amplitude of " + scopeDesc + ".\n"
        "Enter a dB offset (e.g. +6, -3) or a linear factor prefixed with 'x' (e.g. x1.5):",
        juce::MessageBoxIconType::NoIcon,
        getTopLevelComponent());

    dialog->addTextEditor("value", "+0", "Adjustment:");
    dialog->addButton("Apply",  1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, dialog, ids](int result)
            {
                if (result != 1) return;

                const juce::String text = dialog->getTextEditorContents("value").trim();
                float factor = 1.0f;

                if (text.startsWithIgnoreCase("x"))
                {
                    factor = text.substring(1).getFloatValue();
                }
                else
                {
                    const float db = text.getFloatValue();
                    factor = std::pow(10.0f, db / 20.0f);
                }

                if (factor > 0.0f)
                    AmplitudeOps::scale(project, ids, factor);
            }),
        true);
}

// ── Export ────────────────────────────────────────────────────────────────────

void MainComponent::exportMidi()
{
    auto exportData = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    // Pre-pass: compute deviation histogram to guide bend-range selection
    const auto devs = MidiExporter::computeDeviationHistogram(*exportData);

    const float bendCandidateCents[] = { 200.f, 1200.f, 2400.f, 4800.f };
    const int   bendSemitones[]      = { 2, 12, 24, 48 };
    const int   numCandidates        = 4;

    auto* dialog = new juce::AlertWindow("MIDI / MPE Export",
                                         "Choose pitch bend range:",
                                         juce::MessageBoxIconType::NoIcon,
                                         getTopLevelComponent());

    juce::StringArray rangeItems;
    for (int i = 0; i < numCandidates; ++i)
    {
        const float limitCents = bendCandidateCents[i];
        const int   covered    = static_cast<int>(std::count_if(devs.begin(), devs.end(),
                                     [limitCents](float d) { return d <= limitCents; }));
        const int   pct        = devs.empty() ? 100
                                              : static_cast<int>(covered * 100 / (int) devs.size());
        rangeItems.add(juce::String("+/- ") + juce::String(bendSemitones[i])
                       + " semitones  (" + juce::String(pct) + "% covered)");
    }
    dialog->addComboBox("range", rangeItems);
    dialog->getComboBoxComponent("range")->setSelectedItemIndex(2);  // default ±24

    dialog->addButton("Export", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, dialog, exportData](int result) mutable {
                if (result != 1) return;
                const int idx = dialog->getComboBoxComponent("range")->getSelectedItemIndex();
                const int semitones[] = { 2, 12, 24, 48 };
                const int bendRange   = semitones[juce::jlimit(0, 3, idx)];

                auto chooser = std::make_shared<juce::FileChooser>(
                    "Save MIDI file", juce::File{}, "*.mid");
                chooser->launchAsync(
                    juce::FileBrowserComponent::saveMode |
                    juce::FileBrowserComponent::canSelectFiles |
                    juce::FileBrowserComponent::warnAboutOverwriting,
                    [exportData, chooser, bendRange](const juce::FileChooser& fc) {
                        const auto results = fc.getResults();
                        if (results.isEmpty()) return;
                        MidiExporter::Options opts;
                        opts.bendRangeSemitones = bendRange;
                        if (!MidiExporter::exportToFile(*exportData, opts, results[0]))
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "Export Failed", "Could not write MIDI file.", "OK");
                    });
            }),
        true);
}

void MainComponent::exportMidiPackage()
{
    auto exportData  = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    auto sourceFile = std::make_shared<juce::File>(project.getSourceFile());

    auto chooser = std::make_shared<juce::FileChooser>(
        "Choose export folder", juce::File{});
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories,
        [exportData, sourceFile, chooser](const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;

            MidiPackageExporter::Options opts;
            const auto result = MidiPackageExporter::exportPackage(
                *exportData, *sourceFile, results[0], opts);

            if (!result.success)
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Export Failed", result.errorMessage, "OK");
            else
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Export Complete",
                    juce::String(result.filesWritten) +
                        " MIDI files exported successfully.", "OK");
        });
}

void MainComponent::exportCsound()
{
    auto exportData = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Csound CSD file", juce::File{}, "*.csd");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [exportData, chooser](const juce::FileChooser& fc) {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;
            CsoundExporter::Options opts;
            if (!CsoundExporter::exportToFile(*exportData, opts, results[0]))
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Export Failed", "Could not write Csound CSD file.", "OK");
        });
}

void MainComponent::exportSuperCollider()
{
    auto exportData = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Save SuperCollider file", juce::File{}, "*.scd");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [exportData, chooser](const juce::FileChooser& fc) {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;
            SuperColliderExporter::Options opts;
            if (!SuperColliderExporter::exportToFile(*exportData, opts, results[0]))
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Export Failed", "Could not write SuperCollider file.", "OK");
        });
}

void MainComponent::exportSdif()
{
    auto exportData = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    auto* dialog = new juce::AlertWindow("SDIF Export",
                                         "Choose frame type:",
                                         juce::MessageBoxIconType::NoIcon,
                                         getTopLevelComponent());
    dialog->addComboBox("type", { "1TRC  (standard, broad compatibility)",
                                  "RBEP  (Loris bandwidth-enhanced, preserves bandwidth field)" });
    dialog->getComboBoxComponent("type")->setSelectedItemIndex(0);

    dialog->addButton("Export", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, dialog, exportData](int result) mutable {
                if (result != 1) return;
                const bool useRbep =
                    (dialog->getComboBoxComponent("type")->getSelectedItemIndex() == 1);

                auto chooser = std::make_shared<juce::FileChooser>(
                    "Save SDIF file", juce::File{}, "*.sdif");
                chooser->launchAsync(
                    juce::FileBrowserComponent::saveMode |
                    juce::FileBrowserComponent::canSelectFiles |
                    juce::FileBrowserComponent::warnAboutOverwriting,
                    [exportData, chooser, useRbep](const juce::FileChooser& fc) {
                        const auto results = fc.getResults();
                        if (results.isEmpty()) return;
                        SdifExporter::Options opts;
                        opts.frameType = useRbep ? SdifExporter::FrameType::LorisRBEP
                                                 : SdifExporter::FrameType::Standard1TRC;
                        if (!SdifExporter::exportToFile(*exportData, opts, results[0]))
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "Export Failed", "Could not write SDIF file.", "OK");
                    });
            }),
        true);
}

void MainComponent::exportJson()
{
    auto exportData = std::make_shared<std::vector<std::unique_ptr<Partial>>>(
        filterMuted(project.getPartials()));
    if (exportData->empty()) return;

    const juce::String sourceName = project.getSourceFile().getFileName();
    const double       sr         = project.getSampleRate();
    const double       dur        = exportData->empty() ? 0.0
                                  : exportData->back()->endTime();

    auto chooser = std::make_shared<juce::FileChooser>(
        "Save JSON file", juce::File{}, "*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [exportData, chooser, sourceName, sr, dur](const juce::FileChooser& fc) {
            const auto results = fc.getResults();
            if (results.isEmpty()) return;
            JsonExporter::Options opts;
            opts.sourceFileName   = sourceName;
            opts.sampleRate       = sr;
            opts.durationSeconds  = dur;
            if (!JsonExporter::exportToFile(*exportData, opts, results[0]))
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Export Failed", "Could not write JSON file.", "OK");
        });
}