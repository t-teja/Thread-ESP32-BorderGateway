Import("env")
from pathlib import Path
import os

# Package SPIFFS image is prebuilt at tools/rcp_fw.bin
# Tell esptool to write it to the rcp_fw partition when uploading.

project_dir = Path(env["PROJECT_DIR"])
rcp_bin = project_dir / "tools" / "rcp_fw.bin"
if not rcp_bin.exists():
    print("WARNING: tools/rcp_fw.bin missing — run tools/prepare_rcp_image.py")
else:
    # Offset from partitions.csv — factory ends at 0x20000+0x2A0000 = 0x2C0000
    # nvs 0x9000, otadata 0xf000, phy 0x11000, factory 0x20000 size 0x2A0000
    # next auto = 0x2C0000
    RCP_FW_OFFSET = 0x2C0000
    env.Append(
        FLASH_EXTRA_IMAGES=[
            ("0x%X" % RCP_FW_OFFSET, str(rcp_bin)),
        ]
    )
    print("RCP SPIFFS image will flash at 0x%X (%s)" % (RCP_FW_OFFSET, rcp_bin))
