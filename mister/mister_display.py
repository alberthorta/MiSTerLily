#!/usr/bin/env python3
"""MiSTerLily daemon: shows the running MiSTer core on the T-Display-S3.

Replaces the tty2oled daemon (and reuses its picture library). For every core change in
/tmp/CORENAME it sends, in this order of preference:
  1. a colour picture  <ART_DIR>/<core>.png|.jpg            -> MDSIMG
  2. a tty2oled picture <TTY2OLED_PICS>/...gsc|.xbm           -> CMDCOR + raw bytes
  3. the name only                                           -> MDSCORE,<core>

It finds the display with find-display.sh, keeps the port open, reads every reply (so nothing
piles up in the device), and when the display disappears it waits for it, sets the port up again
and resends the current core. While /tmp/tty2oled_sleep exists (MiSTer SAM) it sends nothing.

Usage (normally started by S60misterlily):
  python3 mister_display.py [--config FILE] [--device DEV] [--corename FILE] [-v]
"""
import argparse
import os
import subprocess
import sys
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from mdsserial import Port  # noqa: E402
from pictures import asset_frame, decode_picture, find_art, find_picture  # noqa: E402

DEFAULTS = {
    "MDS_SERIAL": "",
    "TTYDEV": "",  # fixed device instead of auto-detection
    "ART_DIR": "/media/fat/MiSTerLily/art",
    "TTY2OLED_PICS": "/media/fat/tty2oled/pics",
    "USE_TTY2OLED_PICS": "yes",
    "USE_GSC_PICTURE": "yes",
    "USE_US_PICTURE": "no",
    "USE_TEXT_PICTURE": "no",
    "CONTRAST": "255",
    "ROTATE": "no",
}
SLEEP_FILE = "/tmp/tty2oled_sleep"
POLL_SECONDS = 0.1
RETRY_SECONDS = 2.0
# A device node must exist this long before we open it: the kernel creates /dev/ttyACMn a moment
# before the port is usable.
SETTLE_SECONDS = 0.5

verbose = False


def log(msg):
    sys.stderr.write("{} {}\n".format(time.strftime("%H:%M:%S"), msg))
    sys.stderr.flush()


def debug(msg):
    if verbose:
        log(msg)


def read_config(path):
    """KEY="value" lines, as in config.ini (also shell-sourceable)."""
    cfg = dict(DEFAULTS)
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                cfg[key.strip()] = value.strip().strip('"').strip("'")
    except OSError:
        pass
    return cfg


def find_device(cfg):
    if cfg["TTYDEV"]:
        return cfg["TTYDEV"] if os.path.exists(cfg["TTYDEV"]) else None
    env = dict(os.environ, MDS_SERIAL=cfg["MDS_SERIAL"])
    try:
        out = subprocess.run(["bash", os.path.join(HERE, "find-display.sh"), "--quiet"], env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10).stdout
    except (OSError, subprocess.SubprocessError):
        return None
    dev = out.decode(errors="replace").strip()
    return dev or None


def read_core(path):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", "replace").strip()
    except OSError:
        return None


def core_stamp(path):
    """Changes whenever MiSTer rewrites the file, even with the same core (like inotify modify)."""
    try:
        st = os.stat(path)
        return (st.st_mtime_ns, st.st_size, st.st_ino)
    except OSError:
        return None


class Display:
    def __init__(self, cfg):
        self.cfg = cfg
        self.port = None
        self.path = None

    def connected(self):
        return self.port is not None

    def connect(self, path):
        self.port = Port(path)
        self.path = path
        log("display connected: {}".format(path))
        self.port.flush_stale()
        # We read every reply ourselves; acks would only add traffic.
        self.port.write("CMDSTTYACK,0\n")
        self.port.write("CMDCON,{}\n".format(self.cfg["CONTRAST"]))
        self.port.write("CMDROT,{}\n".format(1 if self.cfg["ROTATE"] == "yes" else 0))

    def disconnect(self, why):
        if self.port:
            try:
                self.port.fd is not None and os.close(self.port.fd)
            except OSError:
                pass
            self.port.fd = None
            self.port = None
            log("display lost ({})".format(why))

    def still_there(self):
        return self.port is not None and os.path.exists(self.path)

    def drain(self):
        """Reads replies; logs picture errors. Raises OSError if the device went away."""
        data = self.port.read_for(0)
        for line in data.split(b"\n"):
            if line.startswith(b"MDSIMG;err"):
                log("display: " + line.decode(errors="replace"))
            elif line.strip():
                debug("display: {!r}".format(line))

    def show(self, core):
        cfg = self.cfg
        art = find_art(cfg["ART_DIR"], core)
        if art:
            head, data = asset_frame(core, art)
            self.port.write(head + data)
            log("{} -> colour picture {} ({} bytes)".format(core, os.path.basename(art), len(data)))
            return
        if cfg["USE_TTY2OLED_PICS"] == "yes":
            pic = find_picture(cfg["TTY2OLED_PICS"], core, cfg["USE_US_PICTURE"] == "yes",
                               cfg["USE_GSC_PICTURE"] == "yes", cfg["USE_TEXT_PICTURE"] == "yes")
            if pic:
                data = decode_picture(pic)
                # Our firmware switches to binary mode at once, no pause needed like tty2oled.sh.
                self.port.write("CMDCOR,{},-1\n".format(core).encode() + data)
                log("{} -> tty2oled picture {}".format(core, os.path.relpath(pic, cfg["TTY2OLED_PICS"])))
                return
        self.port.write("MDSCORE,{}\n".format(core))
        log("{} -> name only".format(core))


def run(args):
    cfg = read_config(args.config)
    if args.device:
        cfg["TTYDEV"] = args.device
    display = Display(cfg)
    last_stamp = None
    shown = None  # core currently on the display
    next_search = 0.0
    sleeping = False
    log("started (config {}, corename {})".format(args.config, args.corename))

    while True:
        now = time.monotonic()
        if not display.connected():
            if now >= next_search:
                dev = find_device(cfg)
                if dev:
                    time.sleep(SETTLE_SECONDS)
                    try:
                        display.connect(dev)
                        shown = None  # resend the current core
                    except (OSError, termios.error) as e:
                        display.disconnect("open failed: {}".format(e))
                if not display.connected():
                    next_search = now + RETRY_SECONDS
            time.sleep(POLL_SECONDS)
            continue

        try:
            if not display.still_there():
                raise OSError("device node gone")
            display.drain()

            if os.path.exists(SLEEP_FILE):
                if not sleeping:
                    log("sleep file present, pausing")
                    sleeping = True
            else:
                if sleeping:
                    log("sleep file gone, resuming")
                    sleeping = False
                    shown = None
                stamp = core_stamp(args.corename)
                core = read_core(args.corename) if stamp else None
                if core and (stamp != last_stamp or shown is None):
                    display.show(core)
                    shown = core
                    last_stamp = stamp
        except (OSError, termios.error) as e:  # tcdrain() raises termios.error on a dead port
            display.disconnect(str(e))
            next_search = time.monotonic() + RETRY_SECONDS
        time.sleep(POLL_SECONDS)


def main():
    global verbose
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=os.path.join(HERE, "config.ini"))
    ap.add_argument("--device", help="use this device instead of find-display.sh")
    ap.add_argument("--corename", default="/tmp/CORENAME")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    verbose = args.verbose
    try:
        run(args)
    except KeyboardInterrupt:
        log("stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
