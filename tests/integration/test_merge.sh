#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

"$BIN" merge \
  ".tests/fixtures/blank.pdf" \
  ".tests/fixtures/testing.pdf" \
  ".tests/fixtures/blank.pdf" \
  --pages ";1-2;" \
  -o "$WORKDIR/merged.pdf"

qpdf --check "$WORKDIR/merged.pdf"

pages="$(pdfinfo "$WORKDIR/merged.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "4" ]; then
  echo "Expected 4 pages, got $pages"
  exit 1
fi

echo ""
echo "Merge test: PASSED"
