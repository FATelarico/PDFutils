#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT


# tests/fixtures/testing.pdf \

"$BIN" extract \
  tests/fixtures/testing.pdf \
  --pages "1-3, 4, 7" \
  -o "$WORKDIR/_part.pdf"

qpdf --check "$WORKDIR/testing_part-1.pdf"
qpdf --check "$WORKDIR/testing_part-2.pdf"
qpdf --check "$WORKDIR/testing_part-3.pdf"

pages="$(pdfinfo "$WORKDIR/testing_part-1.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "3" ]; then
  echo "Expected 3 pages, got $pages in the first part"
  exit 1
fi

pages="$(pdfinfo "$WORKDIR/testing_part-2.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "1" ]; then
  echo "Expected 1 pages, got $pages in the second part"
  exit 1
fi

pages="$(pdfinfo "$WORKDIR/testing_part-3.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "1" ]; then
  echo "Expected 1 pages, got $pages in the third part"
  exit 1
fi

echo ""
echo "Extract test: PASSED"
