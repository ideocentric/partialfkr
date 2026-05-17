// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "Model/EditHistory.h"

// Minimal reversible action: increments a counter on perform, decrements on undo.
struct CounterAction : public juce::UndoableAction
{
    int& counter;
    explicit CounterAction(int& c) : counter(c) {}
    bool perform() override { ++counter; return true; }
    bool undo()    override { --counter; return true; }
    int  getSizeInUnits() override { return 1; }
};

TEST_CASE("EditHistory: canUndo false on fresh history", "[EditHistory]")
{
    EditHistory h;
    REQUIRE_FALSE(h.canUndo());
}

TEST_CASE("EditHistory: canRedo false on fresh history", "[EditHistory]")
{
    EditHistory h;
    REQUIRE_FALSE(h.canRedo());
}

TEST_CASE("EditHistory: perform executes action and enables undo", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "increment");
    REQUIRE(counter == 1);
    REQUIRE(h.canUndo());
}

TEST_CASE("EditHistory: undo reverses action and enables redo", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "increment");
    h.undo();
    REQUIRE(counter == 0);
    REQUIRE_FALSE(h.canUndo());
    REQUIRE(h.canRedo());
}

TEST_CASE("EditHistory: redo re-applies action", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "increment");
    h.undo();
    h.redo();
    REQUIRE(counter == 1);
    REQUIRE(h.canUndo());
    REQUIRE_FALSE(h.canRedo());
}

TEST_CASE("EditHistory: new action after undo clears redo stack", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "first");
    h.undo();
    REQUIRE(h.canRedo());
    h.perform(new CounterAction(counter), "second");
    REQUIRE_FALSE(h.canRedo());
    REQUIRE(h.canUndo());
}

TEST_CASE("EditHistory: multiple undos and redos stack correctly", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "a");
    h.perform(new CounterAction(counter), "b");
    h.perform(new CounterAction(counter), "c");
    REQUIRE(counter == 3);

    h.undo(); REQUIRE(counter == 2);
    h.undo(); REQUIRE(counter == 1);
    h.undo(); REQUIRE(counter == 0);
    REQUIRE_FALSE(h.canUndo());

    h.redo(); REQUIRE(counter == 1);
    h.redo(); REQUIRE(counter == 2);
    h.redo(); REQUIRE(counter == 3);
    REQUIRE_FALSE(h.canRedo());
}

TEST_CASE("EditHistory: clearUndoHistory removes all history", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "a");
    h.perform(new CounterAction(counter), "b");
    h.getUndoManager().clearUndoHistory();
    REQUIRE_FALSE(h.canUndo());
    REQUIRE_FALSE(h.canRedo());
}

TEST_CASE("EditHistory: getUndoDescription reflects last performed action", "[EditHistory]")
{
    EditHistory h;
    int counter = 0;
    h.perform(new CounterAction(counter), "my action");
    REQUIRE(h.getUndoDescription().isNotEmpty());
}