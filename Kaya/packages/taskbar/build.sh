#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
make
echo "Build complete. Run ./taskbar (ensure DISPLAY is set for VcXsrv)."
