#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KAYP="$ROOT_DIR/Kaya/kayp/kayp"
PKG_DIR="$ROOT_DIR/Kaya/packages/nexora-welcome"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/pkg" "$TMP_DIR/dist"
cp "$PKG_DIR/control" "$TMP_DIR/pkg/control"
tar -czf "$TMP_DIR/pkg/data.tar.gz" -C "$PKG_DIR/files" .
tar -czf "$TMP_DIR/dist/nexora-welcome_0.1.0.kaya" -C "$TMP_DIR/pkg" control data.tar.gz

"$KAYP" info "$TMP_DIR/dist/nexora-welcome_0.1.0.kaya" | grep -q 'Name: nexora-welcome'
"$KAYP" install "$TMP_DIR/dist/nexora-welcome_0.1.0.kaya" --root "$TMP_DIR/root"
test -f "$TMP_DIR/root/usr/share/nexora/welcome.txt"
"$KAYP" list --root "$TMP_DIR/root" | grep -q 'nexora-welcome'
"$KAYP" remove nexora-welcome --root "$TMP_DIR/root"
test ! -e "$TMP_DIR/root/usr/share/nexora/welcome.txt"
"$KAYP" list --root "$TMP_DIR/root" | grep -q 'kurulu paket yok'
echo 'Kaya smoke test passed (temporary root only).'
