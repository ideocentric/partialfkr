# PartialFKR — Distribution Guide

How to build, sign, notarize, and package a release DMG.

---

## Prerequisites

### Tools

| Tool | Install | Purpose |
|------|---------|---------|
| Xcode Command Line Tools | `xcode-select --install` | `codesign`, `xcrun notarytool`, `stapler` |
| Ninja | `brew install ninja` | CMake build generator |
| create-dmg | `brew install create-dmg` | DMG packaging with background and license |

### Credentials

You need three things from App Store Connect:

- **Developer ID Application certificate** — installed in Keychain  
  Identity string: `Developer ID Application: Matthew Comeione (3722A5T4AG)`
- **App Store Connect API key** (`.p8` file) — for notarization  
  Never commit this to git; it is in `.gitignore`

Store credentials in a `.env` file at the repo root (also gitignored):

```bash
NOTARY_KEY_FILE=/path/to/AuthKey_XXXXXXXXXX.p8
NOTARY_KEY_ID=XXXXXXXXXX
NOTARY_ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

`distribute.sh` sources this file automatically if present.

---

## DMG Assets

### Background image

`Resources/dmg-background.png` — 631 × 390 px dark background with a white icon-drop rectangle and the PartialFKR logo. Source Illustrator file at `samples/icons/partialFKR-files/screenshots/partialfkr-dmg.ai`.

If you need to regenerate the PNG from the `.ai` file, export at 72 dpi / 100% scale to maintain the 631 × 390 dimensions. For Retina support in a future pass, also export at 144 dpi (1262 × 780) and pass it via `--background-2x` in `distribute.sh`.

### License (RTF)

`Resources/license.rtf` — plain-English AGPLv3 summary displayed as a clickthrough SLA when the DMG is opened. Formatted as RTF so macOS renders it with styled text.

To edit the license text, open `Resources/license.rtf` in TextEdit (it will render the RTF). Make changes, save, and re-run the distribution script. Do not convert it to plain text — `create-dmg` requires RTF for the `--eula` option.

---

## Running the Distribution Script

```bash
# From the repo root
export NOTARY_KEY_FILE=/path/to/AuthKey_XXXXXXXXXX.p8
export NOTARY_KEY_ID=XXXXXXXXXX
export NOTARY_ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx

# Or put the above in .env and the script will source it automatically

./scripts/distribute.sh
```

The script runs five steps:

1. **Build** — Release build with `PFKR_CODESIGN=ON` via CMake + Ninja into `build-release/`
2. **Verify signature** — `codesign --verify` + `spctl --assess`
3. **Notarize** — Zips the `.app`, submits to Apple notary service, waits for approval
4. **Staple** — Attaches the notarization ticket to the `.app` bundle
5. **Package** — `create-dmg` produces `PartialFKR-<version>.dmg` at the repo root, then `codesign` signs the DMG itself

Output: `PartialFKR-0.1.0.dmg` (version read from `CMakeLists.txt`).

---

## Icon Positions in the DMG Window

The DMG window is 631 × 390 px, matching the background image. Icon coordinates are in pixels from the top-left of the window:

| Element | X | Y | Notes |
|---------|---|---|-------|
| PartialFKR.app | 175 | 135 | Left side of white rectangle |
| Applications (symlink) | 455 | 135 | Right side of white rectangle |

If you redesign the background image at a different size, update `--window-size`, `--icon`, and `--app-drop-link` in the step 5 block of `distribute.sh` to match.

To test icon placement without going through the full sign/notarize flow, run just the `create-dmg` command directly against a local Debug build:

```bash
create-dmg \
  --volname "PartialFKR TEST" \
  --background "Resources/dmg-background.png" \
  --window-pos 200 120 \
  --window-size 631 390 \
  --icon-size 100 \
  --icon "PartialFKR.app" 175 135 \
  --hide-extension "PartialFKR.app" \
  --app-drop-link 455 135 \
  --eula "Resources/license.rtf" \
  /tmp/PartialFKR-test.dmg \
  build/PartialFKR_artefacts/Debug/
```

Open `/tmp/PartialFKR-test.dmg` in Finder to verify layout before doing a full distribution build.

---

## Bumping the Version

Version is set in one place: `project(PartialFKR VERSION x.y.z)` in `CMakeLists.txt`. The distribute script reads it from there automatically. Update it before running the script for a release.

---

## Troubleshooting

**`create-dmg` exits with "resource busy"**  
A previous DMG mount is still attached. Run `hdiutil detach /Volumes/PartialFKR*` and retry.

**Notarization rejected**  
Check the log URL printed by `notarytool`. Common causes: missing hardened runtime entitlements, unsigned framework, or a code-signing error caught after submission. Fix the issue in `Resources/MacOS/PartialFKR.entitlements` or the `codesign` invocation and rebuild.

**`spctl --assess` fails after signing**  
The Developer ID certificate may not be trusted on this machine. Run `security find-identity -v -p codesigning` to confirm the certificate is present and valid (not expired).

**Icon positions look wrong after background image update**  
The coordinates are absolute pixels from the DMG window's top-left corner, not relative to the white rectangle. Remeasure against the new image and update the `distribute.sh` values.