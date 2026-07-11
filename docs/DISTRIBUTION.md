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
| Python 3.8+ with `venv` | system or `brew install python3` | `dmgbuild` packaging — `python3 -m venv` must be available |
| GitHub CLI | `brew install gh` + `gh auth login` | Uploading DMG and manual to the GitHub release |

No manual `pip install` is required. The first DMG build creates a self-contained venv at
`packaging/macos/.venv/` and installs all Python dependencies automatically.
To rebuild the venv from scratch (e.g. after a Python upgrade or a corrupted install):
```bash
rm -rf packaging/macos/.venv
cmake --build <build-dir> --target dmg   # re-bootstraps on next run
```

### Python packaging environment

DMG creation depends on [dmgbuild](https://github.com/al45tair/dmgbuild), a Python tool that
builds the DMG layout programmatically without AppleScript or Finder.

The toolchain is fully self-contained:

| Path | Purpose |
|------|---------|
| `packaging/macos/.venv/` | Local venv — gitignored, never committed |
| `packaging/macos/requirements.txt` | Pinned dependencies (`dmgbuild~=1.6`) |
| `packaging/macos/make-dmg.sh` | Bootstrap script — creates venv and runs dmgbuild |

**Bootstrap** happens automatically the first time `cmake --build ... --target dmg` is run:
1. `python3 -m venv packaging/macos/.venv/` creates the isolated environment
2. `pip install -r requirements.txt` installs the pinned dmgbuild version
3. `dmgbuild/core.py` is patched to pass `-verbose -debug` to every `hdiutil` call (see [Troubleshooting](#troubleshooting) below)

The venv's Python is used for all subsequent DMG builds — the system Python, Homebrew Python,
conda, or pyenv installations are never touched.

**Rebuilding the venv** — if dmgbuild behaves unexpectedly or you upgrade Python:
```bash
rm -rf packaging/macos/.venv
cmake --build <build-dir> --target dmg   # re-bootstraps cleanly
```

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

The script runs seven steps:

1. **Build** — Release universal binary (`arm64;x86_64`) via CMake + Ninja into `build-release/`
2. **Verify** — `codesign --verify` + `spctl --assess` confirm the app is properly signed
3. **Notarize app** — zips the `.app`, submits to Apple's notary service, waits for approval
4. **Staple app** — attaches the notarization ticket to the `.app` bundle
5. **Package** — `cmake --build build-release --target dmg` places the stapled `.app` into the DMG,
   then `codesign` signs the DMG itself
6. **Notarize DMG** — submits the signed DMG to the notary service, waits, then staples the ticket
7. **Upload** — uploads the signed DMG and `docs/MANUAL.pdf` to the GitHub draft release via `gh`

The app is fully notarized and stapled **before** it is placed in the DMG. Both the `.app` and
the DMG carry independent notarization tickets, ensuring Gatekeeper passes cleanly in all
download scenarios.

Output: `build-release/PartialFKR-<version>-macOS.dmg`

### Testing the DMG without signing

For local testing:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target dmg   # bootstraps packaging/macos/.venv/ on first run
open build/PartialFKR-<version>-macOS.dmg
```

CI also produces an unsigned DMG artifact (`PartialFKR-macOS-CI-unsigned`) on every push to
master. This is useful for verifying DMG layout changes without a local build. It is unsigned
and unnotarized — suitable for contributor testing only, not end-user distribution.

### Bumping the version

Version is set in one place: `project(PartialFKR VERSION x.y.z)` in `CMakeLists.txt`.
Update it before running the distribution script; the DMG filename and installer metadata
are derived from it automatically.

Tag and push the new version:
```bash
git tag -a v0.1.1 -m "v0.1.1"
git push origin master
git push origin v0.1.1
```

Pushing the tag triggers `.github/workflows/release.yml`, which creates a **draft release**
and automatically builds and attaches the Linux `.deb` and `.rpm` packages.

Run the distribution script to build, notarize, and upload the macOS DMG:
```bash
./scripts/distribute.sh
```
This produces `build-release/PartialFKR-<version>-macOS.dmg` (signed, notarized, stapled) and
uploads it along with `docs/MANUAL.pdf` to the GitHub draft release automatically.

Then open the release on GitHub, review the auto-generated notes, and click **Publish release**:
```bash
gh release view v0.1.1 --web
```

### Troubleshooting

**hdiutil hangs during DMG creation**
`hdiutil` is known to hang on certain macOS versions, most often during the `convert` step
(compressing the read-write image to the final UDZO). To aid diagnosis, `-verbose -debug` is
automatically injected into every `hdiutil` call via the dmgbuild patch applied at venv
bootstrap. This diagnostic output flows directly to the terminal — it is not captured by
dmgbuild — so it appears inline in the cmake build output.

To capture the full output to a file for analysis:
```bash
cmake --build <build-dir> --target dmg 2>&1 | tee ~/dmg-build.log
```
Or when running the full distribution script:
```bash
./scripts/distribute.sh 2>&1 | tee ~/distribute-$(date +%Y%m%d-%H%M%S).log
```

The last line printed before a hang identifies the blocking `hdiutil` operation. To isolate
whether `hdiutil` itself is the issue — independent of the app bundle and dmgbuild — run the
standalone smoke test:
```bash
bash packaging/macos/test-hdiutil.sh 2>&1 | tee ~/hdiutil-test.log
```
This runs `create`, `convert`, and `attach`/`detach` on a trivial scratch image and reports
which phase (if any) stalls.

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

### Current status — signing staged, inert

Windows release artifacts are currently **unsigned**. Authenticode signing via SignPath.io
is fully wired into the release workflow but **gated off** — the signing steps skip unless the
repo secret `SIGNPATH_API_TOKEN` and variable `SIGNPATH_ORGANIZATION_ID` are set (see
[When ready: SignPath.io](#when-ready-signpathio) below). Setting both is the entire
activation; no workflow edits are needed.

Signing is deferred (steps left inert) until the project has sufficient download history to
build SmartScreen reputation. Signing without established reputation still results in a
SmartScreen warning ("Windows protected your PC") — the same experience as an unsigned
binary — so activating it earns little until there is real download volume. Users who
download from a known source (GitHub releases page) can click **More info → Run anyway** to
proceed. Note: **winget submission effectively requires a signed installer**, so activate
signing before pursuing a winget listing.

### Building the installer

NSIS is required for packaging (`winget install NSIS.NSIS`).

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
cd build && cpack -C Release
# Output: PartialFKR-<version>-Windows-x86_64.exe
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

4. Note your **Organization ID** from the SignPath portal (it appears in the portal URL) and
   add it as a GitHub repository **variable** (not secret) named `SIGNPATH_ORGANIZATION_ID`.

5. In the SignPath portal, confirm the project slug is `PartialFKR` and the signing-policy
   slug is `release-signing` — these are hard-coded in the workflow (change both places if
   you name them differently).

That's the entire activation. **The signing steps are already staged in
`.github/workflows/release.yml`** and are gated on `env.SIGNPATH_API_TOKEN != ''`, so they
stay inert (installers ship unsigned) until the secret and variable above exist. Once both
are set, the next release run signs the app binary before packaging and the installer after —
no workflow edits required.

#### The staged steps (for reference)

The Windows job already contains the following, guarded so they no-op until configured. The
app `.exe` is signed **before** `cpack` (so the installer bundles the signed binary), and the
installer `.exe` is signed **after**. Both signed artifacts are downloaded back in place so the
existing release-upload step picks them up unchanged.

```yaml
- name: Build
  run: cmake --build build --config Release --parallel

- name: Upload unsigned app binary for signing
  if: ${{ env.SIGNPATH_API_TOKEN != '' }}
  id: upload-app
  uses: actions/upload-artifact@v4
  with:
    name: unsigned-app-${{ matrix.name }}
    path: build\PartialFKR_artefacts\Release\PartialFKR.exe

- name: Sign app binary
  if: ${{ env.SIGNPATH_API_TOKEN != '' }}
  uses: signpath/github-action-submit-signing-request@v2
  with:
    connector-url: https://app.signpath.io
    api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
    organization-id: ${{ vars.SIGNPATH_ORGANIZATION_ID }}
    project-slug: PartialFKR
    signing-policy-slug: release-signing
    # Numeric artifact id from the upload step — NOT the artifact name.
    github-artifact-id: ${{ steps.upload-app.outputs.artifact-id }}
    wait-for-completion: true
    output-artifact-directory: build\PartialFKR_artefacts\Release

- name: Package
  working-directory: build
  run: cpack -C Release

- name: Upload unsigned installer for signing
  if: ${{ env.SIGNPATH_API_TOKEN != '' }}
  id: upload-installer
  uses: actions/upload-artifact@v4
  with:
    name: unsigned-installer-${{ matrix.name }}
    path: build\PartialFKR-*-Windows-*.exe

- name: Sign installer
  if: ${{ env.SIGNPATH_API_TOKEN != '' }}
  uses: signpath/github-action-submit-signing-request@v2
  with:
    connector-url: https://app.signpath.io
    api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
    organization-id: ${{ vars.SIGNPATH_ORGANIZATION_ID }}
    project-slug: PartialFKR
    signing-policy-slug: release-signing
    github-artifact-id: ${{ steps.upload-installer.outputs.artifact-id }}
    wait-for-completion: true
    output-artifact-directory: build
```

> **Note on `github-artifact-id`:** the current action (v2) requires the *numeric* artifact
> id emitted by `actions/upload-artifact@v4` (`steps.<id>.outputs.artifact-id`), not the
> artifact *name*. `connector-url` is also required in v2. Earlier drafts of this doc passed
> the name and omitted the connector — both are fixed above.

### Troubleshooting

**SmartScreen warns even after signing**
SmartScreen reputation accumulates per signing identity as more users download and run the
binary without flagging it — typically over weeks to months of distribution volume. Signing
is necessary but not sufficient on its own; reputation must be earned through actual use.
This is why signing is deferred until the project has an established user base.

**NSIS: `cpack` reports "Cannot find NSIS"**
NSIS is not on the PATH. Install via `winget install NSIS.NSIS` and restart the shell,
or add `C:\Program Files (x86)\NSIS` to `PATH` manually.