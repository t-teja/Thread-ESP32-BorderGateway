"""Package RCP SPIFFS into the build and ensure PlatformIO upload flashes it."""
Import("env")  # noqa: F821 — PlatformIO
from pathlib import Path
import shutil
import os

project_dir = Path(env["PROJECT_DIR"])  # noqa: F821
build_dir = Path(env.subst("$BUILD_DIR"))  # noqa: F821
tools_bin = project_dir / "tools" / "rcp_fw.bin"
build_bin = build_dir / "rcp_fw.bin"

RCP_FW_OFFSET = "0x2C0000"

def ensure_rcp_bin():
    if not tools_bin.exists():
        print("WARNING: %s missing — run: python tools/prepare_rcp_image.py" % tools_bin)
        return None
    build_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(tools_bin, build_bin)
    print("RCP SPIFFS: copied %s -> %s (%d bytes)" % (tools_bin, build_bin, build_bin.stat().st_size))
    return build_bin

# Copy early so cmake flasher_args relative path also works if generation is empty
bin_path = ensure_rcp_bin()

if bin_path is not None:
    # Avoid duplicate entries if script re-runs
    existing = env.get("FLASH_EXTRA_IMAGES", [])  # noqa: F821
    already = any(
        str(img[0]).lower() == RCP_FW_OFFSET.lower() or str(img[0]).lower() == "0x2c0000"
        for img in existing
    )
    if not already:
        env.Append(FLASH_EXTRA_IMAGES=[(RCP_FW_OFFSET, str(bin_path))])  # noqa: F821
        print("RCP SPIFFS will upload at %s" % RCP_FW_OFFSET)
    else:
        # Replace path with absolute existing image if prior entry pointed at missing file
        fixed = []
        for off, path in existing:
            if str(off).lower() in (RCP_FW_OFFSET.lower(), "0x2c0000"):
                fixed.append((off, str(bin_path)))
            else:
                fixed.append((off, path))
        env.Replace(FLASH_EXTRA_IMAGES=fixed)  # noqa: F821
        print("RCP SPIFFS upload path refreshed at %s" % RCP_FW_OFFSET)

# Also hook before upload to re-copy (in case clean wiped build dir)
def _before_upload(source, target, env):  # noqa: ARG001
    ensure_rcp_bin()

env.AddPreAction("upload", env.VerboseAction(_before_upload, "Preparing RCP SPIFFS image for upload"))  # noqa: F821
