#!/usr/bin/env bash
# Minimal hdiutil smoke-test with verbose+debug output.
#
# Runs three isolated operations in sequence so you can see which phase hangs:
#   1. hdiutil create  — build a blank read/write image from a temp folder
#   2. hdiutil convert — convert it to a compressed read-only image (the step dmgbuild does)
#   3. hdiutil attach  — mount the compressed image (optional; comment out if you only want create/convert)
#
# Usage: bash packaging/macos/test-hdiutil.sh [--no-attach]
set -uo pipefail

NO_ATTACH=false
[[ "${1:-}" == "--no-attach" ]] && NO_ATTACH=true

WORK="$(mktemp -d /tmp/hdiutil-test-XXXXXX)"
trap 'echo "-- cleanup $WORK"; rm -rf "$WORK"' EXIT

SRC="$WORK/src"
RW_DMG="$WORK/test-rw.dmg"
FINAL_DMG="$WORK/test-final.dmg"

echo "=== hdiutil version ==="
hdiutil version

echo ""
echo "=== Step 1: create source tree ==="
mkdir -p "$SRC"
echo "hello from PartialFKR hdiutil test" > "$SRC/test.txt"
ls -lh "$SRC"

echo ""
echo "=== Step 2: hdiutil create (UDRW from folder) ==="
hdiutil create \
    -verbose -debug \
    -srcfolder "$SRC" \
    -volname "PartialFKR-test" \
    -fs HFS+ \
    -format UDRW \
    "$RW_DMG"

echo ""
echo "=== Step 3: hdiutil convert (UDRW → UDZO compressed) ==="
hdiutil convert \
    -verbose -debug \
    -format UDZO \
    -o "$FINAL_DMG" \
    "$RW_DMG"

if $NO_ATTACH; then
    echo ""
    echo "=== Skipping attach (--no-attach passed) ==="
else
    echo ""
    echo "=== Step 4: hdiutil attach (mount to verify) ==="
    MOUNT_POINT="$(mktemp -d /tmp/hdiutil-mount-XXXXXX)"
    hdiutil attach \
        -verbose -debug \
        -mountpoint "$MOUNT_POINT" \
        -noautoopen \
        "$FINAL_DMG"
    echo "-- mounted at: $MOUNT_POINT"
    ls -lh "$MOUNT_POINT"
    hdiutil detach -verbose "$MOUNT_POINT"
    rmdir "$MOUNT_POINT"
fi

echo ""
echo "=== All steps completed without hanging ==="