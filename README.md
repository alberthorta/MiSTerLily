# MiSTerLily

A colour status display for a MiSTer FPGA, built on a LILYGO T-Display-S3 (1.9" 320×170 TFT).
When MiSTer loads a core, the display shows that system's logo and name.

It speaks the serial protocol of [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled),
so the unmodified tty2oled daemon and its picture library drive it over native USB.

Status: **Phase 4** — tty2oled compatibility works end to end on the bench (board + real tty2oled
daemon in a Linux container). Not yet tested on a real MiSTer.

## Hardware

- **LILYGO T-Display-S3**, ESP32-S3R8 (16 MB flash, 8 MB PSRAM), ST7789 170×320 on the 8-bit
  parallel bus. Touch or non-touch version. Not the T-Display (ESP32) or T-Display-S3 AMOLED.
- A USB-C data cable. In the final build: a free port on the MiSTer's internal USB hub
  (see [docs/wiring.md](docs/wiring.md)).

## Build and flash

Needs [PlatformIO](https://platformio.org/) (`brew install platformio`).

```sh
cd firmware
pio run                       # build
pio run -t upload             # build and flash (auto-detects the port)
pio test -e native            # protocol unit tests on the computer, no board needed
```

No buttons are needed to flash. If an upload fails: hold **BOOT**, press and release **RST**,
release **BOOT**, upload again.

`tdisplay_s3_debug` is the same firmware with a debug log on UART0 (GPIO43 TX, 115200 baud). The USB
port never carries debug output, because the MiSTer scripts read replies from it.

## USB connection

The board uses the ESP32-S3 built-in USB Serial/JTAG controller: USB ID `303a:1001`,
"USB JTAG/serial debug unit", serial number = the chip's MAC address. No driver needed.

- macOS: `/dev/cu.usbmodem*`
- Linux / MiSTer: `/dev/ttyACM*` (MiSTer kernel: `CONFIG_USB_ACM=y`)

The baud rate is ignored. The firmware polls the USB FIFO directly, so a busy display makes the host
wait instead of losing data. Why not Arduino `Serial`: see [docs/research.md](docs/research.md) §6.

## Testing on a Mac (no MiSTer needed)

All tools use only the Python standard library (no pyserial); they also run on the MiSTer.

```sh
python3 tools/usb_test.py   /dev/cu.usbmodemXXXX            # USB link checks
python3 tools/fake_mister.py /dev/cu.usbmodemXXXX PSX SNES  # behave like the tty2oled daemon
python3 tools/fake_mister.py /dev/cu.usbmodemXXXX --pics <tty2oled Pictures folder> --loop --interval 4 SNES PSX ATARI5200
python3 tools/send.py       /dev/cu.usbmodemXXXX CMDTEST "CMDCON,255"
python3 tools/screenshot.py /dev/cu.usbmodemXXXX screen.png # what the display shows, as PNG
```

`fake_mister.py` sends exactly what `tty2oled.sh` sends (checked byte for byte against the real
daemon, see below). Pictures: clone
[MiSTer_tty2oled_pictures](https://github.com/venice1200/MiSTer_tty2oled_pictures) and pass its
`Pictures` folder. Core names are the values MiSTer writes to `/tmp/CORENAME` (`PSX`, `SNES`,
`ATARI5200`, `Minimig`, ...).

Other tests:

```sh
bash tests/mister/test_find_display.sh                     # find-display.sh against a fake sysfs
bash tests/mister/run_tty2oled_e2e.sh                      # real tty2oled.sh in Docker vs fake_mister.py
python3 tools/fake_mister.py --selftest <Pictures folder>  # picture decoding vs tail | xxd
python3 tests/fixtures/make_fixtures.py                    # rebuild the parser test fixtures
```

## Installing on the MiSTer

See [docs/mister-install.md](docs/mister-install.md). In short: set `TTY2OLED_UPDATE="no"`, install
tty2oled with its updater, copy `mister/` to `/media/fat/MiSTerLily` and run `install.sh`.

## What the screen shows

- A core with a tty2oled picture: the picture (256×64, 1:1, centered) and the system name below.
- A core without a picture, or a failed transfer: the system name, large, in the Commodore 64 font.
- Common systems get a friendly name (`PSX` → PLAYSTATION) with the MiSTer core name small below.
- Start-up: "MiSTer / T-Display-S3" and the firmware version.

## tty2oled compatibility

| | Status |
|---|---|
| Core change with picture (`CMDCOR` + XBM/GSC) | works; transitions are ignored |
| Core change without picture (bare name) | works |
| Startup commands (`QWERTZ`, `CMDCON`, `CMDSETTIME`, `CMDSAVER`) | accepted; `CMDCON` sets brightness |
| `CMDROT` (`ROTATE="yes"`), `CMDDON`/`CMDDOFF`, `CMDCLS`, `CMDTXT`, `CMDGEO`, `CMDSPIC`, `CMDAPD`, `CMDTEST`, `CMDHWINF`, `CMDRESET` | work |
| Screensaver, clock, transitions, d.ti board commands | not implemented (accepted, ignored) |
| tty2oled firmware updater | must be off (`TTY2OLED_UPDATE="no"`); our board answers `HWTDISPS3` so the updater does not recognise it |

Full command list: [docs/protocol.md](docs/protocol.md).

## Known limitations

- Not tested on a MiSTer yet (the QMTECH board has not arrived). Open points are listed at the end
  of [docs/mister-install.md](docs/mister-install.md).
- tty2oled pictures are monochrome/greyscale and 256×64; they are not scaled up (1.25× would make
  pixel logos uneven). Native colour artwork is Phase 5.
- If nobody reads the replies, one `ttyack;` stays in the USB FIFO and reaches the next reader first.
- After unplugging/replugging the display, restart the tty2oled daemon (it sets the port up only at
  start).

## Licence and credits

The serial protocol comes from MiSTer_tty2oled by venice1200 and ojaksch (GPL-3.0). This project
re-implements it; no tty2oled code or pictures are included. The pictures stay on the MiSTer and are
downloaded by the tty2oled updater.
