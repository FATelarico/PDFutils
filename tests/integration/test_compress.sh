#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT


# tests/fixtures/Kelsen_1932_ReineRechtslehre.pdf \

"$BIN" compress \
  --qpdf \
  --optimize-images \
  "tests/fixtures/Hobbes_1651_Leviathan.pdf" \
  -o "$WORKDIR/compress.pdf"

qpdf --check "$WORKDIR/compress.pdf"

pages="$(pdfinfo "$WORKDIR/compress.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "598" ]; then
  echo "Expected 598 pages, got $pages"
  exit 1
fi

original_size="$(stat -c%s "tests/fixtures/Hobbes_1651_Leviathan.pdf")"
compressed_size="$(stat -c%s "$WORKDIR/compress.pdf")"

if [ "$compressed_size" -ge "$original_size" ]; then
  echo "Expected compressed PDF to be smaller"
  # echo "Original:   $original_size bytes"
  # echo "Compressed: $compressed_size bytes"
  exit 1
fi

echo ""
echo "Compress test: PASSED"
