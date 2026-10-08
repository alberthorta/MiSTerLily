#!/usr/bin/env python3
"""Pretend to be a MiSTer running tty2oled.sh, to test the display without a MiSTer.

Sends exactly what tty2oled.sh (venice1200/MiSTer_tty2oled 50c08ac) writes:

  startup:   QWERTZ, CMDCON,<contrast>, CMDSETTIME,<epoch>, CMDSAVER,0,0,0  (0.2 s apart)
  per core:  CMDCOR,<core>,<transition> + 0.2 s + raw picture bytes    if a picture is found
             <core>                                                    otherwise

Picture lookup follows senddata(): <pics>/pics_pri/<core>.gsc|.xbm, then the folders
GSC_US XBM_US GSC XBM XBM_TEXT under <pics>, trying the full name and then shorter prefixes.
Picture files are decoded like `tail -n +4 file | xxd -r -p`.

With --art DIR, a colour picture <DIR>/<core>.png or .jpg (same prefix rule) is tried first and
sent with the MiSTerLily extension: MDSIMG,<bytes>,<crc32>,<png|jpg>,<core> + the file.

Examples:
  python3 tools/fake_mister.py /dev/cu.usbmodem1101 PSX
  python3 tools/fake_mister.py /dev/cu.usbmodem1101 --pics ~/tty2oled_pictures/Pictures SNES PSX ATARI5200 --interval 3
  python3 tools/fake_mister.py /dev/cu.usbmodem1101 --loop --interval 5 MENU SNES PSX
  python3 tools/fake_mister.py --selftest tests/fixtures/pics
"""
import argparse
import os
import re
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

WAITSECS = 0.2  # tty2oled-system.ini
PICTURE_FOLDERS = ["GSC_US", "XBM_US", "GSC", "XBM", "XBM_TEXT"]
HEX_TOKEN = re.compile(rb"0[xX]([0-9a-fA-F]{2})")


def decode_picture(path):
    """Bytes that `tail -n +4 path | xxd -r -p` would produce for a tty2oled picture file."""
    with open(path, "rb") as f:
        lines = f.read().split(b"\n")
    body = b"\n".join(lines[3:])
    return bytes(int(h, 16) for h in HEX_TOKEN.findall(body))


def find_picture(pics, core, use_us=False, use_gsc=True, use_text=False):
    if not pics:
        return None
    pri = os.path.join(pics, "pics_pri")
    for ext in ("gsc", "xbm"):
        p = os.path.join(pri, "{}.{}".format(core, ext))
        if os.path.exists(p):
            return p
    folders = list(PICTURE_FOLDERS)
    if not use_us:
        folders = [f for f in folders if not f.endswith("_US")]
    if not use_gsc:
        folders = [f for f in folders if not f.startswith("GSC")]
    if not use_text:
        folders = [f for f in folders if f != "XBM_TEXT"]
    for folder in folders:
        ext = folder[:3].lower()
        for c in range(len(core), 0, -1):  # full name, then shorter prefixes
            p = os.path.join(pics, folder, "{}.{}".format(core[:c], ext))
            if os.path.exists(p):
                return p
    return None


def find_art(art, core):
    """Native colour picture for `core` in `art`: full name first, then shorter prefixes."""
    if not art:
        return None
    for c in range(len(core), 0, -1):
        for ext in ("png", "jpg"):
            p = os.path.join(art, "{}.{}".format(core[:c], ext))
            if os.path.exists(p):
                return p
    return None


def asset_frame(core, path):
    """MDSIMG header line + file bytes."""
    with open(path, "rb") as f:
        data = f.read()
    fmt = "jpg" if path.lower().endswith((".jpg", ".jpeg")) else "png"
    head = "MDSIMG,{},{:08x},{},{}\n".format(len(data), zlib.crc32(data) & 0xFFFFFFFF, fmt, core)
    return head.encode(), data


def local_epoch():
    # tty2oled.sh: date '-d now <utc offset> hour' +%s, i.e. local wall time as epoch seconds.
    return int(time.time() + time.localtime().tm_gmtoff)


