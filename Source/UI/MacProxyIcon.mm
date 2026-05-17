// SPDX-License-Identifier: AGPL-3.0-or-later
// ObjC-only translation unit — no JuceHeader to avoid juce::Point vs Carbon Point clash.
#import <AppKit/AppKit.h>

extern "C" void MacProxyIcon_setNative(void* nativeHandle, const char* utf8Path)
{
    NSView* nsView = (__bridge NSView*) nativeHandle;
    if (nsView == nil)
        return;

    NSWindow* nsWindow = [nsView window];
    if (nsWindow == nil)
        return;

    if (utf8Path == nullptr || utf8Path[0] == '\0')
        [nsWindow setRepresentedURL: nil];
    else
        [nsWindow setRepresentedURL: [NSURL fileURLWithPath: [NSString stringWithUTF8String: utf8Path]]];
}