# PlatformIO pre: script — unique application .bin basename per build
#
# Together with [platformio] build_dir = .pio/build-fw (see platformio.ini), this avoids
# WinError 32 when Windows, AV, or IDE holds bootloader.bin / partitions.bin / firmware.bin.
# Each run writes e.g. fw_<pid>_<ms>.bin; upload still uses that build's PROGNAME.

Import("env")

import os
import time

pid = os.getpid()
ms = int(time.time() * 1000) & 0xFFFFFF
env.Replace(PROGNAME="fw_%x_%x" % (pid, ms))
