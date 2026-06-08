#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

"$BIN" split \
  "tests/fixtures/testing.pdf" \
  --points "2, 5" \
  -o "$WORKDIR/_split.pdf"

qpdf --check "$WORKDIR/testing_split-1.pdf"
qpdf --check "$WORKDIR/testing_split-2.pdf"

pages="$(pdfinfo "$WORKDIR/testing_split-1.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "2" ]; then
  echo "Expected 2 pages, got $pages in the first split"
  exit 1
fi

pages="$(pdfinfo "$WORKDIR/testing_split-2.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "3" ]; then
  echo "Expected 3 pages, got $pages in the second split"
  exit 1
fi

echo ""
echo "Split test: PASSED"
