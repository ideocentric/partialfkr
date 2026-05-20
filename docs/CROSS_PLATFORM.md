# PartialFKR — Cross-Platform Porting Plan

Covers Windows and Linux. macOS remains the primary platform; all changes
must not regress existing macOS behavior. Work is ordered by dependency:
build system first (it blocks everything else), then code isolation, then
per-OS UX refinement.

> **Status:** Planning. No changes implemented yet.  
> **Audit base:** `master` branch, May 2026.

---

## 1. Executive Summary

The application is structurally portable — JUCE abstracts audio I/O, file
dialogs, and most UI. The blocking issues are narrow:

| # | Issue | Severity | Effort |
|---|-------|----------|--------|
| 1 | `.mm` files unconditionally compiled on all platforms | **BLOCKER** | 15 min |
| 2 | `MacProxyIcon.cpp` extern "C" forward decl always compiled | **BLOCKER** | 15 min |
| 3 | ~~`Ctrl+S` (set in/out from selection) conflicts with Save on Win/Linux~~ | ~~HIGH~~ | **Fixed** |
| 4 | Window menu has macOS-only items on all platforms | MEDIUM | 1 hr |
| 5 | `setMacMainMenu` on Win/Linux: menu lives inside window, not system bar | LOW | none (JUCE handles) |
| 6 | Audio device selection UI not tested on Win/Linux | MEDIUM | 1–2 hrs |
| 7 | Packaging: no installer/package for Win/Linux | LOW | per-platform work |

Issues 1 and 2 are the only compile-time blockers. Everything else is
behavior/UX polish.

---

## 2. Build System

**File:** `CMakeLists.txt`

### 2a. Conditional `.mm` compilation (BLOCKER)

Objective-C++ files (`.mm`) are currently listed unconditionally in
`target_sources`. MSVC and GCC will reject them immediately.

**Current code (lines 148, 150):**
```cmake
Source/UI/MacProxyIcon.mm
Source/UI/MacWindowHelpers.mm
```
listed flat inside `target_sources(PartialFKR PRIVATE ...)`.

**Required change:** Move `.mm` files into an `if(APPLE)` block.

```cmake
# In the target_sources block, replace the two .mm lines with nothing,
# then add after target_sources():
if(APPLE)
    target_sources(PartialFKR PRIVATE
        Source/UI/MacProxyIcon.mm
        Source/UI/MacWindowHelpers.mm
    )
endif()
```

### 2b. Windows-specific CMake entries (future)

When a Windows build target is created:
- `set_target_properties(PartialFKR PROPERTIES WIN32_EXECUTABLE TRUE)` — suppresses the console window for the GUI app. JUCE's `juce_add_gui_app` may already set this; verify.
- No additional link libraries needed — JUCE handles `winmm`, `ole32`, WASAPI, DirectSound internally.

### 2c. Linux-specific CMake entries (future)

When a Linux build target is created:
- `find_package(PkgConfig)` + `pkg_check_modules(ALSA alsa)` if JUCE's
  internal detection is insufficient (usually it is sufficient).
- May need `pkg_check_modules(FREETYPE freetype2)` and `pkg_check_modules(GTK3 gtk+-3.0)` — JUCE links these automatically when found via `juce_recommended_config_flags`.

### 2d. Icon files

The icon selection block already handles all three platforms correctly
(lines 53–59 in CMakeLists.txt). No change needed.

---

## 3. macOS Code Isolation

### 3a. `MacProxyIcon.cpp` — extern "C" forward declaration (BLOCKER)

**File:** `Source/UI/MacProxyIcon.cpp`

The `extern "C" void MacProxyIcon_setNative(...)` declaration at line 5 is
always compiled. On non-macOS the symbol is never defined, causing a linker
error.

**Required change:** Wrap the entire implementation body in `#if JUCE_MAC`.

```cpp
// MacProxyIcon.cpp
#include "MacProxyIcon.h"

void MacProxyIcon::set(juce::Component* topLevelWindow, const juce::File& file)
{
#if JUCE_MAC
    extern "C" void MacProxyIcon_setNative(void* nsWindowHandle, const char* utf8Path);

    if (topLevelWindow == nullptr) return;
    auto* peer = topLevelWindow->getPeer();
    if (peer == nullptr) return;

    const char* path = (file == juce::File()) ? nullptr
                                              : file.getFullPathName().toRawUTF8();
    MacProxyIcon_setNative(peer->getNativeHandle(), path);
#else
    juce::ignoreUnused(topLevelWindow, file);
#endif
}
```

