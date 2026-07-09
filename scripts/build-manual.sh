#!/usr/bin/env bash
# build-manual.sh — Regenerate the user manual from docs/MANUAL.md.
#
# MANUAL.md is the single source of truth. It has TWO generated outputs that must
# stay in sync:
#   - docs/MANUAL.pdf   — uploaded to the GitHub release (see scripts/distribute.sh)
#   - docs/MANUAL.html  — self-contained (CSS + screenshots inlined), bundled into
#                         the Windows installer (see CMakeLists.txt WIN32 branch)
#
# Run this whenever docs/MANUAL.md or any referenced screenshot changes, then commit
# both artifacts.
#
# Prerequisites:
#   - pandoc
#   - a pandoc PDF engine — weasyprint (HTML/CSS based; matches docs/manual.css)
#
# Usage:
#   ./scripts/build-manual.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DOCS_DIR="${REPO_ROOT}/docs"
TITLE="PartialFKR User Manual"

# ── Tool checks ───────────────────────────────────────────────────────────────
if ! command -v pandoc >/dev/null 2>&1; then
    echo "ERROR: pandoc not found. Install it (e.g. 'brew install pandoc')." >&2
    exit 1
fi
if ! command -v weasyprint >/dev/null 2>&1; then
    echo "ERROR: weasyprint (the PDF engine) not found." >&2
    echo "       Install it (e.g. 'pip install weasyprint') or put it on PATH." >&2
    exit 1
fi

cd "${DOCS_DIR}"

# ── 1. PDF (release asset) ────────────────────────────────────────────────────
echo "[1/2] Building MANUAL.pdf ..."
pandoc MANUAL.md -o MANUAL.pdf \
    --standalone \
    --pdf-engine=weasyprint \
    --css=manual.css \
    --metadata title="${TITLE}"

# ── 2. Self-contained HTML (Windows installer doc) ────────────────────────────
# --embed-resources inlines manual.css and every screenshot as data URIs so the
# single file renders offline with no sibling assets.
echo "[2/2] Building MANUAL.html (self-contained) ..."
pandoc MANUAL.md -o MANUAL.html \
    --standalone \
    --embed-resources \
    --css manual.css \
    --metadata title="${TITLE}"

echo ""
echo "Done. Regenerated:"
printf '  %-20s %s\n' "docs/MANUAL.pdf"  "$(du -h MANUAL.pdf  | cut -f1)"
printf '  %-20s %s\n' "docs/MANUAL.html" "$(du -h MANUAL.html | cut -f1)"
echo ""
echo "Commit both artifacts alongside your MANUAL.md changes."