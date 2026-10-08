# Serial protocol implemented by the firmware

Source of truth for tty2oled behaviour: `MiSTer_SSD1322_USB.ino` (BuildVersion 230702) and
`tty2oled.sh` at venice1200/MiSTer_tty2oled `50c08ac`. Details and evidence: `docs/research.md`.
Implementation: `firmware/lib/protocol/` (pure C++, unit-tested with `pio test -e native`).

## Link

USB CDC-ACM (ESP32-S3 USB Serial/JTAG, 303a:1001). Baud rate and line settings are ignored.
The host should set the port raw (`stty ... raw -echo -hupcl`), like tty2oled does.

## Framing

- **Text line**: bytes up to `\n`. A trailing `\r` is removed. Empty lines are ignored.
  Like `Serial.readStringUntil('\n')` with a 500 ms timeout, an unterminated line is also accepted
  after 500 ms without new bytes.
- Lines longer than 255 bytes are dropped up to the next `\n`. Lines with control bytes (< 0x20,
  0x7F) are dropped: they are almost always the tail of a picture cut by a timeout.
- **Picture**: after a line starting with `CMDCOR` or `CMDAPD`, the next bytes are a raw picture
  with **no length header**:
  - 2048 bytes = XBM, 256×64, 1 bpp, 32 bytes/row, least significant bit = leftmost pixel;
  - 8192 bytes = GSC, 256×64, 4 bpp, 128 bytes/row, high nibble = left pixel, 0 black .. 15 white.

  The picture ends at 8192 bytes, or after 500 ms without a byte (then 2048 = XBM, anything else
  = error). If no byte arrives within 1 s of the command line, the picture is "missing".
  tty2oled.sh sleeps 0.2 s between the line and the data.
- Limitation inherited from tty2oled: a command sent less than 500 ms after an XBM picture is read
  as picture data (the result is then an error and the name is shown). tty2oled.sh never does this.

## Replies

- `ttyack;` (no newline) after every line and every picture. `CMDSTTYACK,0` turns it off.
- No `ttyrdy;` at boot (the sketch sends one; nothing reads it, and over USB it would sit in front
  of the next reply).
- Replies the host does not read within 300 ms are dropped, except up to 64 bytes already in the
  USB FIFO (in practice one `ttyack;`). Before a query, send `CMDNULL` and discard what comes back.

## tty2oled commands

| Command | Behaviour here |
|---|---|
| `<core name>` (any other line) | Show the core name (text screen) |
| `CMDCOR,<core>[,t]` + picture | Show the picture (1:1, centered) with the name below; on a missing/bad picture show the name. `t` is accepted and ignored (no transitions yet) |
| `CMDAPD,<core>` + picture | Store the picture and name, do not draw |
| `CMDSPIC[,t]` | Show the stored core screen |
| `CMDSNAM` | Show the name screen |
| `CMDCLS`, `cls` | Clear the screen |
| `CMDCLSWU` | Clear the frame buffer, do not update the screen |
| `CMDDUPD` | Update the screen from the frame buffer |
| `CMDCLST,t,c` | Fill with grey level `c`, forget the stored picture |
| `CMDDOFF` / `CMDDON` | Backlight and panel off / on (content kept) |
| `CMDCON,<0..255>` | Brightness: maps to backlight 40..255 (0 is dim, not black) |
| `CMDROT,<0\|1>` | 1 = turned 180° |
| `CMDTXT,f,c,b,x,y,text` | Text in a 256×64 area centered on the screen; fonts mapped to similar ones; `f >= 100` = buffer only |
| `CMDGEO,g,c,x,y,i,j,k,l` | Figures 1..10 in the same 256×64 area; `g >= 100` = buffer only |
| `CMDSORG`, `sorg` | Start screen |
| `CMDTEST` | Colour bars, 16 greys, 1 px red frame |
| `CMDHWINF` | `HWTDISPS3;MDS-<version>;\n` |
| `CMDRESET` | Restart after 100 ms (the USB port stays; no re-enumeration) |
| `QWERTZ` (also with leading noise), `CMDNULL`, `CMDSETTIME`, `CMDSAVER`, `CMDSWSAVER`, `CMDSECD`, `CMDSHCD`, `CMDSHTIME`, `CMDSSCP`, `CMDBYE`, `bye` | Accepted, nothing visible yet |
| any other `CMD...` | Ignored (the sketch would draw it as a core name) |

`HWTDISPS3` is 9 characters, as `installer.sh` expects (`${BLA:0:9}`), and unknown to its flasher.
The version starts with a letter, so the installer sees it as newer than any tty2oled build and does
not offer an update.

## MiSTerLily commands (prefix `MDS`, unused by tty2oled)

| Command | Reply |
|---|---|
| `MDSPING` | `MDSPONG\n` |
| `MDSINFO` | `MDSINFO;fw=..;board=T-Display-S3;psram=..;fb=psram;uptime_ms=..;reset=..;lines=..;images=..;discards=..;txdrop=..;txstale=..;lastimg=<result>:<bytes>;core=..\n` |
| `MDSCORE,<core>` | Show the core name (no picture transfer) |
| `MDSSHOT` | `MDSSHOT,<w>,<h>,<bytes>\n`, then `ttyack;`, then the frame buffer (RGB565 big-endian, row-major). `tools/screenshot.py` saves it as PNG |

`lastimg` result: 0 = ok, 1 = no data, 2 = bad length.