### 3b. `MacWindowHelpers.cpp` — already guarded

`MacWindowHelpers.cpp` already wraps both functions in `#if JUCE_MAC` /
`#else juce::ignoreUnused(...)`. **No change needed here.**

### 3c. Proxy icon call sites in `MainComponent.cpp`

After the `.cpp` fix above, all three call sites (`MacProxyIcon::set(...)`)
will compile and link on all platforms — they simply become no-ops. No change
needed at the call sites; the abstraction is already clean.

### 3d. macOS-only menu items — `Window` menu

**File:** `Source/Main.cpp` lines 308–315

"Bring All to Front" is a macOS window-management concept. On Windows/Linux
it is both meaningless (single-window app using native decorations) and
already a no-op in `MacWindowHelpers::bringAllToFront()`. The menu item
should be hidden on non-macOS so users don't see a grayed-out command.

```cpp
if (index == 4)  // Window
{
    juce::PopupMenu window;
    window.addCommandItem(&cm, CommandIDs::windowMinimize);
    window.addCommandItem(&cm, CommandIDs::windowZoom);
#if JUCE_MAC
    window.addSeparator();
    window.addCommandItem(&cm, CommandIDs::windowBringAllToFront);
#endif
    return window;
}
```

---

## 4. Menu Bar

### macOS

System-level menu bar via `MenuBarModel::setMacMainMenu()`. Already
implemented and conditionally compiled. **No change needed.**

- "About PartialFKR" lives in the Apple menu (line 34 in `Main.cpp`).

### Windows and Linux

`MenuBarModel::setMacMainMenu(&appMenu)` on non-macOS tells JUCE to draw
the menu bar inside each `DocumentWindow` — this is already in the `#else`
branch at line 38. JUCE handles the rendering automatically with the native
look-and-feel on both platforms.

- "About PartialFKR" correctly falls through to the Help menu via the
  `#if !JUCE_MAC` block at lines 301–304. **No change needed.**

**Windows note:** Windows convention puts the menu bar flush with the top of
the window's client area, not inside the title bar. JUCE's `DocumentWindow`
with `setUsingNativeTitleBar(true)` (already set at line 368) achieves this
correctly on Windows.

**Linux note:** On Linux with native title bars, the menu bar behavior
depends on the desktop environment. GNOME's global menu bar (via
libdbusmenu/AppMenu) is not currently wired; the menu will render inside the
window, which is correct for a first Linux release.

---

## 5. Keyboard Shortcuts

All shortcuts use `ModifierKeys::commandModifier` — which JUCE maps to
**Cmd** on macOS and **Ctrl** on Windows/Linux. This is correct convention
and requires no changes for the standard set.

### 5a. ~~Conflict: `Ctrl+S` for set in/out from selection~~ — Fixed

The in/out marker shortcuts have been fully reworked and promoted to the
command manager system so they appear in the Transport menu with key hints:

| Old | New | Action |
|-----|-----|--------|
| bare `I` | `Alt/Option+I` | Set in point to playhead |
| bare `O` | `Alt/Option+O` | Set out point to playhead |
| `Ctrl+S` | `Alt/Option+S` | Set in/out from selection bounds |

All three use `altModifier` (Option on macOS, Alt on Windows/Linux) — no
conflict on any platform. Registered as `CommandIDs::transportSetInPoint`,
`transportSetOutPoint`, and `transportSetInOutFromSel` in `CommandIDs.h`.
Raw `keyPressed()` handlers removed; the command manager owns dispatch.

### 5b. Dead-key issue: `Cmd+Shift+I` (invert selection)

**Files:** `MainComponent.cpp` line 1300, `PartialView.cpp` line 617

On macOS, Option+I is a dead key (produces `ˆ`). The comment at line 620
documents this. `Cmd+Shift+I` does not involve Option, so the dead key is
not triggered here — the binding works correctly on macOS.

On Windows/Linux, `Ctrl+Shift+I` is the browser/DevTools shortcut, but since
PartialFKR is not a web browser, there is no conflict. **No change needed.**

### 5c. Shortcut reference table (complete)

