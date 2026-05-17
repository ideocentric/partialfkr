// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Model/PfkrFormat.h"
#include "Model/Project.h"

// ── Helpers ────────────────────────────────────────────────────────────────────

static std::unique_ptr<Partial> makePartial(uint32_t id,
                                            juce::Colour colour,
                                            std::vector<std::tuple<double,float,float>> bps)
{
    auto p = std::make_unique<Partial>(id, colour);
    for (auto& [t, f, a] : bps)
        p->breakpoints.push_back({t, f, a, 0.0f, 0.0f});
    return p;
}

static std::vector<std::unique_ptr<Partial>> singlePartial()
{
    std::vector<std::unique_ptr<Partial>> v;
    v.push_back(makePartial(1, juce::Colour(0xff00cc44u),
                            {{0.0, 440.0f, 0.5f}, {1.0, 441.0f, 0.3f}}));
    return v;
}

// ── Round-trip correctness ─────────────────────────────────────────────────────

TEST_CASE("PfkrFormat: empty project round-trips", "[PfkrFormat]")
{
    Project src, dst;
    PfkrFormat::SaveData data;
    data.inPoint  = 0.0;
    data.outPoint = 5.0;

    const juce::String json = PfkrFormat::serialize(src, data);
    REQUIRE(json.isNotEmpty());

    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getPartials().empty());
    REQUIRE(dst.getOriginalPartials().empty());
    REQUIRE(result.data.inPoint  == Catch::Approx(0.0));
    REQUIRE(result.data.outPoint == Catch::Approx(5.0));
}

TEST_CASE("PfkrFormat: single partial round-trips with all fields", "[PfkrFormat]")
{
    Project src, dst;
    auto partials = singlePartial();
    partials[0]->muted.store(true, std::memory_order_relaxed);
    src.setOriginalPartials(partials);
    src.setPartials(std::move(partials));
    src.setSourceFilePath(juce::File("/tmp/test.aif"));

    PfkrFormat::SaveData data;
    data.inPoint       = 0.25;
    data.outPoint      = 0.75;
    data.viewTimeStart = 0.1;
    data.viewTimeEnd   = 2.5;
    data.viewFreqLow   = 100.0f;
    data.viewFreqHigh  = 4000.0f;
    data.windowBounds  = {50, 60, 1200, 800};

    const juce::String json = PfkrFormat::serialize(src, data);
    auto result = PfkrFormat::deserialize(json, dst);

    REQUIRE(result.success);
    REQUIRE(dst.getPartials().size()         == 1);
    REQUIRE(dst.getOriginalPartials().size() == 1);

    const Partial& p = *dst.getPartials()[0];
    REQUIRE(p.getId()    == 1);
    REQUIRE(p.muted.load(std::memory_order_relaxed));
    REQUIRE(p.breakpoints.size() == 2);
    REQUIRE(p.breakpoints[0].time      == Catch::Approx(0.0));
    REQUIRE(p.breakpoints[0].frequency == Catch::Approx(440.0f));
    REQUIRE(p.breakpoints[0].amplitude == Catch::Approx(0.5f));
    REQUIRE(p.breakpoints[1].time      == Catch::Approx(1.0));

    REQUIRE(result.data.inPoint       == Catch::Approx(0.25));
    REQUIRE(result.data.outPoint      == Catch::Approx(0.75));
    REQUIRE(result.data.viewTimeStart == Catch::Approx(0.1));
    REQUIRE(result.data.viewTimeEnd   == Catch::Approx(2.5));
    REQUIRE(result.data.viewFreqLow   == Catch::Approx(100.0f));
    REQUIRE(result.data.viewFreqHigh  == Catch::Approx(4000.0f));
    REQUIRE(result.data.windowBounds  == juce::Rectangle<int>(50, 60, 1200, 800));
}

TEST_CASE("PfkrFormat: original analysis data preserves all 5 Loris fields", "[PfkrFormat]")
{
    Project src, dst;
    auto p = std::make_unique<Partial>(1, juce::Colour(0xff00cc44u));
    p->breakpoints.push_back({0.5, 880.0f, 0.7f, 1.57f, 0.3f});  // {time, freq, amp, phase, bw}
    std::vector<std::unique_ptr<Partial>> v;
    v.push_back(std::move(p));

    src.setOriginalPartials(v);
    src.setPartials(std::move(v));

    const juce::String json = PfkrFormat::serialize(src, {});
    auto result = PfkrFormat::deserialize(json, dst);

    REQUIRE(result.success);
    REQUIRE(dst.getOriginalPartials().size() == 1);

    const Breakpoint& bp = dst.getOriginalPartials()[0]->breakpoints[0];
    REQUIRE(bp.time      == Catch::Approx(0.5));
    REQUIRE(bp.frequency == Catch::Approx(880.0f));
    REQUIRE(bp.amplitude == Catch::Approx(0.7f));
    REQUIRE(bp.bandwidth == Catch::Approx(0.3f));
    REQUIRE(bp.phase     == Catch::Approx(1.57f));
}

TEST_CASE("PfkrFormat: colour survives round-trip", "[PfkrFormat]")
{
    Project src, dst;
    auto partials = singlePartial();  // colour = 0xff00cc44
    src.setOriginalPartials(partials);
    src.setPartials(std::move(partials));

    const juce::String json = PfkrFormat::serialize(src, {});
    auto result = PfkrFormat::deserialize(json, dst);

    REQUIRE(result.success);
    REQUIRE(dst.getPartials()[0]->getColour() == juce::Colour(0xff00cc44u));
}

