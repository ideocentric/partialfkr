// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"

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

void Project::setPartials(std::vector<std::unique_ptr<Partial>> newPartials)
{
    partials = std::move(newPartials);
    selection.clear();
    notifyPartialsChanged();
}

Partial* Project::findPartialById(uint32_t id) const
{
    for (const auto& p : partials)
        if (p->getId() == id)
            return p.get();
    return nullptr;
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