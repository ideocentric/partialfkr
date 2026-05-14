// SPDX-License-Identifier: AGPL-3.0-or-later
#include "EditHistory.h"

EditHistory::EditHistory() = default;

void EditHistory::beginTransaction(const juce::String& description)
{
    undoManager.beginNewTransaction(description);
}

void EditHistory::undo()
{
    undoManager.undo();
}

void EditHistory::redo()
{
    undoManager.redo();
}

void EditHistory::perform(juce::UndoableAction* action, const juce::String& description)
{
    if (!description.isEmpty())
        undoManager.beginNewTransaction(description);
    undoManager.perform(action);
}