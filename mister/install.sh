#!/bin/bash
# Set up MiSTerLily on a MiSTer that already runs tty2oled (venice1200/MiSTer_tty2oled).
#
# Phase 4 uses the upstream tty2oled daemon unchanged. This script only:
#   1. copies find-display.sh and config.ini to /media/fat/MiSTerLily,
#   2. asks the connected display who it is (CMDHWINF) and stores its USB serial number,
#   3. adds a marked block at the end of /media/fat/tty2oled/tty2oled-user.ini:
#        TTYDEV = whatever find-display.sh finds each time tty2oled starts (/dev/ttyACM0 if nothing)
#        TTY2OLED_UPDATE="no"  (the tty2oled updater must never flash its own firmware onto this board)
#   4. restarts the tty2oled daemon.
#
# tty2oled's own updater creates tty2oled-user.ini only when it is missing, so the block survives
# tty2oled updates. Run uninstall.sh to remove it.
#
# Usage: install.sh [--no-restart]
# Test hook: MDS_FAT (default /media/fat).

set -u
FAT="${MDS_FAT:-/media/fat}"
SRC="$(cd "$(dirname "$0")" && pwd)"
DEST="$FAT/MiSTerLily"
T2O="$FAT/tty2oled"
USER_INI="$T2O/tty2oled-user.ini"
BEGIN_MARK="# >>> MiSTerLily (managed by MiSTerLily/install.sh, remove with uninstall.sh)"
END_MARK="# <<< MiSTerLily"

restart=yes
for arg in "$@"; do
  case "$arg" in
    --no-restart) restart=no ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

if [ ! -d "$T2O" ]; then
  echo "tty2oled is not installed ($T2O missing)."
  echo "Install it first with its updater (see docs/mister-install.md), with the display UNPLUGGED."
  exit 1
fi

# 1. Files
mkdir -p "$DEST"
if [ "$SRC" != "$DEST" ]; then
  cp -f "$SRC/find-display.sh" "$SRC/uninstall.sh" "$DEST/"
  [ -e "$DEST/config.ini" ] || cp -f "$SRC/config.ini" "$DEST/"
fi
chmod +x "$DEST/find-display.sh" "$DEST/uninstall.sh" 2>/dev/null || true  # FAT may refuse; bash runs them anyway

# 2. Identify the display. With a restart coming, stop the daemon so it does not write while we ask
# (an idle daemon only writes when /tmp/CORENAME changes, so --no-restart leaves it running).
[ "$restart" = yes ] && [ -x "$T2O/S60tty2oled" ] && "$T2O/S60tty2oled" stop >/dev/null 2>&1
echo "Looking for the display..."
dev="$(MDS_CONFIG=/dev/null bash "$DEST/find-display.sh" --probe)"
if [ -n "$dev" ]; then
  serial="$(MDS_CONFIG=/dev/null bash "$DEST/find-display.sh" --probe --quiet --serial)"
  echo "Found $dev (USB serial $serial)."
  tmp="$DEST/config.ini.tmp"
  grep -v '^MDS_SERIAL=' "$DEST/config.ini" 2>/dev/null >"$tmp"
  echo "MDS_SERIAL=\"$serial\"" >>"$tmp"
  mv -f "$tmp" "$DEST/config.ini"
else
  echo "No MiSTerLily display answered. Continuing: tty2oled will use the first ESP32-S3"
  echo "serial port it finds, or /dev/ttyACM0. Plug the display in and run install.sh again to pin it."
fi

# 3. tty2oled-user.ini block (replace an old one, keep everything else)
touch "$USER_INI"
tmp="$USER_INI.mds.tmp"
awk -v b="$BEGIN_MARK" -v e="$END_MARK" '
  $0 == b { skip = 1; next }
  $0 == e { skip = 0; next }
  !skip { print }' "$USER_INI" >"$tmp"
cat >>"$tmp" <<EOF
$BEGIN_MARK
TTYDEV="\$(bash $DEST/find-display.sh --quiet 2>/dev/null || echo /dev/ttyACM0)"
TTY2OLED_UPDATE="no"
$END_MARK
EOF
mv -f "$tmp" "$USER_INI"
echo "Updated $USER_INI"

# 4. Restart
if [ "$restart" = yes ] && [ -x "$T2O/S60tty2oled" ]; then
  "$T2O/S60tty2oled" start
fi
echo "Done. Test with: echo SNES > /tmp/CORENAME"
