#!/usr/bin/env python3
"""Package GRGDO1 builds and update the installer from those exact artifacts."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


REVISIONS = {"ratgdo_esp32dev": "rev2", "ratgdo_esp32dev_rev1": "rev1"}
PARTS = (("bootloader", 0x1000), ("partitions", 0x8000),
         ("boot_app0", 0xE000), ("firmware", 0x10000))


def validate_tag(tag):
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        raise ValueError(f"Expected a release tag such as v1.4.0, got {tag!r}")
    return tag


def prefix(revision, tag):
    return f"homekit-grgdo1-{revision}-{validate_tag(tag)}"


def package(build, output, revision, tag, boot_app0):
    output.mkdir(parents=True, exist_ok=True)
    name = prefix(revision, tag)
    # Each artifact has its own revision so upload/download cannot mix boards.
    for part, _ in PARTS:
        source = boot_app0 if part == "boot_app0" else build / f"{part}.bin"
        shutil.copyfile(source, output / f"{name}.{part}.bin")
    shutil.copyfile(build / "firmware.elf", output / f"{name}.elf")
    firmware = (output / f"{name}.firmware.bin").read_bytes()
    if not 0 < len(firmware) <= 0x1F0000:
        raise ValueError("Firmware does not fit the GRGDO1 OTA partition")
    (output / f"{name}.firmware.md5").write_text(hashlib.md5(firmware).hexdigest() + "\n")


def update_docs(assets, docs, tag):
    validate_tag(tag)
    # Validate both board revisions before modifying the published installer.
    for rev in ("rev1", "rev2"):
        name = prefix(rev, tag)
        for part, _ in PARTS:
            if not (assets / f"{name}.{part}.bin").is_file():
                raise ValueError(f"Missing {rev} {part} artifact")
        firmware = (assets / f"{name}.firmware.bin").read_bytes()
        expected = (assets / f"{name}.firmware.md5").read_text().strip()
        if hashlib.md5(firmware).hexdigest() != expected:
            raise ValueError(f"Firmware checksum mismatch for {rev}")
    destination = docs / "firmware"
    destination.mkdir(parents=True, exist_ok=True)
    for rev in ("rev1", "rev2"):
        name = prefix(rev, tag)
        for suffix in [f"{part}.bin" for part, _ in PARTS] + ["firmware.md5"]:
            shutil.copyfile(assets / f"{name}.{suffix}", destination / f"{name}.{suffix}")
        manifest = {
            "name": f"homekit-grgdo1-{rev}", "version": tag,
            "new_install_prompt_erase": True, "new_install_improv_wait_time": 60,
            "builds": [{"chipFamily": "ESP32", "parts": [
                {"path": f"firmware/{name}.{part}.bin", "offset": offset}
                for part, offset in PARTS
            ]}],
        }
        (docs / f"manifest-{rev}.json").write_text(json.dumps(manifest, indent=2) + "\n")
    shutil.copyfile(docs / "manifest-rev2.json", docs / "manifest.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("package")
    build.add_argument("--environment", required=True, choices=REVISIONS)
    build.add_argument("--build-root", type=Path, default=Path(".pio/build"))
    build.add_argument("--boot-app0", type=Path, required=True)
    build.add_argument("--output", type=Path, default=Path("release_assets"))
    build.add_argument("--tag", required=True)
    docs = commands.add_parser("update-docs")
    docs.add_argument("--assets", type=Path, default=Path("release_assets"))
    docs.add_argument("--docs", type=Path, default=Path("docs"))
    docs.add_argument("--tag", required=True)
    args = parser.parse_args()
    if args.command == "package":
        package(args.build_root / args.environment, args.output,
                REVISIONS[args.environment], args.tag, args.boot_app0)
    else:
        update_docs(args.assets, args.docs, args.tag)


if __name__ == "__main__":
    main()
