# MiSTerLily

A colour status display for a MiSTer FPGA, built on a LILYGO T-Display-S3 (1.9" 320×170 TFT).
It shows the current core (logo and name), like [MiSTer_tty2oled](https://github.com/venice1200/MiSTer_tty2oled),
and talks the same serial protocol over native USB CDC.

Status: **Phase 1** (display bring-up). See `docs/research.md` for the protocol and hardware findings.

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