| Command | macOS | Windows / Linux | Status |
|---------|-------|-----------------|--------|
| New | ⌘N | Ctrl+N | OK |
| Open | ⌘O | Ctrl+O | OK |
| Close | ⌘W | Ctrl+W | OK |
| Save | ⌘S | Ctrl+S | OK |
| Save As | ⌘⇧S | Ctrl+Shift+S | OK |
| Minimize | ⌘M | Ctrl+M | OK |
| Undo | ⌘Z | Ctrl+Z | OK |
| Redo | ⌘⇧Z | Ctrl+Shift+Z | OK |
| Cut | ⌘X | Ctrl+X | OK |
| Copy | ⌘C | Ctrl+C | OK |
| Paste | ⌘V | Ctrl+V | OK |
| Select All | ⌘A | Ctrl+A | OK |
| Deselect All | ⌘⇧A | Ctrl+Shift+A | OK |
| Invert Selection | ⌘⇧I | Ctrl+Shift+I | OK |
| Bridge Partials | ⌘J | Ctrl+J | OK |
| Crossfade Overlap | ⌘⇧J | Ctrl+Shift+J | OK |
| Zoom In | ⌘= | Ctrl+= | OK |
| Zoom Out | ⌘- | Ctrl+- | OK |
| Zoom Fit | ⌘0 | Ctrl+0 | OK |
| Toggle Panel | ⌘⌥S | Ctrl+Alt+S | OK |
| Set in point | ⌥I | Alt+I | OK |
| Set out point | ⌥O | Alt+O | OK |
| Set in/out from selection | ⌥S | Alt+S | OK |

---

## 6. OpenGL / Display

The OpenGL infrastructure (`openGLContext`, `newOpenGLContextCreated`, etc.)
is present but dormant. All rendering currently goes through
`Juce2DRenderer` (software rasterization via `juce::Graphics`). This is
fully cross-platform with no changes needed.

When OpenGL rendering is reactivated (future milestone), note:

- **Windows:** JUCE uses WGL. No special setup required beyond what
  `OpenGLContext::attachTo()` provides.
- **Linux:** JUCE uses GLX. Requires `libGL` and `libX11` — both standard.
  Mesa software fallback is available for CI/VMs.
- **Retina / HiDPI:** On macOS, JUCE scales the OpenGL viewport via
  `Component::getDesktopScaleFactor()`. On Windows 10+, the same API works
  when the app is DPI-aware. Add `<dpiAware>true/PM</dpiAware>` to the
  Windows manifest (JUCE's `juce_add_gui_app` handles this when
  `NEEDS_WEB_BROWSER 0` is set and the JUCE manifest is used).
- **Linux HiDPI:** Wayland and fractional scaling under X11 both require
  reading `GDK_SCALE` or `QT_SCALE_FACTOR` (JUCE reads these automatically
  in recent versions). Test on a 200% display before shipping.

---

## 7. Audio Device

`MainComponent` inherits `AudioAppComponent` and calls `setAudioChannels(0, 2)`.
JUCE dispatches to:

- **macOS:** CoreAudio (already working)
- **Windows:** WASAPI (default), DirectSound (fallback). JUCE selects WASAPI
  automatically. No code changes needed; verify in the Audio Settings dialog.
- **Linux:** ALSA (default), PulseAudio/PipeWire (optional). JUCE selects
  ALSA by default. On modern systems PipeWire is preferred; recommend shipping
  instructions that cover `pw-jack` or PipeWire's ALSA emulation layer.

JUCE's `AudioDeviceSelectorComponent` (used in Settings dialogs) handles
device enumeration and UI on all three platforms automatically. If a Settings
dialog does not yet exist, add one before Linux testing — default device
selection is less reliable on Linux than macOS.

---

## 8. File Dialogs and Paths

All `FileChooser` calls use standard JUCE API and portable file extension
strings. `juce::File` uses the platform's path separator internally. **No
changes needed.**

Verify on Windows that the `.pfkr` file association (currently registered via
PlistBuddy in CMakeLists) has an equivalent Windows registry entry. This is
separate from the CMake build and would be handled by the installer (see §9).

---

## 9. Platform-Specific UX Details

### macOS (existing, for reference)

- Proxy icon in title bar (file identity drag target) — `MacProxyIcon`
- Zoom button → window expansion — `MacWindowHelpers::zoom()`
- "Bring All to Front" in Window menu — `MacWindowHelpers::bringAllToFront()`
- "About" in Apple menu
- `.pfkr` UTI registration via PlistBuddy post-build

### Windows

| Feature | Implementation | Notes |
|---------|---------------|-------|
| No proxy icon | `MacProxyIcon::set()` is a no-op | Acceptable; Windows has no equivalent |
| Window maximize | `DocumentWindow::allButtons` handles | Native button already present |
| "About" in Help menu | Already guarded by `#if !JUCE_MAC` | Works |
| `.pfkr` file association | Windows installer must write registry keys | See §10 Windows packaging |
| Minimize shortcut | `Cmd` → `Ctrl+M` via commandModifier | Verify; Windows convention is `Win+Down` but Ctrl+M is acceptable |

