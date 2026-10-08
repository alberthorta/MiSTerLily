# Installing on the MiSTer (tty2oled compatibility mode)

MiSTerLily works with the **unmodified tty2oled daemon** (venice1200/MiSTer_tty2oled). tty2oled
watches `/tmp/CORENAME` and sends the core name and its picture over the serial port; our firmware
understands that protocol. Our scripts only tell tty2oled where the display is.

> Status: tested on Linux in a container with the real `tty2oled.sh` (byte-for-byte identical to
> `tools/fake_mister.py`, which is tested against the board). **Not yet tested on a real MiSTer.**

## 1. Protect the board from the tty2oled firmware updater

The tty2oled updater can flash its own (OLED) firmware onto any ESP32 it finds on `TTYDEV`, and it
resets the board while checking. Turn that off **before** the first run:

```sh
mkdir -p /media/fat/tty2oled
echo 'TTY2OLED_UPDATE="no"' >> /media/fat/tty2oled/tty2oled-user.ini
```

The updater only creates `tty2oled-user.ini` when it is missing, so this setting stays.
If a menu titled "Flash tool for ESP devices" ever appears, choose **Exit**.

## 2. Install tty2oled

From the MiSTer command line (F9) or SSH:

```sh
wget https://raw.githubusercontent.com/venice1200/MiSTer_tty2oled/main/update_tty2oled.sh -O /media/fat/Scripts/update_tty2oled.sh
/media/fat/Scripts/update_tty2oled.sh
```

This installs the daemon, the init script, the start line in `/media/fat/linux/user-startup.sh`,
and the core pictures (`/media/fat/tty2oled/pics`). Run it again later to get new pictures.

## 3. Install MiSTerLily

Plug the display in. Copy the `mister/` folder of this repository to the MiSTer (for example with
`scp -r mister root@mister:/media/fat/MiSTerLily`) and run:

```sh
bash /media/fat/MiSTerLily/install.sh
```

It:
1. asks every ESP32-S3 serial port `CMDHWINF` and stores the USB serial number of the one that
   answers `HWTDISPS3` in `/media/fat/MiSTerLily/config.ini`;
2. adds this block at the end of `/media/fat/tty2oled/tty2oled-user.ini`:
   ```sh
   # >>> MiSTerLily (managed by MiSTerLily/install.sh, remove with uninstall.sh)
   TTYDEV="$(bash /media/fat/MiSTerLily/find-display.sh --quiet 2>/dev/null || echo /dev/ttyACM0)"
   TTY2OLED_UPDATE="no"
   # <<< MiSTerLily
   ```
   Every tty2oled script sources this file, so `TTYDEV` is looked up again each time (the
   `/dev/ttyACM<n>` number can change). `find-display.sh --quiet` only reads sysfs; it never talks to
   the device. If it finds nothing, `/dev/ttyACM0` is used;
3. restarts the tty2oled daemon.

`bash /media/fat/MiSTerLily/uninstall.sh` removes the block (`--purge` also deletes the folder).
The `TTY2OLED_UPDATE="no"` line from step 1 stays on purpose: keep it while the board is attached.

## Brightness

tty2oled sends its `CONTRAST` setting (default 100, made for OLEDs) as `CMDCON` when the daemon
starts. The display maps 0..255 to backlight 40..255, so the default gives about half brightness.
For full brightness add `CONTRAST="255"` to `tty2oled-user.ini` (above the MiSTerLily block).

## 4. Check it

```sh
bash /media/fat/MiSTerLily/find-display.sh      # lists ACM devices and the chosen one
echo SNES > /tmp/CORENAME                         # the daemon sends SNES now
```

Then load cores from the MiSTer menu. For a log, set `debug="true"` in `tty2oled-user.ini`, restart
with `/media/fat/tty2oled/S60tty2oled restart`, and read `/tmp/tty2oled`.

## How the device is found

- The board is a USB CDC-ACM device, `303a:1001` (Espressif USB Serial/JTAG). The MiSTer kernel has
  `CONFIG_USB_ACM=y`, so it appears as `/dev/ttyACM<n>`, not `/dev/ttyUSB0` (tty2oled's default).
- `find-display.sh` reads `/sys/class/tty/ttyACM*/device/..` (`idVendor`, `idProduct`, `serial`).
  It does not rely on `/dev/serial/by-id` (that needs udev).
- Every ESP32-S3/C3 board has the same USB IDs. If several are connected, the serial number stored by
  `install.sh` picks ours; `find-display.sh --probe` asks each one `CMDHWINF`.

## Not verified yet (needs the MiSTer)

- The sysfs layout and `/dev/ttyACM<n>` number on the MiSTer kernel.
- Boot order: `S60tty2oled` waits up to 10 s for `TTYDEV`. If the hub enumerates the display later
  than that, the daemon does not start; run `S60tty2oled start` again.
- Linux `cdc-acm` closing behaviour with tty2oled's one-`echo`-per-message writes (on macOS, closing
  right after writing can drop data; Linux should wait for the data to drain).
- After unplugging and replugging the display, the new `/dev/ttyACM<n>` has default terminal settings
  (tty2oled runs `stty` only when the daemon starts). Restart the daemon after replugging.
