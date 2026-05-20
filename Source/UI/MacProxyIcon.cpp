// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MacProxyIcon.h"

// Implemented in MacProxyIcon.mm to keep ObjC out of this translation unit.
// Declared at file scope: extern "C" is not valid inside a function body.
#if JUCE_MAC
extern "C" void MacProxyIcon_setNative(void* nsWindowHandle, const char* utf8Path);
#endif

void MacProxyIcon::set(juce::Component* topLevelWindow, const juce::File& file)
{
#if JUCE_MAC
    if (topLevelWindow == nullptr)
        return;

    auto* peer = topLevelWindow->getPeer();
    if (peer == nullptr)
        return;

    const char* path = (file == juce::File()) ? nullptr
                                               : file.getFullPathName().toRawUTF8();
    MacProxyIcon_setNative(peer->getNativeHandle(), path);
#else
    juce::ignoreUnused(topLevelWindow, file);
#endif
}