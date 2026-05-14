// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * Thin wrapper around juce::UndoManager that provides the project's undo/redo
 * stack. Slider-drag gestures are grouped with beginNewTransaction() before
 * the first drag event; per-click operations commit atomically.
 */
class EditHistory {
public:
    EditHistory();

    [[nodiscard]] juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    void beginTransaction(const juce::String& description = {});
    void undo();
    void redo();

    [[nodiscard]] bool canUndo() const noexcept { return undoManager.canUndo(); }
    [[nodiscard]] bool canRedo() const noexcept { return undoManager.canRedo(); }

    [[nodiscard]] juce::String getUndoDescription() const { return undoManager.getUndoDescription(); }
    [[nodiscard]] juce::String getRedoDescription() const { return undoManager.getRedoDescription(); }

    void perform(juce::UndoableAction* action, const juce::String& description = {});

private:
    juce::UndoManager undoManager{30'000'000, 200};  // 30 MB / 200 actions max

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditHistory)
};