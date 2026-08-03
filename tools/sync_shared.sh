#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# sync_shared.sh -- push the canonical shared headers into every sketch folder.
#
# The Arduino IDE copies a sketch folder to a temp dir before building, so a
# sketch cannot #include "../../config/board.h". Each sketch folder therefore
# carries its own copy, and this script is what keeps those copies honest.
# Edit config/board.h and config/config.h, run this, rebuild.
#
#   ./tools/sync_shared.sh
# ---------------------------------------------------------------------------
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BOARD_SRC="config/board.h"
CONFIG_SRC="config/config.h"

# Folders that need board.h (pins / board identity).
BOARD_TARGETS=(
  stage0/s01_display_test
  stage0/s02_touch_test
  stage0/s02e_touch_calibrate
  stage0/s03_wifi_time_heap_test
  stage0/s04_https_fetch_test
  app
)

# Folders that need config.h (credentials / location / TZ).
CONFIG_TARGETS=(
  stage0/s03_wifi_time_heap_test
  stage0/s04_https_fetch_test
  app
)

echo "syncing from $ROOT"

for dir in "${BOARD_TARGETS[@]}"; do
  if [ -d "$dir" ]; then
    cp "$BOARD_SRC" "$dir/board.h"
    echo "  board.h  -> $dir/"
  fi
done

# Guard: never overwrite working credentials with the placeholder file. That
# would silently break the WiFi tests and look like a hardware problem.
# Match the #define VALUES only -- a mention of the token in a comment must not
# trip the guard, or a correctly filled-in config never syncs.
if grep -qE '^[[:space:]]*#define.*"CHANGE_ME"' "$CONFIG_SRC"; then
  echo
  echo "  SKIPPED config.h: $CONFIG_SRC still contains CHANGE_ME."
  echo "  Fill in the real values there (it is git-ignored), then rerun."
else
  for dir in "${CONFIG_TARGETS[@]}"; do
    if [ -d "$dir" ]; then
      cp "$CONFIG_SRC" "$dir/config.h"
      echo "  config.h -> $dir/"
    fi
  done
fi

echo "done."
