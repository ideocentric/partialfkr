#!/usr/bin/env bash
# Creates a macOS drag-and-drop DMG using dmgbuild.
#
# dmgbuild writes the .DS_Store programmatically — no AppleScript, no Finder,
# no Applications→/Applications symlink in the source tree (CLion-safe).
# The Applications symlink is created by dmgbuild inside the mounted image only.
#
# Python dependencies are managed in a local venv at packaging/macos/.venv/
# so they never conflict with the developer's system Python environment.
# The venv is bootstrapped automatically on first run.
#
# dmgbuild hardcodes /usr/bin/hdiutil, so -verbose/-debug are injected by
# patching all_args in the installed dmgbuild/core.py after pip install.
# hdiutil stderr is inherited (not captured), so diagnostic output flows
# directly to the terminal for all create/attach/convert/detach calls.
#
# Usage:
#   make-dmg.sh <app_bundle> <output.dmg>
set -euo pipefail

APP_PATH="$1"
OUTPUT="$2"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"
REQUIREMENTS="$SCRIPT_DIR/requirements.txt"

# ── Bootstrap venv if missing or dmgbuild not installed ──────────────────────
if [[ ! -x "$VENV_DIR/bin/dmgbuild" ]]; then
    echo "-- Setting up packaging venv at $VENV_DIR"
    python3 -m venv "$VENV_DIR"
    "$VENV_DIR/bin/pip" install --quiet --require-virtualenv -r "$REQUIREMENTS"

    # Patch dmgbuild to pass -verbose -debug to every hdiutil call.
    # dmgbuild hardcodes /usr/bin/hdiutil and builds all_args as:
    #   all_args = ["/usr/bin/hdiutil", cmd]
    # We append the flags immediately after cmd so they apply to every verb
    # (create, attach, convert, detach, resize).
    # Use the venv's own Python to locate core.py — avoids hardcoding the
    # Python minor version in the lib/ path.
    CORE_PY="$("$VENV_DIR/bin/python3" -c \
        'import dmgbuild, os; print(os.path.join(os.path.dirname(dmgbuild.__file__), "core.py"))')"
    sed -i '' \
        's|all_args = \["/usr/bin/hdiutil", cmd\]|all_args = ["/usr/bin/hdiutil", cmd, "-verbose", "-debug"]|' \
        "$CORE_PY"
    echo "-- Patched dmgbuild/core.py with -verbose -debug"
    echo "-- Venv ready"
fi

"$VENV_DIR/bin/dmgbuild" \
    -s "$SCRIPT_DIR/dmgbuild-settings.py" \
    -D "app=$APP_PATH" \
    -D "background=$SCRIPT_DIR/background.png" \
    -D "license=$REPO_ROOT/Resources/license.rtf" \
    "PartialFKR" "$OUTPUT"

echo "Created: $OUTPUT"