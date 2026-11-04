# PlatformIO pre: script — unique application .bin basename per build
#
# Together with [platformio] build_dir = .pio/build-fw (see platformio.ini), this avoids
# WinError 32 when Windows, AV, or IDE holds bootloader.bin / partitions.bin / firmware.bin.
# Each run writes e.g. fw_<pid>_<ms>.bin; upload still uses that build's PROGNAME.

Import("env")

import os
import subprocess
import time

pid = os.getpid()
ms = int(time.time() * 1000) & 0xFFFFFF
env.Replace(PROGNAME="fw_%x_%x" % (pid, ms))

# esptool draws its progress bar with block characters. When pio output is piped on Windows,
# pio cannot print them and dies mid-flash, which leaves esptool blocked and the board black.
if os.name == "nt":
    env["ENV"]["PYTHONIOENCODING"] = "ascii:replace"


def git_output(*args):
    return subprocess.check_output(["git", "-C", env["PROJECT_DIR"], *args], text=True).strip()


# Identify the exact source on the About screen: FW_VERSION alone hides unreleased changes.
try:
    build_id = git_output("rev-parse", "--short", "HEAD")
    if git_output("status", "--porcelain"):
        build_id += "-dirty"
except (OSError, subprocess.CalledProcessError):
    build_id = "local"
env.Append(CPPDEFINES=[("FW_BUILD_ID", env.StringifyMacro(build_id))])
