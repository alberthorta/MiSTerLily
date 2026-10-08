# MiSTerLily

A colour status display for a MiSTer FPGA, built on a LILYGO T-Display-S3 (1.9" 320×170 TFT).
It shows the current core (logo and name), like [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled),
and talks the same serial protocol over native USB CDC.

Status: **Phase 3** (display, USB CDC link, protocol parser, core name screen). See `docs/research.md` for the protocol and hardware findings.

## Hardware

- LILYGO T-Display-S3, **non-touch or touch, ESP32-S3R8 with 16 MB flash / 8 MB PSRAM**, 8-bit parallel ST7789.
- A USB-C data cable.

## Build and flash

Needs [PlatformIO](https://platformio.org/) (`brew install platformio`).

```sh
cd firmware
pio run                       # build
pio run -t upload             # build and flash (auto-detects the port)
```

`tdisplay_s3_debug` is the same firmware with a debug log on UART0 (GPIO43 TX, 115200 baud).
The USB port never carries debug output, because the MiSTer scripts read replies from it.

If upload fails: hold **BOOT**, press and release **RST**, release **BOOT**, then upload again.

## USB connection

The board uses the ESP32-S3 built-in USB Serial/JTAG controller (USB ID `303a:1001`,
"USB JTAG/serial debug unit"). No driver is needed.

- macOS: `/dev/cu.usbmodem*`
- Linux / MiSTer: `/dev/ttyACM*` (the MiSTer kernel has `CONFIG_USB_ACM=y`)

The baud rate is ignored. Flashing and resetting work over the same port without buttons.

## Testing on a Mac

```sh
cd firmware && pio test -e native        # protocol unit tests, no board needed
python3 tools/usb_test.py /dev/cu.usbmodemXXXX   # USB checks against the board
```

`tools/` needs only the Python standard library (no pyserial), so it also runs on the MiSTer.

Our own commands use the `MDS` prefix, which no tty2oled command uses:

| Command | Reply |
|---|---|
| `MDSPING` | `MDSPONG` |
| `MDSINFO` | `MDSINFO;fw=...;psram=...;uptime_ms=...;...` (one line) |
| `CMDHWINF` (tty2oled) | `HWTDISPS3;MDS-<version>;` |

Like tty2oled, every received line is answered with `ttyack;` (`CMDSTTYACK,0` turns it off).

## Simulating a MiSTer

`tools/fake_mister.py` sends exactly what the tty2oled daemon sends: its startup sequence, then
`CMDCOR,<core>,-1` + raw picture when a picture exists, else the bare core name.

```sh
python3 tools/fake_mister.py /dev/cu.usbmodemXXXX PSX                       # name only
python3 tools/fake_mister.py /dev/cu.usbmodemXXXX --pics <tty2oled Pictures folder> SNES PSX ATARI5200
python3 tools/fake_mister.py /dev/cu.usbmodemXXXX --loop --interval 4 MENU SNES PSX Saturn
```

Core names are the values MiSTer writes to `/tmp/CORENAME` (`PSX`, `SNES`, `ATARI5200`, `Minimig`, ...).
The screen shows a friendly name for common systems (`PSX` → PLAYSTATION) with the core name small below.
