# Help & Accessibility Audit

> Audit date: 2026-07-10. Status: **findings recorded, no fixes applied yet.**
> Scope: in-app Help wiring per platform, tooltip coverage, screen-reader / keyboard
> accessibility, and Linux desktop help conventions. Plan at the end.

## 1. Help mechanism — per platform

Menu bar has a **Help** menu (`Source/Main.cpp`) with **"PartialFKR Help"** (`CommandIDs::appHelp`),
a separator, and **"About PartialFKR"** (`appAbout`, also in the macOS Apple menu). Wiring is
identical on all platforms — no `#ifdef`.

- **"PartialFKR Help"** → `juce::URL("https://github.com/ideocentric/partialfkr").launchInDefaultBrowser()`
  (`Main.cpp:190-192`). Opens the **repo root (README)** in a browser on every OS. Needs internet.
- **"About PartialFKR"** → static dialog (name/version/desc/license/©) + OK. `kHelpUrl` in
  `AboutComponent.cpp` is **dead code** (defined, never used). No clickable link.
- **No F1 shortcut** on the Help command.

**Manual bundling (CMake `install()` + packaging):**

| Platform | App | Local manual bundled? |
|---|---|---|
| macOS (DMG) | `.app` | ❌ none (`packaging/macos/make-dmg.sh` bundles only the app) |
| Windows (NSIS) | `.exe` | ✅ `MANUAL.html` beside the exe |
| Linux (DEB/RPM) | binary + icon + `.desktop` + metainfo | ❌ none |
| GitHub release | — | ✅ `MANUAL.pdf` asset (all platforms) |

**Key issues:** Help never opens the bundled manual (Windows ships `MANUAL.html` next to the exe
but Help opens GitHub, so that file is dead weight); macOS/Linux ship no local manual; Help points
at the repo root, not docs; the AppStream metainfo `<url type="homepage">` is `https://partialfkr.com`
but in-app Help opens GitHub — **inconsistent**; no `<url type="help">` in the metainfo.

## 2. Tooltip coverage

`TooltipWindow` is installed (`MainComponent.h:191`). Coverage is **ToolsPanel only** (12 tooltips):

| Area | Controls | Tooltip |
|---|---|---|
| Tools › Edit Mode | Select, Direct Select | ✅ |
| Tools › Markers | Set In, Set Out, Set In/Out | ✅ |
| Tools › Operations | Bridge, Crossfade, Stretch, Scale, Normalize, Fade In/Out | ✅ |
| Transport bar | Stop, Play/Pause, Loop | ❌ |
| Canvas chrome | Zoom in/out (H & V) | ❌ |
| Right panel | Gain fader | ❌ |
| Reduction filters | 6 sliders + Reset | ❌ (have visible labels) |
| Export dialog | sample-rate combo, Cancel, Export | ❌ |

Existing tooltip *style* is mostly HIG-conformant (verb-first, sentence case, no trailing
punctuation). Minor: they append the shortcut in parens (Apple examples don't; harmless).

## 3. Accessibility (ADA) gaps

**Zero** accessibility-API usage in `Source/` (`setTitle` / `setDescription` /
`createAccessibilityHandler` = 0 occurrences).

1. **Icon buttons aren't keyboard-reachable.** All 12 ToolsPanel buttons and the 3 transport
   buttons set `setWantsKeyboardFocus(false)` (`ToolsPanel.cpp`, `TransportBar.cpp:12-14`) — you
   cannot Tab to them. Keyboard-only users rely entirely on menu commands/shortcuts (zoom has neither).
2. **Empty / poor accessible names.** `TransportButton` and `ZoomButton` are `juce::Button({})`
   (empty text) → a screen reader announces "button" with no label. `DrawableButton`s carry terse
   code names ("select", "xfade", "sel", "fadein") as their accessible name.
3. **Tooltip text is not exposed to AT.** JUCE derives a control's accessible *name* from its
   title/button-text, not its tooltip — so the good strings already written are invisible to
   VoiceOver / Orca / Narrator.
4. **Canvas is opaque to screen readers.** `PartialView` is keyboard-focusable and handles keys,
   but has **no `createAccessibilityHandler`** — the partials (the actual content) are not exposed
   as accessible elements. Deepest gap: the core editing surface is invisible to AT.
5. **Color-only amplitude channel.** Amplitude is encoded by green hue + brightness with no
   non-color cue — unreadable for some low-vision / color-blind users.

## 4. Linux help conventions

| Desktop | Convention |
|---|---|
| GNOME (40+) | "Help" in hamburger/primary menu; **F1** opens help; Mallard/DocBook content in **Yelp** via `help:appname`; online help via AppStream `<url type="help">` |
| KDE | Help menu → **"AppName Handbook" (F1)** → KHelpCenter (DocBook); + Report Bug / About |
| freedesktop (cross-desktop) | **F1** universal help key; **AppStream `<url type="help">`**; `xdg-open` to launch files/URLs |

**JUCE reality:** no native Yelp/KHelpCenter integration (needs Mallard/DocBook + desktop-specific
viewers) — not worth authoring. Pragmatic path mirrors macOS: open bundled HTML
(`launchInDefaultApplication` → `xdg-open`) or online docs.

HIG references: Apple [Offering help](https://developer.apple.com/design/human-interface-guidelines/offering-help)
& [The menu bar](https://developer.apple.com/design/human-interface-guidelines/the-menu-bar);
[GNOME HIG](https://developer.gnome.org/hig/); [KDE HIG](https://develop.kde.org/hig/).

Apple HIG note (verbatim, menu-bar page): the Help-menu **search field** appears
"*When you use the Help Book format for this documentation*"; the App Help item "*opens the content
in the built-in Help Viewer*" when it uses Help Book format. A full Apple Help Book is high-effort
and JUCE has no support for it — treat "open the manual" as a deliberate, acceptable divergence.

## 5. Plan (Help + ADA converge)

1. **Bundle the manual on every platform** — macOS DMG + Linux `share/doc/partialfkr/`, as **HTML**
   (offline + accessible).
2. **One `openHelp()`**: bundled manual if present → else online docs. Fix the URL to a docs page and
   reconcile GitHub vs `partialfkr.com`.
3. **Bind F1** to Help; add `<url type="help">` to the AppStream metainfo.
4. **Accessibility**, by impact-per-effort:
   - *(cheap, high value)* Shared helper that sets **tooltip + accessible name/description** from one
     string, applied to every icon button — fixes labels on all three OSes at once.
   - *(cheap)* Add tooltips to transport / zoom / gain.
   - *(medium)* Reconsider `setWantsKeyboardFocus(false)` so AT/keyboard users can reach the toolbar.
   - *(larger)* `PartialView::createAccessibilityHandler` — at minimum a spoken summary; ideally
     partial-level navigation.
   - *(design)* A non-color amplitude cue.
5. Remove dead `kHelpUrl`, or make the About dialog link to the help/website.