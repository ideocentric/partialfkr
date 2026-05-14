// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Analysis/PartialData.h"

// Helper to build a Partial with explicit breakpoints
static std::unique_ptr<Partial> makePartial(uint32_t id,
                                            std::vector<std::tuple<double,float,float>> bps)
{
    auto p = std::make_unique<Partial>(id, juce::Colour(0xffffff00u));
    for (auto& [t, freq, amp] : bps)
        p->breakpoints.push_back({t, freq, amp, 0.0f, 0.0f});
    return p;
}

TEST_CASE("Partial time queries", "[PartialData]")
{
    auto p = makePartial(0, {{0.0, 440.0f, 0.5f}, {1.0, 550.0f, 0.3f}});

    REQUIRE(p->startTime() == Catch::Approx(0.0));
    REQUIRE(p->endTime()   == Catch::Approx(1.0));
    REQUIRE(p->duration()  == Catch::Approx(1.0));

    REQUIRE(p->containsTime(0.0));
    REQUIRE(p->containsTime(0.5));
    REQUIRE(p->containsTime(1.0));
    REQUIRE_FALSE(p->containsTime(-0.001));
    REQUIRE_FALSE(p->containsTime(1.001));
}

TEST_CASE("Partial peak amplitude", "[PartialData]")
{
    auto p = makePartial(1, {{0.0, 440.0f, 0.2f}, {0.5, 440.0f, 0.9f}, {1.0, 440.0f, 0.1f}});
    REQUIRE(p->peakAmplitude() == Catch::Approx(0.9f));
}

TEST_CASE("Partial total energy", "[PartialData]")
{
    // Constant amplitude 0.5 over 1 second → energy = 0.5
    auto p = makePartial(2, {{0.0, 440.0f, 0.5f}, {1.0, 440.0f, 0.5f}});
    REQUIRE(p->totalEnergy() == Catch::Approx(0.5f));
}

TEST_CASE("Partial median frequency", "[PartialData]")
{
    auto p = makePartial(3, {
        {0.0, 100.0f, 1.0f},
        {0.5, 200.0f, 1.0f},
        {1.0, 300.0f, 1.0f},
    });
    REQUIRE(p->medianFrequency() == Catch::Approx(200.0f));
}

TEST_CASE("Partial mute flag is atomic", "[PartialData]")
{
    auto p = makePartial(4, {{0.0, 440.0f, 1.0f}, {1.0, 440.0f, 0.0f}});
    REQUIRE_FALSE(p->muted.load());

    p->muted.store(true);
    REQUIRE(p->muted.load());

    p->muted.store(false);
    REQUIRE_FALSE(p->muted.load());
}

TEST_CASE("Breakpoint stores all fields", "[PartialData]")
{
    Breakpoint bp{0.01, 261.63f, 0.75f, 1.57f, 0.02f};
    REQUIRE(bp.time      == Catch::Approx(0.01));
    REQUIRE(bp.frequency == Catch::Approx(261.63f));
    REQUIRE(bp.amplitude == Catch::Approx(0.75f));
    REQUIRE(bp.phase     == Catch::Approx(1.57f));
    REQUIRE(bp.bandwidth == Catch::Approx(0.02f));
}