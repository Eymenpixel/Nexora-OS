#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/Kaya/packages/nexora-welcome"
OUTPUT="$ROOT_DIR/Kaya/packages/nexora-welcome_0.1.0.kaya"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/package"
cp "$SOURCE_DIR/control" "$TMP_DIR/package/control"
tar -czf "$TMP_DIR/package/data.tar.gz" -C "$SOURCE_DIR/files" .
tar -czf "$OUTPUT" -C "$TMP_DIR/package" control data.tar.gz
printf 'Created %s\n' "$OUTPUT"
