#!/bin/bash
# Runs the unmodified upstream tty2oled daemon in a Debian container against our install.sh and
# compares its output with tools/fake_mister.py. Needs Docker and network (for the clones).
#
#   bash tests/mister/run_tty2oled_e2e.sh [path to a MiSTer_tty2oled clone] [path to tty2oled Pictures]
#
# Without arguments it clones venice1200/MiSTer_tty2oled and fetches two pictures from
# venice1200/MiSTer_tty2oled_pictures into a temporary folder (they are not stored in this repo).

set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

T2O="${1:-}"
PICS="${2:-}"
if [ -z "$T2O" ]; then
  git clone -q --depth 1 https://github.com/venice1200/MiSTer_tty2oled.git "$WORK/tty2oled"
  T2O="$WORK/tty2oled"
fi
mkdir -p "$WORK/pics/GSC" "$WORK/pics/XBM"
if [ -z "$PICS" ]; then
  base=https://raw.githubusercontent.com/venice1200/MiSTer_tty2oled_pictures/main/Pictures
  curl -fsSL "$base/GSC/SNES.gsc" -o "$WORK/pics/GSC/SNES.gsc"
  curl -fsSL "$base/XBM/PSX.xbm" -o "$WORK/pics/XBM/PSX.xbm"
else
  cp "$PICS/GSC/SNES.gsc" "$WORK/pics/GSC/"
  cp "$PICS/XBM/PSX.xbm" "$WORK/pics/XBM/"
fi
cp "$ROOT/tests/fixtures/pics/GSC/MDSTEST.gsc" "$WORK/pics/GSC/"
echo "upstream tty2oled: $(git -C "$T2O" log -1 --format='%h %cd' 2>/dev/null || echo "$T2O")"

docker run --rm -i \
  -v "$ROOT:/src:ro" -v "$T2O:/upstream:ro" -v "$WORK/pics:/pics:ro" \
  debian:bookworm-slim bash -s <<'EOF'
set -e
apt-get update -qq >/dev/null && apt-get install -y -qq inotify-tools xxd python3 procps dos2unix >/dev/null
mkdir -p /media/fat/tty2oled
cp /upstream/tty2oled.sh /upstream/S60tty2oled /upstream/tty2oled-system.ini /upstream/tty2oled-user.ini /media/fat/tty2oled/
cp -r /pics /media/fat/tty2oled/pics
chmod +x /media/fat/tty2oled/tty2oled.sh /media/fat/tty2oled/S60tty2oled
python3 /src/tests/mister/tty2oled_e2e.py
EOF
