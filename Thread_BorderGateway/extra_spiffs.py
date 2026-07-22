"""Ensure rcp_fw SPIFFS image is present and included in esptool upload."""
Import("env")  # noqa: F821 — PlatformIO
from pathlib import Path
import shutil

project_dir = Path(env["PROJECT_DIR"])  # noqa: F821
build_dir = Path(env.subst("$BUILD_DIR"))  # noqa: F821
tools_bin = project_dir / "tools" / "rcp_fw.bin"
build_bin = build_dir / "rcp_fw.bin"
RCP_FW_OFFSET = "0x2C0000"


def ensure_rcp_bin():
    if not tools_bin.exists():
        print("WARNING: %s missing — run python tools/prepare_rcp_image.py" % tools_bin)
        return None
    build_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(tools_bin, build_bin)
    print(
        "RCP SPIFFS: copied %s -> %s (%d bytes)"
        % (tools_bin, build_bin, build_bin.stat().st_size)
    )
    return build_bin


def add_to_flash_extra(bin_path):
    """Keep FLASH_EXTRA_IMAGES in sync (used by some targets)."""
    existing = list(env.get("FLASH_EXTRA_IMAGES", []))  # noqa: F821
    fixed = []
    found = False
    for off, path in existing:
        if str(off).lower() in (RCP_FW_OFFSET.lower(), "0x2c0000"):
            fixed.append((off, str(bin_path)))
            found = True
        else:
            fixed.append((off, path))
    if not found:
        fixed.append((RCP_FW_OFFSET, str(bin_path)))
    env.Replace(FLASH_EXTRA_IMAGES=fixed)  # noqa: F821


def add_to_uploaderflags(bin_path):
    """
    PlatformIO freezes UPLOADERFLAGS from FLASH_EXTRA_IMAGES before post
    extra_scripts run. Patch UPLOADERFLAGS directly so esptool actually writes
    the RCP partition.
    """
    flags = list(env.get("UPLOADERFLAGS", []))  # noqa: F821
    # Remove any previous rcp offset/path pair
    cleaned = []
    skip_next = False
    for i, f in enumerate(flags):
        if skip_next:
            skip_next = False
            continue
        fl = str(f).lower()
        if fl in (RCP_FW_OFFSET.lower(), "0x2c0000"):
            skip_next = True
            continue
        cleaned.append(f)
    cleaned.extend([RCP_FW_OFFSET, str(bin_path)])
    env.Replace(UPLOADERFLAGS=cleaned)  # noqa: F821
    print("RCP SPIFFS added to UPLOADERFLAGS at %s -> %s" % (RCP_FW_OFFSET, bin_path))


bin_path = ensure_rcp_bin()
if bin_path is not None:
    add_to_flash_extra(bin_path)
    add_to_uploaderflags(bin_path)


def _before_upload(source, target, env):  # noqa: ARG001, F811
    p = ensure_rcp_bin()
    if p is not None:
        add_to_flash_extra(p)
        add_to_uploaderflags(p)


env.AddPreAction(  # noqa: F821
    "upload",
    env.VerboseAction(_before_upload, "Preparing RCP SPIFFS image for upload"),  # noqa: F821
)
