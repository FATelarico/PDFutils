#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT


# tests/fixtures/Kelsen_1932_ReineRechtslehre.pdf \

"$BIN" img2pdf \
  --dpi 144 \
  --no-auto-rotate \
  "tests/fixtures/DP355340.jpg" \
  -o "$WORKDIR/convert.pdf"

qpdf --check "$WORKDIR/convert.pdf"

pages="$(pdfinfo "$WORKDIR/convert.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "1" ]; then
  echo "Expected 1 pages, got $pages"
  exit 1
fi

echo ""
echo "Convert test: PASSED"
