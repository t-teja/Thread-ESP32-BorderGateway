#!/usr/bin/env python3
"""Rebuild RCP image packaging for hub SPIFFS (run after Thread_RCP build)."""
import os, shutil, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RCP_BUILD = ROOT / "Thread_RCP" / ".pio" / "build" / "esp32h2"
HUB = ROOT / "Thread_BorderGateway"
STAGING = HUB / "rcp_build_dir"
TOOLS = HUB / "tools"
CREATE = TOOLS / "create_ota_image.py"

def main():
    if not RCP_BUILD.exists():
        print("Build Thread_RCP first")
        sys.exit(1)
    STAGING.mkdir(parents=True, exist_ok=True)
    (STAGING / "rcp_version").write_text("thread-hub-rcp-0.4.0\n")
    (STAGING / "flash_args").write_text(
        "--flash_mode dio --flash_freq 48m --flash_size 2MB\n"
        "0x0 bootloader/bootloader.bin\n"
        "0x8000 partition_table/partition-table.bin\n"
        "0x10000 esp_ot_rcp.bin\n"
    )
    (STAGING / "bootloader").mkdir(exist_ok=True)
    boot = RCP_BUILD / "bootloader" / "bootloader.bin"
    if not boot.exists():
        boot = RCP_BUILD / "bootloader.bin"
    shutil.copy2(boot, STAGING / "bootloader" / "bootloader.bin")
    (STAGING / "partition_table").mkdir(exist_ok=True)
    pt = RCP_BUILD / "partition_table" / "partition-table.bin"
    if not pt.exists():
        pt = RCP_BUILD / "partitions.bin"
    shutil.copy2(pt, STAGING / "partition_table" / "partition-table.bin")
    fw = RCP_BUILD / "Thread_RCP.bin"
    if not fw.exists():
        fw = RCP_BUILD / "firmware.bin"
    shutil.copy2(fw, STAGING / "esp_ot_rcp.bin")

    out = TOOLS / "spiffs_image" / "ot_rcp_0" / "rcp_image"
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.check_call([sys.executable, str(CREATE),
        "--rcp-build-dir", str(STAGING),
        "--target-file", str(out)])

    # Find spiffsgen
    home = Path.home() / ".platformio" / "packages"
    gens = list(home.glob("framework-espidf*/components/spiffs/spiffsgen.py"))
    if not gens:
        print("spiffsgen missing")
        sys.exit(1)
    bin_out = TOOLS / "rcp_fw.bin"
    subprocess.check_call([sys.executable, str(gens[0]), "0xA0000", str(TOOLS / "spiffs_image"), str(bin_out)])
    print("Wrote", bin_out, bin_out.stat().st_size)

if __name__ == "__main__":
    main()
