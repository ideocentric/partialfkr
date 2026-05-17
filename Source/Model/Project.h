// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"
#include "EditHistory.h"
#include "Selection.h"

#include <JuceHeader.h>

#include <memory>
#include <unordered_map>
#include <vector>

/**
 * Top-level document object. Owns all partials and the source audio.
 *
 * UI and synth hold raw pointers/references into partials; they never copy
 * Partial objects. Partial IDs are stable across the document's lifetime.
 */
class Project {
public:
    Project();

    // ── Source audio ─────────────────────────────────────────────────────────
    void loadSourceAudio(const juce::File& file);
    /** Set the source file path without loading the audio buffer (used on .pfkr load). */
    void setSourceFilePath(const juce::File& file) { sourceFile = file; }
    [[nodiscard]] const juce::AudioBuffer<float>& getSourceAudio() const { return sourceAudio; }
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate; }
    [[nodiscard]] juce::File getSourceFile() const { return sourceFile; }
    /** True when the project has source audio attached (partials exist or source file is set). */
    [[nodiscard]] bool hasSourceAudio() const noexcept
    {
        return !partials.empty() || sourceFile != juce::File{};
    }

    // ── Partials ──────────────────────────────────────────────────────────────

    /** Replace all partials (called after analysis completes). */
    void setPartials(std::vector<std::unique_ptr<Partial>> newPartials);

    /** Store the immutable original Loris snapshot (deep-copied from src). */
    void setOriginalPartials(const std::vector<std::unique_ptr<Partial>>& src);

    [[nodiscard]] const std::vector<std::unique_ptr<Partial>>& getOriginalPartials() const noexcept
    {
        return originalPartials;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Partial>>& getPartials() const noexcept
    {
        return partials;
    }

    [[nodiscard]] size_t getPartialCount() const noexcept { return partials.size(); }

    /** Returns nullptr if id not found. */
    [[nodiscard]] Partial* findPartialById(uint32_t id) const;

    /** Delete partials by ID. */
    void deletePartials(const std::vector<uint32_t>& ids);

    /** Append new partials, assigning stable IDs above the current maximum. */
    void addPartials(std::vector<std::unique_ptr<Partial>> newPartials);

    /**
     * Replace existing partials in-place by matching ID.
     * Partials not found in the replacement list are left unchanged.
     * Calls notifyPartialsChanged once.
     */
    void replacePartials(std::vector<std::unique_ptr<Partial>> replacements);

    /** Returns the next available partial ID (max existing + 1). */
    [[nodiscard]] uint32_t nextPartialId() const noexcept;

    /**
     * Remove specific breakpoints from partials by (partialId, bpIndex) pairs.
     * Contiguous remaining breakpoints form new partials (trim/split as needed).
     * Runs shorter than 2 breakpoints are discarded. Calls notifyPartialsChanged.
     * Returns the IDs of any newly created split partials (for undo).
     */
    std::vector<uint32_t> applyBreakpointDeletion(
        const std::unordered_map<uint32_t, std::vector<size_t>>& deletions);

    // ── Mute / solo ───────────────────────────────────────────────────────────

    void setMuted(uint32_t partialId, bool mute);
    void setMutedBatch(const std::vector<uint32_t>& ids, bool mute);
    void setSoloed(uint32_t partialId, bool solo);
    void clearAllSolo();

    /** Whether any partial currently has solo set. */
    [[nodiscard]] bool hasAnySolo() const noexcept;

    // ── Sub-objects ───────────────────────────────────────────────────────────

    [[nodiscard]] Selection&      getSelection() noexcept { return selection; }
    [[nodiscard]] const Selection& getSelection() const noexcept { return selection; }
    [[nodiscard]] EditHistory&    getEditHistory() noexcept { return editHistory; }

    // ── Metadata ──────────────────────────────────────────────────────────────

    [[nodiscard]] juce::String getName() const { return projectName; }
    void setName(const juce::String& name) { projectName = name; }

    /** Listeners notified on the message thread when the partial set changes. */
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void partialsChanged(Project&) {}
        virtual void muteStateChanged(Project&, uint32_t /*partialId*/) {}
    };

    void addListener(Listener* l)    { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    /** Broadcasts partialsChanged() to all listeners. Public so external controllers
     *  (e.g. ReductionController) can notify views after modifying display-relevant
     *  state (such as mute flags) without altering the partials list itself. */
    void notifyPartialsChanged();

private:
    void notifyMuteChanged(uint32_t id);

    std::vector<std::unique_ptr<Partial>> partials;
    std::vector<std::unique_ptr<Partial>> originalPartials;  ///< immutable Loris snapshot
    juce::AudioBuffer<float>              sourceAudio;
    double                                sampleRate = 0.0;
    juce::File                            sourceFile;
    juce::String                          projectName;

    Selection   selection;
    EditHistory editHistory;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};