class Sender:
    def __init__(self, port, verbose):
        self.port = port
        self.verbose = verbose

    def line(self, text):
        if self.verbose:
            print(">> " + text)
        self.port.write(text + "\n")

    def raw(self, data, label):
        if self.verbose:
            print(">> [{} bytes {}]".format(len(data), label))
        self.port.write(data)


def startup(s, contrast):
    s.line("QWERTZ")
    time.sleep(WAITSECS)
    s.line("CMDCON,{}".format(contrast))
    time.sleep(WAITSECS)
    s.line("CMDSETTIME,{}".format(local_epoch()))
    time.sleep(WAITSECS)
    s.line("CMDSAVER,0,0,0")
    time.sleep(WAITSECS)


def send_core(s, core, pics, transition, opts):
    art = find_art(opts.art, core)
    if art:
        head, data = asset_frame(core, art)
        s.raw(head + data, "MDSIMG " + os.path.basename(art))
        return
    pic = find_picture(pics, core, opts.us, not opts.no_gsc, opts.text)
    if pic:
        data = decode_picture(pic)
        s.line("CMDCOR,{},{}".format(core, transition))
        time.sleep(WAITSECS)
        s.raw(data, os.path.relpath(pic, pics))
    else:
        s.line(core)


def selftest(pics):
    """Checks decode_picture() against the real tail | xxd pipeline for every picture under pics."""
    count = 0
    for root, _dirs, files in os.walk(pics):
        for name in sorted(files):
            if not name.endswith((".gsc", ".xbm")):
                continue
            path = os.path.join(root, name)
            ours = decode_picture(path)
            ref = subprocess.run(["bash", "-c", 'tail -n +4 "$1" | xxd -r -p', "_", path],
                                 check=True, stdout=subprocess.PIPE).stdout
            if ours != ref:
                print("MISMATCH {} ({} vs {} bytes)".format(path, len(ours), len(ref)))
                return 1
            if len(ours) not in (2048, 8192):
                print("WARNING {} decodes to {} bytes".format(path, len(ours)))
            count += 1
    print("selftest ok: {} picture(s) decode exactly like tail | xxd".format(count))
    return 0 if count else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("device", nargs="?", help="serial device, e.g. /dev/cu.usbmodem1101 or /dev/ttyACM0")
    ap.add_argument("cores", nargs="*", help="core names as MiSTer writes them to /tmp/CORENAME")
    ap.add_argument("--pics", help="tty2oled picture folder (contains GSC/, XBM/, ...)")
    ap.add_argument("--art", help="colour pictures folder (<core>.png / <core>.jpg), tried first")
    ap.add_argument("--contrast", type=int, default=100, help="CMDCON value (default 100, as tty2oled)")
    ap.add_argument("--transition", type=int, default=-1, help="CMDCOR transition (default -1)")
    ap.add_argument("--interval", type=float, default=3.0, help="seconds between cores")
    ap.add_argument("--loop", action="store_true", help="repeat the core list until Ctrl-C")
    ap.add_argument("--no-startup", action="store_true", help="skip the daemon startup sequence")
    ap.add_argument("--us", action="store_true", help="also use *_US folders (USE_US_PICTURE=yes)")
    ap.add_argument("--no-gsc", action="store_true", help="skip GSC folders (USE_GSC_PICTURE=no)")
    ap.add_argument("--text", action="store_true", help="also use XBM_TEXT (USE_TEXT_PICTURE=yes)")
    ap.add_argument("--selftest", metavar="PICS", help="compare the decoder with tail | xxd, then exit")
    ap.add_argument("-q", "--quiet", action="store_true")
    opts = ap.parse_intermixed_args()

    if opts.selftest:
        return selftest(opts.selftest)
    if not opts.device:
        ap.error("device is required")

    from mdsserial import Port

    pics = os.path.expanduser(opts.pics) if opts.pics else None
    with Port(opts.device) as port:
        s = Sender(port, not opts.quiet)
        if not opts.no_startup:
            startup(s, opts.contrast)
        try:
            while True:
                for i, core in enumerate(opts.cores):
                    send_core(s, core, pics, opts.transition, opts)
                    if opts.loop or i < len(opts.cores) - 1:
                        time.sleep(opts.interval)
                if not opts.loop:
                    break
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
