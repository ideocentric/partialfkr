// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <unordered_set>
#include <vector>

/**
 * Tracks which partials the user has selected. Keyed by stable partial ID.
 * Lives on the message thread; not accessed from the audio thread.
 */
class Selection {
public:
    void select(uint32_t id);
    void deselect(uint32_t id);
    void toggle(uint32_t id);
    void selectAll(const std::vector<uint32_t>& ids);
    void invert   (const std::vector<uint32_t>& allIds);
    void clear();

    [[nodiscard]] bool isSelected(uint32_t id) const noexcept;
    [[nodiscard]] size_t count() const noexcept { return selected.size(); }
    [[nodiscard]] bool isEmpty() const noexcept { return selected.empty(); }

    [[nodiscard]] std::vector<uint32_t> getSelectedIds() const;

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void selectionChanged(Selection&) = 0;
    };

    void addListener(Listener* l)    { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    void notify();

    std::unordered_set<uint32_t>  selected;
    juce::ListenerList<Listener>  listeners;
};