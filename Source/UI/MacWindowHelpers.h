// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <JuceHeader.h>

/**
 * macOS-specific window management helpers.
 * On non-macOS platforms all calls are no-ops.
 */
namespace MacWindowHelpers
{
    /** Sends a zoom (maximize/restore) message to the given top-level window. */
    void zoom(juce::Component* topLevelWindow);

    /** Brings all application windows to the front (macOS "Arrange in Front"). */
    void bringAllToFront();
}