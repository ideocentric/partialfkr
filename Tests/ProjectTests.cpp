// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Model/Project.h"

static std::unique_ptr<Partial> makePartial(uint32_t id)
{
    auto p = std::make_unique<Partial>(id, juce::Colour(0xffffffff));
    p->breakpoints.push_back({0.0, 440.0f, 0.5f, 0.0f, 0.0f});
    p->breakpoints.push_back({1.0, 440.0f, 0.3f, 0.0f, 0.0f});
    return p;
}

// ── hasSourceAudio ─────────────────────────────────────────────────────────────

TEST_CASE("Project::hasSourceAudio false on fresh project", "[Project]")
{
    Project p;
    REQUIRE_FALSE(p.hasSourceAudio());
}

TEST_CASE("Project::hasSourceAudio true after setSourceFilePath", "[Project]")
{
    Project p;
    p.setSourceFilePath(juce::File("/tmp/test.aif"));
    REQUIRE(p.hasSourceAudio());
}

TEST_CASE("Project::hasSourceAudio true after non-empty setPartials", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> partials;
    partials.push_back(makePartial(1));
    p.setPartials(std::move(partials));
    REQUIRE(p.hasSourceAudio());
}

TEST_CASE("Project::hasSourceAudio false after setPartials with empty vector", "[Project]")
{
    Project p;
    p.setPartials({});
    REQUIRE_FALSE(p.hasSourceAudio());
}

// ── originalPartials ──────────────────────────────────────────────────────────

TEST_CASE("Project::setOriginalPartials stores correct count", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> src;
    src.push_back(makePartial(1));
    src.push_back(makePartial(2));
    src.push_back(makePartial(3));

    p.setOriginalPartials(src);
    REQUIRE(p.getOriginalPartials().size() == 3);
}

TEST_CASE("Project::setOriginalPartials deep-copies — modifying source does not affect stored copy", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> src;
    src.push_back(makePartial(42));
    p.setOriginalPartials(src);

    // Mutate the source after storing
    src[0]->breakpoints.clear();

    // Stored copy is unaffected
    REQUIRE(p.getOriginalPartials()[0]->breakpoints.size() == 2);
}

TEST_CASE("Project::setOriginalPartials preserves IDs and breakpoint data", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> src;
    auto part = std::make_unique<Partial>(99u, juce::Colour(0xff112233u));
    part->breakpoints.push_back({0.5, 880.0f, 0.8f, 1.2f, 0.1f});  // {time, freq, amp, phase, bw}
    src.push_back(std::move(part));

    p.setOriginalPartials(src);
    REQUIRE(p.getOriginalPartials()[0]->getId() == 99u);

    const Breakpoint& bp = p.getOriginalPartials()[0]->breakpoints[0];
    REQUIRE(bp.time      == Catch::Approx(0.5));
    REQUIRE(bp.frequency == Catch::Approx(880.0f));
    REQUIRE(bp.amplitude == Catch::Approx(0.8f));
    REQUIRE(bp.bandwidth == Catch::Approx(0.1f));
    REQUIRE(bp.phase     == Catch::Approx(1.2f));
}

TEST_CASE("Project::setOriginalPartials originals are always unmuted regardless of source", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> src;
    auto part = makePartial(1);
    part->muted.store(true, std::memory_order_relaxed);
    src.push_back(std::move(part));

    p.setOriginalPartials(src);
    // clonePartialData intentionally does not copy mute/solo
    REQUIRE_FALSE(p.getOriginalPartials()[0]->muted.load(std::memory_order_relaxed));
}

TEST_CASE("Project::setOriginalPartials replaces previous snapshot", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> first, second;
    first.push_back(makePartial(1));
    first.push_back(makePartial(2));
    p.setOriginalPartials(first);
    REQUIRE(p.getOriginalPartials().size() == 2);

    second.push_back(makePartial(10));
    p.setOriginalPartials(second);
    REQUIRE(p.getOriginalPartials().size() == 1);
    REQUIRE(p.getOriginalPartials()[0]->getId() == 10u);
}

// ── findPartialById ───────────────────────────────────────────────────────────

TEST_CASE("Project::findPartialById returns correct partial", "[Project]")
{
    Project p;
    std::vector<std::unique_ptr<Partial>> partials;
    partials.push_back(makePartial(5));
    partials.push_back(makePartial(10));
    p.setPartials(std::move(partials));

    REQUIRE(p.findPartialById(5)  != nullptr);
    REQUIRE(p.findPartialById(10) != nullptr);
    REQUIRE(p.findPartialById(99) == nullptr);
    REQUIRE(p.findPartialById(5)->getId()  == 5u);
    REQUIRE(p.findPartialById(10)->getId() == 10u);
}