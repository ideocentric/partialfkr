// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"

#include <unordered_set>

Project::Project() = default;

void Project::loadSourceAudio(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr) {
        juce::Logger::writeToLog("Failed to open audio file: " + file.getFullPathName());
        return;
    }

    sampleRate = reader->sampleRate;
    sourceFile = file;

    const int numChannels = static_cast<int>(reader->numChannels);
    const int numSamples  = static_cast<int>(reader->lengthInSamples);

    juce::AudioBuffer<float> raw(numChannels, numSamples);
    reader->read(&raw, 0, numSamples, 0, true, true);

    // Sum to mono with a warning if stereo
    if (numChannels > 1) {
        juce::Logger::writeToLog("Stereo source detected — summing to mono for analysis.");
        sourceAudio.setSize(1, numSamples);
        sourceAudio.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            sourceAudio.addFrom(0, 0, raw, ch, 0, numSamples, 1.0f / numChannels);
    } else {
        sourceAudio = std::move(raw);
    }
}

static std::unique_ptr<Partial> clonePartialData(const Partial& src)
{
    auto p = std::make_unique<Partial>(src.getId(), src.getColour());
    p->breakpoints = src.breakpoints;
    return p;
}

void Project::setPartials(std::vector<std::unique_ptr<Partial>> newPartials)
{
    partials = std::move(newPartials);
    selection.clear();
    notifyPartialsChanged();
}

void Project::setOriginalPartials(const std::vector<std::unique_ptr<Partial>>& src)
{
    originalPartials.clear();
    originalPartials.reserve(src.size());
    for (const auto& p : src)
        originalPartials.push_back(clonePartialData(*p));
}

Partial* Project::findPartialById(uint32_t id) const
{
    for (const auto& p : partials)
        if (p->getId() == id)
            return p.get();
    return nullptr;
}

uint32_t Project::nextPartialId() const noexcept
{
    uint32_t maxId = 0;
    for (const auto& p : partials)
        maxId = std::max(maxId, p->getId());
    return partials.empty() ? 0 : maxId + 1;
}

void Project::addPartials(std::vector<std::unique_ptr<Partial>> newPartials)
{
    for (auto& p : newPartials)
        partials.push_back(std::move(p));
    notifyPartialsChanged();
}

void Project::replacePartials(std::vector<std::unique_ptr<Partial>> replacements)
{
    std::unordered_map<uint32_t, std::unique_ptr<Partial>> byId;
    for (auto& r : replacements)
        byId[r->getId()] = std::move(r);

    for (auto& p : partials)
    {
        auto it = byId.find(p->getId());
        if (it != byId.end())
            p = std::move(it->second);
    }
    notifyPartialsChanged();
}

void Project::deletePartials(const std::vector<uint32_t>& ids)
{
    std::unordered_set<uint32_t> toDelete(ids.begin(), ids.end());

    auto newEnd = std::remove_if(partials.begin(), partials.end(),
                                 [&](const auto& p) { return toDelete.count(p->getId()); });
    partials.erase(newEnd, partials.end());

    for (auto id : ids)
        selection.deselect(id);

    notifyPartialsChanged();
}

std::vector<uint32_t> Project::applyBreakpointDeletion(
    const std::unordered_map<uint32_t, std::vector<size_t>>& deletions)
{
    std::vector<uint32_t>               toDeleteIds;
    std::vector<std::unique_ptr<Partial>> toAdd;
    std::vector<uint32_t>               newIds;

    uint32_t nextId = nextPartialId();

    for (const auto& [partialId, bpIndices] : deletions)
    {
        auto* p = findPartialById(partialId);
        if (!p) continue;

        std::unordered_set<size_t> toRemove(bpIndices.begin(), bpIndices.end());

        // Split remaining breakpoints into maximal contiguous runs
        std::vector<std::vector<Breakpoint>> runs;
        std::vector<Breakpoint>              cur;
        for (size_t i = 0; i < p->breakpoints.size(); ++i)
        {
            if (toRemove.count(i))
            {
                if (!cur.empty()) { runs.push_back(std::move(cur)); cur.clear(); }
            }
            else
            {
                cur.push_back(p->breakpoints[i]);
            }
        }
        if (!cur.empty()) runs.push_back(std::move(cur));

        toDeleteIds.push_back(partialId);

        for (auto& run : runs)
        {
            if (run.size() < 2) continue;   // too short — discard
            auto np = std::make_unique<Partial>(nextId, p->getColour());
            np->breakpoints = std::move(run);
            newIds.push_back(nextId);
            ++nextId;
            toAdd.push_back(std::move(np));
        }
    }

    // Mutate: delete originals then add replacements
    std::unordered_set<uint32_t> deleteSet(toDeleteIds.begin(), toDeleteIds.end());
    auto newEnd = std::remove_if(partials.begin(), partials.end(),
                                 [&](const auto& p) { return deleteSet.count(p->getId()); });
    partials.erase(newEnd, partials.end());
    for (auto id : toDeleteIds) selection.deselect(id);

    for (auto& np : toAdd)
        partials.push_back(std::move(np));

    notifyPartialsChanged();
    return newIds;
}

void Project::setMuted(uint32_t partialId, bool mute)
{
    if (auto* p = findPartialById(partialId))
        p->muted.store(mute, std::memory_order_relaxed);
    notifyMuteChanged(partialId);
}

void Project::setMutedBatch(const std::vector<uint32_t>& ids, bool mute)
{
    for (auto id : ids)
        if (auto* p = findPartialById(id))
            p->muted.store(mute, std::memory_order_relaxed);

    listeners.call([this](Listener& l) { l.partialsChanged(*this); });
}

void Project::setSoloed(uint32_t partialId, bool solo)
{
    if (auto* p = findPartialById(partialId))
        p->soloed.store(solo, std::memory_order_relaxed);
    notifyMuteChanged(partialId);
}

void Project::clearAllSolo()
{
    for (auto& p : partials)
        p->soloed.store(false, std::memory_order_relaxed);
    listeners.call([this](Listener& l) { l.partialsChanged(*this); });
}

bool Project::hasAnySolo() const noexcept
{
    for (const auto& p : partials)
        if (p->soloed.load(std::memory_order_relaxed))
            return true;
    return false;
}

void Project::notifyPartialsChanged()
{
    listeners.call([this](Listener& l) { l.partialsChanged(*this); });
}

void Project::notifyMuteChanged(uint32_t id)
{
    listeners.call([this, id](Listener& l) { l.muteStateChanged(*this, id); });
}