### Linux

| Feature | Implementation | Notes |
|---------|---------------|-------|
| No proxy icon | `MacProxyIcon::set()` is a no-op | Acceptable |
| Window maximize | `DocumentWindow::allButtons` handles | Depends on WM; test under GNOME, KDE |
| "About" in Help menu | Already guarded by `#if !JUCE_MAC` | Works |
| `.pfkr` file association | `.desktop` file + `xdg-mime` | Part of AppImage/deb packaging |
| Font rendering | JUCE uses FreeType | Test that waveform labels are legible |
| HiDPI | Fractional scaling (Wayland) | Set `GDK_SCALE=2` in CI to catch issues |

---

## 10. Packaging and Distribution

### macOS (existing)

`scripts/distribute.sh` — build, sign, notarize, staple, create DMG.
Documented in `docs/DISTRIBUTION.md`.

### Windows (future)

Recommended toolchain: **NSIS** (free) or **Inno Setup** (free) for the
installer. WiX is more complex and better suited to enterprise software.

Installer must:
1. Copy `PartialFKR.exe` to `%ProgramFiles%\ideocentric\PartialFKR\`
2. Write registry key for `.pfkr` file association:
   ```
   HKCR\.pfkr → PartialFKR.Project
   HKCR\PartialFKR.Project\shell\open\command → "PartialFKR.exe" "%1"
   ```
3. Create Start Menu shortcut
4. Add uninstaller entry in Add/Remove Programs

Code signing on Windows uses `signtool.exe` with an EV Code Signing
certificate (required for SmartScreen to not flag the installer).

### Linux (future)

Recommended format: **AppImage** for the broadest distribution compatibility
(no root required, runs on any distro). Supplement with `.deb` for Debian/
Ubuntu users.

AppImage toolchain:
- Build with CMake + Ninja against the target distro's Qt/GTK headers
- Use `linuxdeploy` + `linuxdeploy-plugin-appimage` to bundle dependencies
- Include a `.desktop` file and icon for file association

`.desktop` file entry for `.pfkr` association:
```
[Desktop Entry]
MimeType=application/x-pfkr;
```
Register with `xdg-mime install --novendor partialfkr.xml`.

---

## 11. Implementation Order

Work is blocked on items in each phase completing before the next begins.

### Phase 1 — Compile on all platforms (1–2 hrs)

1. CMakeLists.txt: move `.mm` files into `if(APPLE)` block
2. `MacProxyIcon.cpp`: wrap body in `#if JUCE_MAC`
3. Verify debug build compiles on macOS (no regression)
4. Get a Windows or Linux CI environment and confirm it compiles

### Phase 2 — Runs correctly on all platforms (2–4 hrs)

5. ~~Remap `Ctrl+S` (set in/out)~~ — done: Alt/Option+I, O, S via command manager
6. Hide "Bring All to Front" in Window menu on non-macOS
7. Smoke-test: launch, load a WAV, analyze, play audio, export MIDI

### Phase 3 — Per-platform UX polish (4–8 hrs total)

**Windows:**
8. Verify WASAPI device selection and audio device dialog
9. Test all keyboard shortcuts; confirm Ctrl ≠ Cmd confusion is absent
10. Confirm `.pfkr` file open via double-click works (requires installer)

**Linux:**
11. Test under GNOME (X11) and ideally KDE (Wayland)
12. Verify ALSA / PipeWire audio output
13. Test HiDPI rendering at 200% scale
14. Verify font legibility in `Juce2DRenderer` at default size

### Phase 4 — Packaging (per-platform, variable effort)

15. Windows: NSIS installer with `.pfkr` registry entries and signing
16. Linux: AppImage + `.desktop` file + `xdg-mime` registration

---

## 12. Testing Checklist (per platform)

Run on each target platform (macOS already green):

- [ ] App launches without crash
- [ ] Menu bar renders correctly with all 6 menus
- [ ] All keyboard shortcuts fire the expected commands
- [ ] Open audio file → analysis → partials display in PartialView
- [ ] Mute/unmute a partial, hear the result in real time
- [ ] Transport: play, pause, stop, loop
- [ ] Save / load a `.pfkr` project file
- [ ] Export MIDI, JSON, Csound
- [ ] Audio device dropdown (if Settings dialog exists) shows correct devices
- [ ] Resize window; PartialView redraws correctly
- [ ] HiDPI: launch on a 200% display; text and partial lines are sharp