TEST_CASE("PfkrFormat: solo flag round-trips", "[PfkrFormat]")
{
    Project src, dst;
    auto partials = singlePartial();
    partials[0]->soloed.store(true, std::memory_order_relaxed);
    src.setOriginalPartials(partials);
    src.setPartials(std::move(partials));

    const juce::String json = PfkrFormat::serialize(src, {});
    auto result = PfkrFormat::deserialize(json, dst);

    REQUIRE(result.success);
    REQUIRE(dst.getPartials()[0]->soloed.load(std::memory_order_relaxed));
}

TEST_CASE("PfkrFormat: multiple partials round-trip preserving order and IDs", "[PfkrFormat]")
{
    Project src, dst;
    std::vector<std::unique_ptr<Partial>> partials;
    for (uint32_t id = 1; id <= 5; ++id)
        partials.push_back(makePartial(id, juce::Colour(id * 0x11111111u),
                                       {{0.0, float(id) * 100.0f, 0.5f},
                                        {1.0, float(id) * 100.0f, 0.2f}}));

    src.setOriginalPartials(partials);
    src.setPartials(std::move(partials));

    const juce::String json = PfkrFormat::serialize(src, {});
    auto result = PfkrFormat::deserialize(json, dst);

    REQUIRE(result.success);
    REQUIRE(dst.getPartials().size() == 5);
    for (size_t i = 0; i < 5; ++i)
        REQUIRE(dst.getPartials()[i]->getId() == (uint32_t)(i + 1));
}

// ── Corruption / robustness ────────────────────────────────────────────────────

TEST_CASE("PfkrFormat: empty string returns failure", "[PfkrFormat]")
{
    Project dst;
    auto result = PfkrFormat::deserialize("", dst);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.errorMessage.isNotEmpty());
}

TEST_CASE("PfkrFormat: invalid JSON returns failure", "[PfkrFormat]")
{
    Project dst;
    auto result = PfkrFormat::deserialize("{ not valid json !!", dst);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.errorMessage.isNotEmpty());
}

TEST_CASE("PfkrFormat: wrong version returns failure", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({"version":99,"sourceAudioPath":"","analysisData":[],"partials":[]})";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.errorMessage.isNotEmpty());
}

TEST_CASE("PfkrFormat: missing partials array yields empty result without crash", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({"version":1,"sourceAudioPath":"","analysisData":[]})";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getPartials().empty());
}

TEST_CASE("PfkrFormat: missing analysisData array yields empty originals without crash", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({"version":1,"sourceAudioPath":"","partials":[]})";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getOriginalPartials().empty());
}

TEST_CASE("PfkrFormat: truncated current-state breakpoint (< 3 elements) is silently skipped", "[PfkrFormat]")
{
    Project dst;
    // One valid breakpoint [0.0, 440.0, 0.5] and one truncated [1.0, 880.0]
    const juce::String json = R"({
        "version":1,"sourceAudioPath":"","analysisData":[],
        "partials":[{"id":1,"muted":false,"solo":false,"colour":"ff00cc44",
                     "breakpoints":[[0.0,440.0,0.5],[1.0,880.0]]}]
    })";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getPartials().size() == 1);
    REQUIRE(dst.getPartials()[0]->breakpoints.size() == 1);  // truncated row skipped
    REQUIRE(dst.getPartials()[0]->breakpoints[0].frequency == Catch::Approx(440.0f));
}

TEST_CASE("PfkrFormat: truncated analysis breakpoint (< 5 elements) is silently skipped", "[PfkrFormat]")
{
    Project dst;
    // One valid [t,f,a,bw,phase] and one truncated [t,f,a,bw]
    const juce::String json = R"({
        "version":1,"sourceAudioPath":"","partials":[],
        "analysisData":[{"id":1,"colour":"ff00cc44",
                         "breakpoints":[[0.0,440.0,0.5,0.0,0.0],[1.0,880.0,0.3,0.1]]}]
    })";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getOriginalPartials().size() == 1);
    REQUIRE(dst.getOriginalPartials()[0]->breakpoints.size() == 1);
}

TEST_CASE("PfkrFormat: partial entry that is not an object is skipped", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({
        "version":1,"sourceAudioPath":"","analysisData":[],
        "partials":[42, {"id":7,"muted":false,"solo":false,"colour":"ff00cc44","breakpoints":[[0.0,440.0,0.5],[1.0,440.0,0.4]]}]
    })";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getPartials().size() == 1);
    REQUIRE(dst.getPartials()[0]->getId() == 7);
}

TEST_CASE("PfkrFormat: missing markers section uses caller-provided defaults", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({"version":1,"sourceAudioPath":"","analysisData":[],"partials":[]})";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    // No markers section — result.data retains default-constructed values (0.0 / -1.0)
    REQUIRE(result.data.inPoint  == Catch::Approx(0.0));
    REQUIRE(result.data.outPoint == Catch::Approx(-1.0));
}

TEST_CASE("PfkrFormat: non-array partials value is treated as empty", "[PfkrFormat]")
{
    Project dst;
    const juce::String json = R"({"version":1,"sourceAudioPath":"","analysisData":[],"partials":"corrupt"})";
    auto result = PfkrFormat::deserialize(json, dst);
    REQUIRE(result.success);
    REQUIRE(dst.getPartials().empty());
}