#!/usr/bin/env bash
# Creates a macOS drag-and-drop DMG using dmgbuild.
#
# dmgbuild writes the .DS_Store programmatically — no AppleScript, no Finder,
# no Applications→/Applications symlink in the source tree (CLion-safe).
# The Applications symlink is created by dmgbuild inside the mounted image only.
#
# Usage:
#   make-dmg.sh <app_bundle> <output.dmg>
set -euo pipefail

APP_PATH="$1"
OUTPUT="$2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

dmgbuild \
    -s "$SCRIPT_DIR/dmgbuild-settings.py" \
    -D "app=$APP_PATH" \
    -D "background=$SCRIPT_DIR/background.png" \
    -D "license=$REPO_ROOT/Resources/license.rtf" \
    "PartialFKR" "$OUTPUT"

echo "Created: $OUTPUT"