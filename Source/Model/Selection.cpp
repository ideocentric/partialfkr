// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Selection.h"

void Selection::select(uint32_t id)
{
    selected.insert(id);
    notify();
}

void Selection::deselect(uint32_t id)
{
    selected.erase(id);
    notify();
}

void Selection::toggle(uint32_t id)
{
    if (selected.count(id))
        selected.erase(id);
    else
        selected.insert(id);
    notify();
}

void Selection::selectAll(const std::vector<uint32_t>& ids)
{
    for (auto id : ids)
        selected.insert(id);
    notify();
}

void Selection::invert(const std::vector<uint32_t>& allIds)
{
    std::unordered_set<uint32_t> newSelected;
    for (auto id : allIds)
        if (!selected.count(id))
            newSelected.insert(id);
    selected = std::move(newSelected);
    notify();
}

void Selection::clear()
{
    if (selected.empty())
        return;
    selected.clear();
    notify();
}

bool Selection::isSelected(uint32_t id) const noexcept
{
    return selected.count(id) > 0;
}

std::vector<uint32_t> Selection::getSelectedIds() const
{
    return {selected.begin(), selected.end()};
}

void Selection::notify()
{
    listeners.call([this](Listener& l) { l.selectionChanged(*this); });
}