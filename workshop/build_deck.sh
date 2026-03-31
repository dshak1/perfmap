#!/bin/zsh
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

mkdir -p "$SCRIPT_DIR/.mpl-cache"
mkdir -p "$SCRIPT_DIR/.cache"
XDG_CACHE_HOME="$SCRIPT_DIR/.cache" \
MPLCONFIGDIR="$SCRIPT_DIR/.mpl-cache" \
  /opt/anaconda3/bin/python3 generate_assets.py
pandoc perfmap_workshop_deck.md -t pptx -o perfmap_workshop_deck.pptx

echo "Built $SCRIPT_DIR/perfmap_workshop_deck.pptx"
