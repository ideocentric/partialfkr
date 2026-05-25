# PartialFKR — Distribution Guide

How to build, sign, notarize, and package a release for macOS and Windows.

For day-to-day build instructions see [`CONTRIBUTING.md`](../CONTRIBUTING.md).

---

## macOS

### Prerequisites

| Tool | Install | Purpose |
|------|---------|---------|
| Xcode Command Line Tools | `xcode-select --install` | `codesign`, `xcrun notarytool`, `stapler` |
| CMake 3.22+ | `brew install cmake` | Build system |
| Ninja | `brew install ninja` | Build generator |
| Python 3 | system or `brew install python3` | Required for `dmgbuild` |
| dmgbuild | `pip3 install dmgbuild` | DMG creation — no AppleScript, no Finder |

### Credentials

You must be enrolled in the **Apple Developer Program** to run a distribution build.
Two credentials are required:

- **Developer ID Application certificate** — installed in your login Keychain.
  Obtain from developer.apple.com → Certificates, IDs & Profiles → Certificates.
  Identity string format: `Developer ID Application: Your Name (TEAMID)`
  To find it locally: `security find-identity -v -p codesigning`

- **App Store Connect API key** (`.p8` file) — for notarization.
  Obtain from developer.apple.com → Keys. The `.p8` is downloaded once; store it safely.
  Never commit this file; it is covered by `.gitignore`.

Copy `.env.example` to `.env` at the repo root and fill in all four values:

```bash
cp .env.example .env
# edit .env
```

```bash
# .env
CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
NOTARY_KEY_FILE=/path/to/AuthKey_XXXXXXXXXX.p8
NOTARY_KEY_ID=XXXXXXXXXX
NOTARY_ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

`scripts/distribute.sh` sources `.env` automatically. All four variables are required —
the script will exit with a clear error if any are missing or if the `.p8` file is not found.

### DMG assets

**Background image** — `packaging/macos/background.png`
600 × 360 px. To replace it, export your new image at exactly 600 × 360, overwrite the file,
and adjust `window_rect` in `packaging/macos/dmgbuild-settings.py` if the dimensions change.

**License (RTF)** — `Resources/license.rtf`
Displayed as a click-through SLA when the DMG is opened. Edit in TextEdit (it renders RTF).
Do not convert to plain text — the RTF format is required for the macOS SLA mechanism.

**Layout** — `packaging/macos/dmgbuild-settings.py`
All icon positions, window size, and background reference live here as plain Python.
No Finder configuration or `.DS_Store` maintenance needed.

### Icon positions

| Element | X | Y |
|---------|---|---|
| PartialFKR.app | 150 | 125 |
| Applications (symlink) | 300 | 125 |
| license.rtf | 450 | 125 |

Coordinates are pixels from the top-left of the DMG window (600 × 360 px).
To adjust, edit `icon_locations` in `packaging/macos/dmgbuild-settings.py` and rebuild the DMG target.

### Running the distribution script

```bash
# From the repo root
cp .env.example .env        # fill in your credentials
./scripts/distribute.sh
```

The script runs six steps:

1. **Build** — Release universal binary (`arm64;x86_64`) via CMake + Ninja into `build-release/`
2. **Verify** — `codesign --verify` + `spctl --assess` confirm the app is properly signed
3. **Notarize app** — zips the `.app`, submits to Apple's notary service, waits for approval
4. **Staple app** — attaches the notarization ticket to the `.app` bundle
5. **Package** — `cmake --build build-release --target dmg` places the stapled `.app` into the DMG,
   then `codesign` signs the DMG itself
6. **Notarize DMG** — submits the signed DMG to the notary service, waits, then staples the ticket

The app is fully notarized and stapled **before** it is placed in the DMG. Both the `.app` and
the DMG carry independent notarization tickets, ensuring Gatekeeper passes cleanly in all
download scenarios.

Output: `build-release/PartialFKR-0.1.0-macOS.dmg`

### Testing the DMG without signing

For local testing:
```bash
pip3 install dmgbuild          # one-time
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target dmg
open build/PartialFKR-0.1.0-macOS.dmg
```

CI also produces an unsigned DMG artifact (`PartialFKR-macOS-CI-unsigned`) on every push to
master. This is useful for verifying DMG layout changes without a local build. It is unsigned
and unnotarized — suitable for contributor testing only, not end-user distribution.

### Bumping the version

Version is set in one place: `project(PartialFKR VERSION x.y.z)` in `CMakeLists.txt`.
Update it before running the distribution script; the DMG filename and installer metadata
are derived from it automatically.

### Troubleshooting

**Notarization rejected**
Check the log URL printed by `notarytool`. Common causes: missing hardened runtime entitlements,
unsigned embedded framework, or a code-signing error surfaced after submission. Fix the issue
in the entitlements file or the `codesign` invocation and rebuild.

**`spctl --assess` fails after signing**
The Developer ID certificate may not be trusted on this machine.
Run `security find-identity -v -p codesigning` to confirm it is present, valid, and not expired.

**DMG mounts but shows no background**
The `background.png` path in `dmgbuild-settings.py` is passed at build time from `make-dmg.sh`.
Verify `packaging/macos/background.png` exists. Run `cmake --build build --target dmg` again
with verbose output: `cmake --build build --target dmg -- -v`.

---

## Windows

### Current status — signing deferred

Windows release artifacts are currently **unsigned**. The CI job produces an NSIS installer
for contributor testing only (see the `PartialFKR-Windows-<arch>-CI-unsigned` artifact).

Signing is deferred until the project has sufficient download history to build SmartScreen
reputation. Signing without established reputation still results in a SmartScreen warning
("Windows protected your PC") — the same experience as an unsigned binary — so the overhead
of setting up signing is not yet justified. Users who download from a known source (GitHub
releases page) can click **More info → Run anyway** to proceed.

### Building the installer

NSIS is required for packaging (`winget install NSIS.NSIS`).

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
cd build && cpack -C Release
# Output: PartialFKR-0.1.0-Windows-x86_64.exe
```

