// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PfkrFormat.h"

static constexpr int kFormatVersion = 1;

// ── Helpers ────────────────────────────────────────────────────────────────────

static juce::var bpToVar5(const Breakpoint& bp)
{
    juce::Array<juce::var> a;
    a.add(bp.time);
    a.add((double) bp.frequency);
    a.add((double) bp.amplitude);
    a.add((double) bp.bandwidth);
    a.add((double) bp.phase);
    return juce::var(a);
}

static juce::var bpToVar3(const Breakpoint& bp)
{
    juce::Array<juce::var> a;
    a.add(bp.time);
    a.add((double) bp.frequency);
    a.add((double) bp.amplitude);
    return juce::var(a);
}

// ── Serialize ─────────────────────────────────────────────────────────────────

juce::String PfkrFormat::serialize(const Project& project, const SaveData& data)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", kFormatVersion);
    root->setProperty("sourceAudioPath", project.getSourceFile().getFullPathName());

    // Original Loris analysis — all 5 fields per breakpoint
    {
        juce::Array<juce::var> arr;
        for (const auto& p : project.getOriginalPartials())
        {
            juce::DynamicObject::Ptr pd = new juce::DynamicObject();
            pd->setProperty("id",     (int) p->getId());
            pd->setProperty("colour", p->getColour().toString());

            juce::Array<juce::var> bps;
            for (const auto& bp : p->breakpoints)
                bps.add(bpToVar5(bp));
            pd->setProperty("breakpoints", bps);

            arr.add(juce::var(pd.get()));
        }
        root->setProperty("analysisData", arr);
    }

    // Current edited state — t/f/a only plus editorial fields
    {
        juce::Array<juce::var> arr;
        for (const auto& p : project.getPartials())
        {
            juce::DynamicObject::Ptr pd = new juce::DynamicObject();
            pd->setProperty("id",     (int) p->getId());
            pd->setProperty("muted",  p->muted.load(std::memory_order_relaxed));
            pd->setProperty("solo",   p->soloed.load(std::memory_order_relaxed));
            pd->setProperty("colour", p->getColour().toString());

            juce::Array<juce::var> bps;
            for (const auto& bp : p->breakpoints)
                bps.add(bpToVar3(bp));
            pd->setProperty("breakpoints", bps);

            arr.add(juce::var(pd.get()));
        }
        root->setProperty("partials", arr);
    }

    // Markers
    {
        juce::DynamicObject::Ptr m = new juce::DynamicObject();
        m->setProperty("in",  data.inPoint);
        m->setProperty("out", data.outPoint);
        root->setProperty("markers", juce::var(m.get()));
    }

    // View state
    {
        juce::DynamicObject::Ptr v = new juce::DynamicObject();
        v->setProperty("timeStart",        data.viewTimeStart);
        v->setProperty("timeEnd",          data.viewTimeEnd);
        v->setProperty("freqLow",          (double) data.viewFreqLow);
        v->setProperty("freqHigh",         (double) data.viewFreqHigh);
        v->setProperty("rightPanelVisible", data.rightPanelVisible);
        root->setProperty("view", juce::var(v.get()));
    }

    // Transport state
    {
        juce::DynamicObject::Ptr t = new juce::DynamicObject();
        t->setProperty("loopEnabled", data.loopEnabled);
        root->setProperty("transport", juce::var(t.get()));
    }

    // Window bounds
    {
        juce::DynamicObject::Ptr w = new juce::DynamicObject();
        w->setProperty("x", data.windowBounds.getX());
        w->setProperty("y", data.windowBounds.getY());
        w->setProperty("w", data.windowBounds.getWidth());
        w->setProperty("h", data.windowBounds.getHeight());
        root->setProperty("window", juce::var(w.get()));
    }

    return juce::JSON::toString(juce::var(root.get()), false);
}

// ── Deserialize ───────────────────────────────────────────────────────────────

static bool getObj(const juce::var& v, juce::DynamicObject*& out, const char* field = nullptr)
{
    const juce::var& src = field ? v[field] : v;
    out = src.getDynamicObject();
    return out != nullptr;
}

