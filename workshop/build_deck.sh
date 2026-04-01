#!/bin/zsh
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

mkdir -p "$SCRIPT_DIR/.mpl-cache"
mkdir -p "$SCRIPT_DIR/.cache"

if [[ -n "${PYTHON_BIN:-}" ]]; then
  PYTHON="$PYTHON_BIN"
elif [[ -x /opt/anaconda3/bin/python3 ]]; then
  PYTHON=/opt/anaconda3/bin/python3
else
  PYTHON=$(command -v python3)
fi

if [[ -z "$PYTHON" ]]; then
  echo "python3 not found" >&2
  exit 1
fi

if ! command -v pandoc >/dev/null 2>&1; then
  echo "pandoc not found" >&2
  exit 1
fi

XDG_CACHE_HOME="$SCRIPT_DIR/.cache" \
MPLCONFIGDIR="$SCRIPT_DIR/.mpl-cache" \
  "$PYTHON" generate_assets.py
pandoc perfmap_workshop_deck.md -t pptx -o perfmap_workshop_deck.pptx

echo "Built $SCRIPT_DIR/perfmap_workshop_deck.pptx"
