#!/usr/bin/env bash
# distribute.sh — Build, sign, notarize, staple, and package PartialFKR for distribution.
#
# Usage:
#   export NOTARY_KEY_FILE=/path/to/AuthKey_XXXX.p8
#   export NOTARY_KEY_ID=XXXXXXXXXX
#   export NOTARY_ISSUER_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
#   ./scripts/distribute.sh
#
# The three NOTARY_* variables map to your App Store Connect API key.
# Never commit the .p8 file to git — it is listed in .gitignore.

set -euo pipefail

# ── Load .env if present ─────────────────────────────────────────────────────
ENV_FILE="$(cd "$(dirname "$0")/.." && pwd)/.env"
if [[ -f "${ENV_FILE}" ]]; then
    # shellcheck disable=SC1090
    set -a; source "${ENV_FILE}"; set +a
fi

# ── Identity ──────────────────────────────────────────────────────────────────
SIGN_IDENTITY="Developer ID Application: Matthew Comeione (3722A5T4AG)"
BUNDLE_ID="com.ideocentric.PartialFKR"

# ── Paths ─────────────────────────────────────────────────────────────────────
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-release"
APP_PATH="${BUILD_DIR}/PartialFKR_artefacts/Release/PartialFKR.app"

# Read version from CMakeLists.txt
VERSION=$(grep -m1 'project(PartialFKR VERSION' "${REPO_ROOT}/CMakeLists.txt" \
          | sed 's/.*VERSION \([0-9.]*\).*/\1/')
DMG_NAME="${REPO_ROOT}/PartialFKR-${VERSION}.dmg"

# ── Validate env vars ─────────────────────────────────────────────────────────
: "${NOTARY_KEY_FILE:?Set NOTARY_KEY_FILE to the path of your .p8 API key file}"
: "${NOTARY_KEY_ID:?Set NOTARY_KEY_ID to the 10-character key ID}"
: "${NOTARY_ISSUER_ID:?Set NOTARY_ISSUER_ID to the Issuer UUID from App Store Connect}"

if [[ ! -f "${NOTARY_KEY_FILE}" ]]; then
    echo "ERROR: NOTARY_KEY_FILE not found: ${NOTARY_KEY_FILE}"
    exit 1
fi

echo "==> PartialFKR ${VERSION} — distribution build"
echo "    App:      ${APP_PATH}"
echo "    Output:   ${DMG_NAME}"
echo ""

# ── 1. Build Release with signing enabled ────────────────────────────────────
echo "[1/5] Building Release..."
cmake -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPFKR_CODESIGN=ON \
    "${REPO_ROOT}"
ninja -C "${BUILD_DIR}" PartialFKR

# ── 2. Verify signature ───────────────────────────────────────────────────────
echo "[2/5] Verifying signature..."
codesign --verify --deep --strict --verbose=2 "${APP_PATH}"
spctl --assess --type exec --verbose "${APP_PATH}"

# ── 3. Notarize ───────────────────────────────────────────────────────────────
echo "[3/5] Notarizing..."
STAGING_ZIP="${REPO_ROOT}/PartialFKR-staging.zip"
ditto -c -k --keepParent "${APP_PATH}" "${STAGING_ZIP}"

xcrun notarytool submit "${STAGING_ZIP}" \
    --key          "${NOTARY_KEY_FILE}" \
    --key-id       "${NOTARY_KEY_ID}" \
    --issuer       "${NOTARY_ISSUER_ID}" \
    --wait \
    --verbose

rm -f "${STAGING_ZIP}"

# ── 4. Staple ─────────────────────────────────────────────────────────────────
echo "[4/5] Stapling notarization ticket..."
xcrun stapler staple "${APP_PATH}"
xcrun stapler validate "${APP_PATH}"

# ── 5. Create and sign DMG ───────────────────────────────────────────────────
echo "[5/5] Creating DMG..."
rm -f "${DMG_NAME}"

if ! command -v create-dmg &>/dev/null; then
    echo "ERROR: create-dmg not found. Install with: brew install create-dmg"
    exit 1
fi

# Temporary staging folder so the DMG contains only the .app
STAGING_DIR=$(mktemp -d)
cp -R "${APP_PATH}" "${STAGING_DIR}/"

create-dmg \
    --volname "PartialFKR ${VERSION}" \
    --background "${REPO_ROOT}/Resources/dmg-background.png" \
    --window-pos 200 120 \
    --window-size 631 390 \
    --icon-size 100 \
    --icon "PartialFKR.app" 175 135 \
    --hide-extension "PartialFKR.app" \
    --app-drop-link 455 135 \
    --eula "${REPO_ROOT}/Resources/license.rtf" \
    "${DMG_NAME}" \
    "${STAGING_DIR}/"

rm -rf "${STAGING_DIR}"

# Sign the DMG itself
codesign \
    --sign "${SIGN_IDENTITY}" \
    --timestamp \
    "${DMG_NAME}"

echo ""
echo "Done: ${DMG_NAME}"
echo "Share this DMG with testers — it is signed, notarized, and stapled."