PfkrFormat::LoadResult PfkrFormat::deserialize(const juce::String& json, Project& project)
{
    LoadResult result;

    const juce::var root = juce::JSON::parse(json);
    juce::DynamicObject* rootObj = nullptr;
    if (!getObj(root, rootObj))
    {
        result.errorMessage = "Invalid JSON or empty file.";
        return result;
    }

    if ((int) rootObj->getProperty("version") != kFormatVersion)
    {
        result.errorMessage = "Unsupported .pfkr version.";
        return result;
    }

    // Source audio path (path only — no audio buffer loaded)
    project.setSourceFilePath(
        juce::File(rootObj->getProperty("sourceAudioPath").toString()));

    // Original analysis data
    {
        std::vector<std::unique_ptr<Partial>> originals;
        if (auto* arr = rootObj->getProperty("analysisData").getArray())
        {
            for (const auto& pVar : *arr)
            {
                auto* pObj = pVar.getDynamicObject();
                if (pObj == nullptr) continue;

                const uint32_t    id     = (uint32_t)(int) pObj->getProperty("id");
                const juce::Colour col   = juce::Colour::fromString(
                                               pObj->getProperty("colour").toString());
                auto partial = std::make_unique<Partial>(id, col);

                if (auto* bpArr = pObj->getProperty("breakpoints").getArray())
                {
                    for (const auto& bpVar : *bpArr)
                    {
                        auto* a = bpVar.getArray();
                        if (a == nullptr || a->size() < 5) continue;
                        Breakpoint bp;
                        bp.time      = (double) (*a)[0];
                        bp.frequency = (float)  (double) (*a)[1];
                        bp.amplitude = (float)  (double) (*a)[2];
                        bp.bandwidth = (float)  (double) (*a)[3];
                        bp.phase     = (float)  (double) (*a)[4];
                        partial->breakpoints.push_back(bp);
                    }
                }
                originals.push_back(std::move(partial));
            }
        }
        // Pass directly — setOriginalPartials would deep copy; use the move overload
        // by setting via the member directly through a dedicated setter.
        // Use setOriginalPartials with const ref (it deep-copies), but originals are
        // freshly constructed here so we can move them if we add a move overload.
        // For now, clear and assign directly to avoid redundant copies.
        // We call the existing const-ref overload (it's safe since we own originals).
        project.setOriginalPartials(originals);
    }

    // Current edited state
    {
        std::vector<std::unique_ptr<Partial>> partials;
        if (auto* arr = rootObj->getProperty("partials").getArray())
        {
            for (const auto& pVar : *arr)
            {
                auto* pObj = pVar.getDynamicObject();
                if (pObj == nullptr) continue;

                const uint32_t     id     = (uint32_t)(int) pObj->getProperty("id");
                const juce::Colour col    = juce::Colour::fromString(
                                                pObj->getProperty("colour").toString());
                auto partial = std::make_unique<Partial>(id, col);
                partial->muted.store ((bool) pObj->getProperty("muted"),
                                      std::memory_order_relaxed);
                partial->soloed.store((bool) pObj->getProperty("solo"),
                                      std::memory_order_relaxed);

                if (auto* bpArr = pObj->getProperty("breakpoints").getArray())
                {
                    for (const auto& bpVar : *bpArr)
                    {
                        auto* a = bpVar.getArray();
                        if (a == nullptr || a->size() < 3) continue;
                        Breakpoint bp;
                        bp.time      = (double) (*a)[0];
                        bp.frequency = (float)  (double) (*a)[1];
                        bp.amplitude = (float)  (double) (*a)[2];
                        bp.bandwidth = 0.0f;
                        bp.phase     = 0.0f;
                        partial->breakpoints.push_back(bp);
                    }
                }
                partials.push_back(std::move(partial));
            }
        }
        project.setPartials(std::move(partials));
    }

    // Markers
    if (auto* m = rootObj->getProperty("markers").getDynamicObject())
    {
        result.data.inPoint  = (double) m->getProperty("in");
        result.data.outPoint = (double) m->getProperty("out");
    }

    // View state
    if (auto* v = rootObj->getProperty("view").getDynamicObject())
    {
        result.data.viewTimeStart     = (double) v->getProperty("timeStart");
        result.data.viewTimeEnd       = (double) v->getProperty("timeEnd");
        result.data.viewFreqLow       = (float)  (double) v->getProperty("freqLow");
        result.data.viewFreqHigh      = (float)  (double) v->getProperty("freqHigh");

        // Graceful default for files saved before this field existed: panel shown
        const juce::var panelVar = v->getProperty("rightPanelVisible");
        result.data.rightPanelVisible = panelVar.isVoid() ? true : (bool) panelVar;
    }

    // Transport state (section absent in files saved before this field existed)
    if (auto* t = rootObj->getProperty("transport").getDynamicObject())
    {
        const juce::var loopVar   = t->getProperty("loopEnabled");
        result.data.loopEnabled   = loopVar.isVoid() ? false : (bool) loopVar;
    }

    // Window bounds
    if (auto* w = rootObj->getProperty("window").getDynamicObject())
    {
        result.data.windowBounds = {
            (int) w->getProperty("x"), (int) w->getProperty("y"),
            (int) w->getProperty("w"), (int) w->getProperty("h")
        };
    }

    result.success = true;
    return result;
}