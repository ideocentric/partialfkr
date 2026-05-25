#!/usr/bin/env bash
# distribute.sh — Build, sign, notarize, staple, and package PartialFKR for distribution.
#
# Prerequisites:
#   - Xcode Command Line Tools (codesign, xcrun notarytool, stapler)
#   - CMake + Ninja
#   - dmgbuild: pip3 install dmgbuild
#
# Credentials — copy .env.example to .env and fill in all four values:
#   CODESIGN_IDENTITY   — Developer ID Application certificate identity string
#   NOTARY_KEY_FILE     — path to your .p8 App Store Connect API key
#   NOTARY_KEY_ID       — 10-character key ID
#   NOTARY_ISSUER_ID    — Issuer UUID from App Store Connect
#
# Usage:
#   ./scripts/distribute.sh
#
# The .env file is gitignored. Never commit credentials.

set -euo pipefail

# ── Load .env if present ─────────────────────────────────────────────────────
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV_FILE="${REPO_ROOT}/.env"
if [[ -f "${ENV_FILE}" ]]; then
    # shellcheck disable=SC1090
    set -a; source "${ENV_FILE}"; set +a
fi

# ── Validate credentials ──────────────────────────────────────────────────────
: "${CODESIGN_IDENTITY:?Set CODESIGN_IDENTITY in .env — see .env.example}"
: "${NOTARY_KEY_FILE:?Set NOTARY_KEY_FILE in .env — see .env.example}"
: "${NOTARY_KEY_ID:?Set NOTARY_KEY_ID in .env — see .env.example}"
: "${NOTARY_ISSUER_ID:?Set NOTARY_ISSUER_ID in .env — see .env.example}"

if [[ ! -f "${NOTARY_KEY_FILE}" ]]; then
    echo "ERROR: NOTARY_KEY_FILE not found: ${NOTARY_KEY_FILE}"
    exit 1
fi

if ! command -v dmgbuild &>/dev/null; then
    echo "ERROR: dmgbuild not found. Install with: pip3 install dmgbuild"
    exit 1
fi

# ── Paths ─────────────────────────────────────────────────────────────────────
BUILD_DIR="${REPO_ROOT}/build-release"
APP_PATH="${BUILD_DIR}/PartialFKR_artefacts/Release/PartialFKR.app"

VERSION=$(grep -m1 'project(PartialFKR VERSION' "${REPO_ROOT}/CMakeLists.txt" \
          | sed 's/.*VERSION \([0-9.]*\).*/\1/')
DMG_PATH="${BUILD_DIR}/PartialFKR-${VERSION}-macOS.dmg"
STAGING_ZIP="${REPO_ROOT}/PartialFKR-staging.zip"

echo "==> PartialFKR ${VERSION} — distribution build"
echo "    Identity: ${CODESIGN_IDENTITY}"
echo "    App:      ${APP_PATH}"
echo "    DMG:      ${DMG_PATH}"
echo ""

# ── 1. Build Release ──────────────────────────────────────────────────────────
echo "[1/6] Building Release (universal binary)..."
cmake -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DPFKR_CODESIGN=ON \
    "${REPO_ROOT}"
ninja -C "${BUILD_DIR}" PartialFKR

# ── 2. Verify signature ───────────────────────────────────────────────────────
echo "[2/6] Verifying app signature..."
codesign --verify --deep --strict --verbose=2 "${APP_PATH}"
spctl --assess --type exec --verbose "${APP_PATH}"

# ── 3. Notarize the app ───────────────────────────────────────────────────────
echo "[3/6] Notarizing app..."
ditto -c -k --keepParent "${APP_PATH}" "${STAGING_ZIP}"

xcrun notarytool submit "${STAGING_ZIP}" \
    --key     "${NOTARY_KEY_FILE}" \
    --key-id  "${NOTARY_KEY_ID}" \
    --issuer  "${NOTARY_ISSUER_ID}" \
    --wait \
    --verbose

rm -f "${STAGING_ZIP}"

# ── 4. Staple notarization ticket to the app ─────────────────────────────────
echo "[4/6] Stapling app..."
xcrun stapler staple "${APP_PATH}"
xcrun stapler validate "${APP_PATH}"

# ── 5. Create and sign DMG ───────────────────────────────────────────────────
# The app is fully notarized and stapled before being placed in the DMG.
echo "[5/6] Creating DMG..."
cmake --build "${BUILD_DIR}" --target dmg

codesign \
    --sign "${CODESIGN_IDENTITY}" \
    --timestamp \
    "${DMG_PATH}"

# ── 6. Notarize and staple the DMG ───────────────────────────────────────────
echo "[6/6] Notarizing DMG..."
xcrun notarytool submit "${DMG_PATH}" \
    --key     "${NOTARY_KEY_FILE}" \
    --key-id  "${NOTARY_KEY_ID}" \
    --issuer  "${NOTARY_ISSUER_ID}" \
    --wait \
    --verbose

xcrun stapler staple "${DMG_PATH}"
xcrun stapler validate "${DMG_PATH}"

echo ""
echo "Done: ${DMG_PATH}"
echo "This DMG is signed, notarized, and stapled — ready for distribution."