// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * Sets the macOS proxy icon in the native title bar — the small file icon that
 * appears next to the window title, supporting Finder reveal and drag-to-dock.
 * Pass File() (default-constructed) to clear the proxy icon.
 */
namespace MacProxyIcon
{
    void set(juce::Component* topLevelWindow, const juce::File& file);
}