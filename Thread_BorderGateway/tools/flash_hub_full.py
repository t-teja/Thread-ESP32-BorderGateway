#!/usr/bin/env python3
"""Full hub flash including rcp_fw SPIFFS."""
import argparse
import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

HUB = Path(__file__).resolve().parents[1]
BUILD = HUB / ".pio" / "build" / "esp32s3"


def find_esptool():
    home = Path.home() / ".platformio" / "packages"
    hits = list(home.glob("tool-esptoolpy*/esptool.py"))
    if hits:
        return [sys.executable, str(hits[0])]
    # try pio package python
    return [sys.executable, "-m", "esptool"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodem211401")
    ap.add_argument("--baud", type=int, default=921600)
    args = ap.parse_args()

    rcp = BUILD / "rcp_fw.bin"
    tools_rcp = HUB / "tools" / "rcp_fw.bin"
    if not rcp.exists():
        if tools_rcp.exists():
            shutil.copy2(tools_rcp, rcp)
        else:
            print("missing rcp_fw.bin")
            return 1
    elif tools_rcp.exists():
        shutil.copy2(tools_rcp, rcp)

    boot = BUILD / "bootloader.bin"
    if not boot.exists():
        boot = BUILD / "bootloader" / "bootloader.bin"
    parts = [
        ("0x0", boot),
        ("0x8000", BUILD / "partitions.bin"),
        ("0xf000", BUILD / "ota_data_initial.bin"),
        ("0x20000", BUILD / "firmware.bin"),
        ("0x2C0000", rcp),
    ]
    for off, p in parts:
        if not p.exists():
            print("missing", p)
            return 1
        print("flash", off, p, p.stat().st_size)

    esptool = find_esptool()
    cmd = esptool + [
        "--chip", "esp32s3",
        "--port", args.port,
        "--baud", str(args.baud),
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", "-z",
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "8MB",
    ]
    for off, p in parts:
        cmd.extend([off, str(p)])

    print("Running:", " ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
