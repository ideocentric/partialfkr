// SPDX-License-Identifier: AGPL-3.0-or-later
// ObjC-only translation unit — no JuceHeader to avoid juce::Point vs Carbon Point clash.
#import <AppKit/AppKit.h>

extern "C" void MacWindowHelpers_zoom(void* nativeHandle)
{
    NSView* nsView = (__bridge NSView*) nativeHandle;
    if (nsView == nil) return;
    NSWindow* nsWindow = [nsView window];
    if (nsWindow == nil) return;
    [nsWindow zoom:nil];
}

extern "C" void MacWindowHelpers_bringAllToFront()
{
    [NSApp arrangeInFront:nil];
}