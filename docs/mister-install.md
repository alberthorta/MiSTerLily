# Installing on the MiSTer

Two modes:

- **Native (default)**: our daemon (`mister_display.py`) shows colour pictures from
  `/media/fat/MiSTerLily/art`, falls back to the tty2oled pictures, then to the core name.
  It finds the display itself, and reconnects when it is unplugged and plugged in again.
- **tty2oled compatibility** (`install.sh --tty2oled`): the unmodified tty2oled daemon drives the
  display; we only tell it where the display is. Grey logos only.

> Status: both modes are tested on Linux in a container (the real `tty2oled.sh`; our daemon with
> reconnection), and the daemon is tested against the board from a Mac (including unplug/replug).
> **Not yet tested on a real MiSTer.**

Needs `python3` on the MiSTer (native mode). The MiSTer Linux image has it: `update_all` is a Python
program run directly by the system.

## 1. Protect the board from the tty2oled firmware updater

The tty2oled updater can flash its own (OLED) firmware onto any ESP32 it finds, and it resets the
board while checking. If you use tty2oled pictures (recommended), turn that off **before** its first run:

```sh
mkdir -p /media/fat/tty2oled
echo 'TTY2OLED_UPDATE="no"' >> /media/fat/tty2oled/tty2oled-user.ini
```

The updater only creates `tty2oled-user.ini` when it is missing, so this setting stays.
If a menu titled "Flash tool for ESP devices" ever appears, choose **Exit**.

## 2. Install tty2oled (for its picture library)

From the MiSTer command line (F9) or SSH:

```sh
wget https://raw.githubusercontent.com/venice1200/MiSTer_tty2oled/main/update_tty2oled.sh -O /media/fat/Scripts/update_tty2oled.sh
/media/fat/Scripts/update_tty2oled.sh
```

This downloads the core pictures to `/media/fat/tty2oled/pics`. Run it again later for new pictures;
in native mode it keeps the tty2oled daemon disabled.

Without tty2oled the native mode still works with colour pictures and core names.

## 3. Install MiSTerLily

Plug the display in. Copy the `mister/` folder of this repository to the MiSTer (for example
`scp -r mister root@mister:/media/fat/MiSTerLily`) and run:

```sh
bash /media/fat/MiSTerLily/install.sh              # native mode
bash /media/fat/MiSTerLily/install.sh --tty2oled   # or: tty2oled compatibility mode
```

Native mode:
1. copies the files to `/media/fat/MiSTerLily` (an existing `config.ini` is kept) and creates `art/`;
2. asks every ESP32-S3 serial port `CMDHWINF` and stores the USB serial number of ours in `config.ini`;
3. adds a marked block to `tty2oled-user.ini` (`TTYDEV` and `TTY2OLED_UPDATE="no"`);
4. disables the tty2oled daemon the way tty2oled does it (`S60tty2oled` → `_S60tty2oled`);
5. adds `S60misterlily` to `/media/fat/linux/user-startup.sh` and starts the daemon.

Running `install.sh` again switches modes. `bash /media/fat/MiSTerLily/uninstall.sh` undoes
everything and gives the display back to tty2oled (`--purge` also deletes the folder and your art).

## 4. Colour pictures

Make them on a computer with `tools/convert_asset.py` (any PNG/JPEG → 320×170) and copy them to
`/media/fat/MiSTerLily/art/<core>.png` (or `.jpg`). `<core>` is the name in `/tmp/CORENAME`;
like tty2oled, shorter prefixes also match (`art/Genesis.png` is used for a core named `Genesis_xyz`).
New files are used at the next core change; no restart needed.

## 5. Check it

```sh
bash /media/fat/MiSTerLily/find-display.sh       # lists ACM devices and the chosen one
/media/fat/MiSTerLily/S60misterlily status
echo SNES > /tmp/CORENAME                         # shows SNES now
cat /tmp/mister-display.log                       # what was sent, connections, picture errors
```

Settings are in `/media/fat/MiSTerLily/config.ini`: brightness, rotation, fade, idle sleep (dark or a
dim clock), the menu screen (clock with time/date/IP, or the tty2oled MENU logo), picture folders, a
fixed `TTYDEV`. Restart with `S60misterlily restart` after a change.

The daemon pings the display every 5 s. If the display restarts (watchdog), it sends the core again;
if it stops answering, the daemon opens the port again. If the daemon stops (or MiSTer shuts down
while the display keeps power), the display shows "Waiting for MiSTer" after 15 s.

In tty2oled mode, use tty2oled's own log instead: `debug="true"` in `tty2oled-user.ini`, restart with
`/media/fat/tty2oled/S60tty2oled restart`, read `/tmp/tty2oled`. Brightness there comes from
tty2oled's `CONTRAST` (default 100 ≈ half): add `CONTRAST="255"` to `tty2oled-user.ini`.

## How the device is found

- The board is a USB CDC-ACM device, `303a:1001` (Espressif USB Serial/JTAG). The MiSTer kernel has
  `CONFIG_USB_ACM=y`, so it appears as `/dev/ttyACM<n>`, not `/dev/ttyUSB0` (tty2oled's default).
- `find-display.sh` reads `/sys/class/tty/ttyACM*/device/..` (`idVendor`, `idProduct`, `serial`).
  It does not rely on `/dev/serial/by-id` (that needs udev).
- Every ESP32-S3/C3 board has the same USB IDs. If several are connected, the serial number stored by
  `install.sh` picks ours; `find-display.sh --probe` asks each one `CMDHWINF`.
- If detection fails on the MiSTer, set `TTYDEV="/dev/ttyACM0"` (or the right number) in `config.ini`.

## Not verified yet (needs the MiSTer)

- The sysfs layout and `/dev/ttyACM<n>` number on the MiSTer kernel.
- `python3` version and modules on the current MiSTer image (the daemon uses only `os`, `select`,
  `termios`, `binascii`, `subprocess`, `argparse`).
- tty2oled mode only: `S60tty2oled` waits up to 10 s for `TTYDEV` at boot; a late hub can miss it.
  It also sets the port up only at start, so restart it after replugging the display.
  (Native mode waits and reconnects by itself.)
- Linux `cdc-acm` closing behaviour with tty2oled's one-`echo`-per-message writes (native mode keeps
  the port open, so it is not affected).
