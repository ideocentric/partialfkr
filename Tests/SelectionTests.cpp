// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "Model/Selection.h"

TEST_CASE("Selection: fresh selection is empty", "[Selection]")
{
    Selection s;
    REQUIRE(s.isEmpty());
    REQUIRE_FALSE(s.isSelected(1));
}

TEST_CASE("Selection: select makes isSelected true", "[Selection]")
{
    Selection s;
    s.select(42);
    REQUIRE(s.isSelected(42));
    REQUIRE_FALSE(s.isEmpty());
}

TEST_CASE("Selection: deselect removes entry", "[Selection]")
{
    Selection s;
    s.select(1);
    s.select(2);
    s.deselect(1);
    REQUIRE_FALSE(s.isSelected(1));
    REQUIRE(s.isSelected(2));
}

TEST_CASE("Selection: clear empties all entries", "[Selection]")
{
    Selection s;
    s.select(1);
    s.select(2);
    s.select(3);
    s.clear();
    REQUIRE(s.isEmpty());
    REQUIRE_FALSE(s.isSelected(1));
    REQUIRE_FALSE(s.isSelected(2));
}

TEST_CASE("Selection: selecting same ID twice has no effect", "[Selection]")
{
    Selection s;
    s.select(7);
    s.select(7);
    REQUIRE(s.isSelected(7));
    s.deselect(7);
    REQUIRE_FALSE(s.isSelected(7));
}

TEST_CASE("Selection: deselecting non-selected ID is a no-op", "[Selection]")
{
    Selection s;
    s.select(1);
    s.deselect(99);  // not selected — should not throw or corrupt state
    REQUIRE(s.isSelected(1));
}

TEST_CASE("Selection: isSelected false for unknown ID", "[Selection]")
{
    Selection s;
    s.select(1);
    REQUIRE_FALSE(s.isSelected(2));
    REQUIRE_FALSE(s.isSelected(0));
}

TEST_CASE("Selection: multiple IDs tracked independently", "[Selection]")
{
    Selection s;
    for (uint32_t id = 1; id <= 10; ++id)
        s.select(id);

    for (uint32_t id = 1; id <= 10; ++id)
        REQUIRE(s.isSelected(id));

    s.deselect(5);
    REQUIRE_FALSE(s.isSelected(5));
    REQUIRE(s.isSelected(4));
    REQUIRE(s.isSelected(6));
}