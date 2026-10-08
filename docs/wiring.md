# Wiring

The display needs only USB: data and 5 V power come over the same cable. No extra wires.

## Development

```
Mac (or MiSTer) ──USB-C cable──> T-Display-S3 USB-C socket
```

Use a data cable (some USB-C cables are charge-only). On macOS the board appears as
`/dev/cu.usbmodem*`, on Linux as `/dev/ttyACM*`. Flashing and the display protocol use the same port.

## Final installation (inside the MiSTer case)

```
QMTECH MiSTer USB host ──> CH334F USB 2.0 hub (internal) ──┬──> TP-Link T2UB Nano (Wi-Fi/Bluetooth)
                                                            └──> T-Display-S3 (USB-C)
```

- Connect the board through its **USB-C socket** with a short cable or a USB-C plug adapter from the
  hub's downstream port. The board is a normal USB 2.0 full-speed device; the hub needs no driver.
- Power: the board takes its 5 V from USB. Check that the hub port can supply the board together with
  the Wi-Fi/Bluetooth dongle; measure the current with a USB meter before closing the case (not yet
  measured for this project).
- The landscape picture reads correctly with the USB-C socket on the **right** (checked on the board).
  If the board must be mounted the other way round, set `ROTATE="yes"` in `tty2oled-user.ini`
  (tty2oled then sends `CMDROT,1`).
- The BOOT and RST buttons should stay reachable or be left alone; normal flashing does not need them.

## Not covered here

Soldering to the board's USB data pads (instead of using the USB-C socket) is **not** described: the
T-Display-S3 schematic (`schematic/T_Display_S3.pdf` in the LILYGO repository) has not been checked
for this yet.
