#!/usr/bin/env python3
"""End-to-end check of the native mode (our daemon) in a Linux container laid out like a MiSTer.

Runs inside the container started by run_e2e.sh. A pty stands in for the display at
/dev/ttyACM0 (config TTYDEV, since a container has no USB sysfs entry). Checks install.sh,
what the daemon sends for each kind of core, reconnection, the sleep file and uninstall.sh.
"""
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, "/src/mister")
from pictures import decode_picture  # noqa: E402

FAT = "/media/fat"
failures = 0


def check(name, ok, detail=""):
    global failures
    print("{} {}{}".format("PASS" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures += 1


class FakeDisplay:
    """A pty whose slave is /dev/ttyACM0; collects everything the daemon writes."""

    def __init__(self):
        self.master, self.slave = os.openpty()
        if os.path.lexists("/dev/ttyACM0"):
            os.remove("/dev/ttyACM0")
        os.symlink(os.ttyname(self.slave), "/dev/ttyACM0")
        self.data = bytearray()
        self.alive = True
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        while self.alive:
            try:
                self.data.extend(os.read(self.master, 65536))
            except OSError:
                time.sleep(0.01)

    def unplug(self):
        os.remove("/dev/ttyACM0")
        self.alive = False


def sh(cmd):
    with open("/tmp/sh.log", "a") as log:
        return subprocess.run(["bash", "-c", cmd], stdout=log, stderr=subprocess.STDOUT).returncode


def set_core(name):
    with open("/tmp/CORENAME", "w") as f:
        f.write(name)
    time.sleep(0.8)


def main():
    os.makedirs(FAT + "/linux", exist_ok=True)
    with open("/tmp/CORENAME", "w") as f:
        f.write("MENU")

    rc = sh("bash /src/mister/install.sh --no-start")
    check("install.sh exit 0", rc == 0, open("/tmp/sh.log").read())
    check("tty2oled daemon disabled", os.path.exists(FAT + "/tty2oled/_S60tty2oled")
          and not os.path.exists(FAT + "/tty2oled/S60tty2oled"))
    sh("bash /src/mister/install.sh --no-start")  # a second install must not add a second line
    startup = open(FAT + "/linux/user-startup.sh").read()
    lines = [l for l in startup.splitlines() if "MiSTerLily/S60misterlily" in l]
    check("boot line added once, also after installing twice", len(lines) == 1, startup)
    cfg = open(FAT + "/MiSTerLily/config.ini").read()
    check("config records native mode", 'MDS_MODE="native"' in cfg and 'MDS_DISABLED_TTY2OLED="yes"' in cfg)
    ini = open(FAT + "/tty2oled/tty2oled-user.ini").read()
    check("tty2oled updater kept off", 'TTY2OLED_UPDATE="no"' in ini)

    # Colour picture for MDSTEST (it also has a tty2oled picture: colour must win).
    subprocess.run(["cp", "/src/tests/fixtures/art/MDSTEST.png", FAT + "/MiSTerLily/art/"], check=True)
    with open(FAT + "/MiSTerLily/config.ini", "a") as f:
        f.write('TTYDEV="/dev/ttyACM0"\n')

    disp = FakeDisplay()
    sh("bash {}/MiSTerLily/S60misterlily start".format(FAT))
    time.sleep(1.5)
    check("daemon running", sh("bash {}/MiSTerLily/S60misterlily status".format(FAT)) == 0)
    for core in ("MDSTEST", "SNES", "Atari5200"):
        set_core(core)
    data = bytes(disp.data)

    check("setup sequence", data.startswith(b"CMDNULL\nCMDSTTYACK,0\nCMDCON,255\nCMDROT,0\n"), repr(data[:60]))
    check("MENU sent at start", b"MDSCORE,MENU\n" in data)
    png = open("/src/tests/fixtures/art/MDSTEST.png", "rb").read()
    m = re.search(rb"MDSIMG,(\d+),([0-9a-f]{8}),png,MDSTEST\n", data)
    check("colour picture wins, sent as MDSIMG", m is not None and int(m.group(1)) == len(png)
          and data[m.end():m.end() + len(png)] == png)
    gsc = decode_picture(FAT + "/tty2oled/pics/GSC/SNES.gsc")
    check("tty2oled picture sent as CMDCOR", b"CMDCOR,SNES,-1\n" + gsc in data)
    check("core without picture sent as MDSCORE", data.endswith(b"MDSCORE,Atari5200\n"), repr(data[-40:]))

    # Unplug, plug in again: the daemon must notice, find it again and resend the current core.
    disp.unplug()
    time.sleep(1.0)
    disp2 = FakeDisplay()
    time.sleep(4.0)
    data2 = bytes(disp2.data)
    check("reconnect: setup sent again", data2.startswith(b"CMDNULL\nCMDSTTYACK,0\n"), repr(data2[:40]))
    check("reconnect: current core sent again", data2.endswith(b"MDSCORE,Atari5200\n"), repr(data2[-40:]))
    log = open("/tmp/mister-display.log").read()
    check("log shows lost and connected twice", "display lost" in log and log.count("display connected") == 2, log)

    # The sleep file (MiSTer SAM) pauses sending.
    open("/tmp/tty2oled_sleep", "w").close()
    time.sleep(0.3)
    before = len(disp2.data)
    set_core("SNES")
    check("sleep file pauses", len(disp2.data) == before)
    os.remove("/tmp/tty2oled_sleep")
    time.sleep(0.8)
    check("resumes after the sleep file", bytes(disp2.data).endswith(gsc))

    rc = sh("bash {}/MiSTerLily/uninstall.sh".format(FAT))
    check("uninstall exit 0", rc == 0)
    check("daemon stopped", sh("bash {}/MiSTerLily/S60misterlily status".format(FAT)) != 0)
    check("tty2oled daemon restored", os.path.exists(FAT + "/tty2oled/S60tty2oled"))
    check("boot line removed", "MiSTerLily" not in open(FAT + "/linux/user-startup.sh").read())
    sh("bash {}/tty2oled/S60tty2oled stop".format(FAT))

    print("{} failure(s)".format(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
