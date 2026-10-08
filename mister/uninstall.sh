#!/bin/bash
# Remove the MiSTerLily block from tty2oled-user.ini and restart tty2oled.
# With --purge, also delete /media/fat/MiSTerLily.
# Test hook: MDS_FAT (default /media/fat).

set -u
FAT="${MDS_FAT:-/media/fat}"
T2O="$FAT/tty2oled"
USER_INI="$T2O/tty2oled-user.ini"
BEGIN_MARK="# >>> MiSTerLily (managed by MiSTerLily/install.sh, remove with uninstall.sh)"
END_MARK="# <<< MiSTerLily"

purge=no
[ "${1:-}" = "--purge" ] && purge=yes

if [ -f "$USER_INI" ]; then
  tmp="$USER_INI.mds.tmp"
  awk -v b="$BEGIN_MARK" -v e="$END_MARK" '
    $0 == b { skip = 1; next }
    $0 == e { skip = 0; next }
    !skip { print }' "$USER_INI" >"$tmp" && mv -f "$tmp" "$USER_INI"
  echo "Removed the MiSTerLily block from $USER_INI"
fi

[ -x "$T2O/S60tty2oled" ] && "$T2O/S60tty2oled" restart

if [ "$purge" = yes ]; then
  rm -rf "$FAT/MiSTerLily"
  echo "Deleted $FAT/MiSTerLily"
fi
