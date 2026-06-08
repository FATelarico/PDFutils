#!/usr/bin/env bash
set -euo pipefail

BIN="$1"
# BIN="PDFutils-cli"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT


# tests/fixtures/testing.pdf \

"$BIN" insert \
  -i "tests/fixtures/blank.pdf" \
  --after-every \
  "tests/fixtures/testing.pdf" \
  -o "$WORKDIR/inserted.pdf"

qpdf --check "$WORKDIR/inserted.pdf"

pages="$(pdfinfo "$WORKDIR/inserted.pdf" | awk '/^Pages:/ {print $2}')"

if [ "$pages" != "16" ]; then
  echo "Expected 16 pages, got $pages"
  exit 1
fi

echo ""
echo "Insert test: PASSED"
