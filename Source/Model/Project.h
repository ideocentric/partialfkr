// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Analysis/PartialData.h"
#include "EditHistory.h"
#include "Selection.h"

#include <JuceHeader.h>

#include <memory>
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
    [[nodiscard]] const juce::AudioBuffer<float>& getSourceAudio() const { return sourceAudio; }
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate; }
    [[nodiscard]] juce::File getSourceFile() const { return sourceFile; }

    // ── Partials ──────────────────────────────────────────────────────────────

    /** Replace all partials (called after analysis completes). */
    void setPartials(std::vector<std::unique_ptr<Partial>> newPartials);

    [[nodiscard]] const std::vector<std::unique_ptr<Partial>>& getPartials() const noexcept
    {
        return partials;
    }

    [[nodiscard]] size_t getPartialCount() const noexcept { return partials.size(); }

    /** Returns nullptr if id not found. */
    [[nodiscard]] Partial* findPartialById(uint32_t id) const;

    /** Delete partials by ID. Undoable. */
    void deletePartials(const std::vector<uint32_t>& ids);

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

private:
    void notifyPartialsChanged();
    void notifyMuteChanged(uint32_t id);

    std::vector<std::unique_ptr<Partial>> partials;
    juce::AudioBuffer<float>              sourceAudio;
    double                                sampleRate = 0.0;
    juce::File                            sourceFile;
    juce::String                          projectName;

    Selection   selection;
    EditHistory editHistory;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};