### When ready: SignPath.io

**[SignPath.io](https://signpath.io)** is the planned signing solution. It provides free
Authenticode signing for open source projects, requires no DUNS number or business
registration, and integrates directly with GitHub Actions. Identity is verified through
GitHub repository ownership.

#### One-time setup

1. Apply for the open source program at **signpath.io/open-source**.
   Approval typically takes a few business days. You will need a public GitHub repository
   with an OSI-approved license (AGPLv3 qualifies).

2. Once approved, log into the SignPath portal and:
   - Create a **Project** for PartialFKR
   - Create an **Artifact Configuration** that targets `.exe` files
   - Create a **Signing Policy** (e.g. `release-signing`) linked to the open source certificate

3. Generate a **CI User API token** in the SignPath portal and add it as a GitHub repository
   secret named `SIGNPATH_API_TOKEN`.

4. Note your **Organization ID** from the SignPath portal — it appears in the URL and is
   needed in the workflow.

#### GitHub Actions workflow excerpt

Add the following steps to the Windows job after building, when ready to enable signing.
Both the application binary and the installer must be signed separately.

```yaml
- name: Build
  run: cmake --build build --config Release --parallel

- name: Upload unsigned binary for signing
  uses: actions/upload-artifact@v4
  with:
    name: unsigned-binary
    path: build\PartialFKR_artefacts\Release\PartialFKR.exe

- name: Sign application binary
  uses: signpath/github-action-submit-signing-request@v1
  with:
    api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
    organization-id: '<your-organization-id>'
    project-slug: 'PartialFKR'
    signing-policy-slug: 'release-signing'
    github-artifact-id: 'unsigned-binary'
    wait-for-completion: true
    output-artifact-directory: signed-binary

- name: Package
  working-directory: build
  run: cpack -C Release

- name: Upload unsigned installer for signing
  uses: actions/upload-artifact@v4
  with:
    name: unsigned-installer
    path: build\PartialFKR-*-Windows-*.exe

- name: Sign installer
  uses: signpath/github-action-submit-signing-request@v1
  with:
    api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
    organization-id: '<your-organization-id>'
    project-slug: 'PartialFKR'
    signing-policy-slug: 'release-signing'
    github-artifact-id: 'unsigned-installer'
    wait-for-completion: true
    output-artifact-directory: signed-installer
```

Replace `<your-organization-id>` with the ID from the SignPath portal before enabling.

### Troubleshooting

**SmartScreen warns even after signing**
SmartScreen reputation accumulates per signing identity as more users download and run the
binary without flagging it — typically over weeks to months of distribution volume. Signing
is necessary but not sufficient on its own; reputation must be earned through actual use.
This is why signing is deferred until the project has an established user base.

**NSIS: `cpack` reports "Cannot find NSIS"**
NSIS is not on the PATH. Install via `winget install NSIS.NSIS` and restart the shell,
or add `C:\Program Files (x86)\NSIS` to `PATH` manually.