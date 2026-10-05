#!/usr/bin/env python3
"""Package PlatformIO builds for DIY Flasher."""

import configparser
import json
import os
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"


def version_from_head():
    try:
        tags = subprocess.check_output(
            ["git", "tag", "--points-at", "HEAD", "--sort=-version:refname"],
            cwd=ROOT,
            text=True,
        ).splitlines()
    except (OSError, subprocess.CalledProcessError):
        tags = []
    return tags[0] if tags else "dev"


def package():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini")
    boards = [section.removeprefix("env:") for section in config.sections()
              if section.startswith("env:")]
    if not boards:
        raise RuntimeError("No PlatformIO environments found")

    version = version_from_head()
    safe_version = re.sub(r"[^A-Za-z0-9._-]", "_", version)
    core_dir = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    ota_image = core_dir / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    manifest = []

    DIST.mkdir(exist_ok=True)
    for board in boards:
        if not re.fullmatch(r"[A-Za-z0-9._-]+", board):
            raise ValueError(f"Unsafe board name: {board}")
        if board != "lolin_s2_mini":
            raise ValueError(f"Flash layout and settings are not defined for {board}")
        build_dir = ROOT / ".pio" / "build" / board
        folder = f"{safe_version}_{board}"
        output_dir = DIST / folder
        output_dir.mkdir(exist_ok=True)
        images = [
            ("0x1000", build_dir / "bootloader.bin", "bootloader.bin"),
            ("0x8000", build_dir / "partitions.bin", "partitions.bin"),
            ("0xE000", ota_image, "boot_app0.bin"),
            ("0x10000", build_dir / "firmware.bin", "firmware.bin"),
        ]
        files = []
        for address, source, name in images:
            if not source.is_file():
                raise FileNotFoundError(f"Required flash image missing: {source}")
            shutil.copyfile(source, output_dir / name)
            files.append({"address": address, "url": f"{folder}/{name}", "name": name})

        manifest.append({
            "value": folder,
            "label": f"Lolin S2 Mini ({version})",
            "firmwareVersion": version,
            "board": "Lolin S2 Mini",
            "variants": [],
            "baudrate": 115200,
            "manualBootloader": True,
            "useStub": False,
            "files": files,
        })

    (DIST / "firmwares-mouse64.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Packaged {len(manifest)} board(s) as version {version} in {DIST}")


if __name__ == "__main__":
    package()
