// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MacWindowHelpers.h"

#if JUCE_MAC
extern "C" void MacWindowHelpers_zoom(void* nativeHandle);
extern "C" void MacWindowHelpers_bringAllToFront();
#endif

void MacWindowHelpers::zoom(juce::Component* topLevelWindow)
{
#if JUCE_MAC
    if (topLevelWindow != nullptr)
        if (auto* peer = topLevelWindow->getPeer())
            MacWindowHelpers_zoom(peer->getNativeHandle());
#else
    juce::ignoreUnused(topLevelWindow);
#endif
}

void MacWindowHelpers::bringAllToFront()
{
#if JUCE_MAC
    MacWindowHelpers_bringAllToFront();
#endif
}