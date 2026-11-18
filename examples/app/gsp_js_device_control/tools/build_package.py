#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Compile the device-control scenes, optionally preview them, and pack a .bpk.

The GUI backend and the JavaScript execution backend are separate. This template
is GSP plus JavaScript. LVGL packages stay on the existing JSON UI path.
Figma export does not produce a GSP bundle.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

APP_ROOT = Path(__file__).resolve().parents[1]
SOURCE_PACKAGE = APP_ROOT / "package"
SCENES = [
    APP_ROOT / "scenes" / "status.json",
    APP_ROOT / "scenes" / "brightness.json",
]
# Match the ESP32-P4 Function EV firmware profile used by examples/system/gsp_app.
GSP_PROFILE = [
    "--platform-soc",
    "esp32p4",
    "--pixel-format",
    "rgb565",
    "--psram",
    "--hardware-jpeg",
    "--software-jpeg",
    "--ppa",
    "--dma2d",
    "--list-max-slots",
    "65535",
    "--list-text-slots",
    "65533",
    "--instance-states-per-slot",
    "65535",
    "--max-dynamic-image-targets",
    "32767",
    "--max-font-packs",
    "32",
    "--project-max-scenes",
    "65534",
    "--project-max-lists",
    "256",
    "--project-list-max-slots",
    "65535",
    "--project-list-text-slots",
    "65533",
    "--project-text-slots",
    "65533",
    "--project-dynamic-image-slots",
    "0",
    "--project-component-instances",
    "65534",
    "--project-stack-view-max-depth",
    "64",
    "--project-instance-states-per-slot",
    "65535",
    "--project-glyph-run-slots",
    "65535",
    "--project-max-asset-anims",
    "255",
]
DEFAULT_GSPC = Path.home() / ".espressif/esp-gsp-tools/gspc/v0.5.0/linux-x86_64/bin/gspc"


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def reject_unsupported_combination(args: argparse.Namespace) -> None:
    if args.from_figma:
        fail(
            "Figma → GSP is not supported. Author the GSP scene JSON in scenes/, "
            "or keep using the LVGL JSON UI path for Figma export."
        )
    if args.gui_backend != "gsp":
        fail(
            "This template's GUI backend is gsp and its execution backend is JavaScript. "
            "Refusing gui backend "
            f"{args.gui_backend!r}. Keep the LVGL path on existing JSON UI packages; "
            "do not point a GSP bundle at it."
        )
    if args.execution_backend != "JavaScript":
        fail(
            "This template's execution backend is JavaScript. "
            f"Refusing execution backend {args.execution_backend!r}."
        )


def find_gspc(explicit: str) -> Path:
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            fail(f"GSPC_EXECUTABLE does not exist: {path}")
        return path
    env = os.environ.get("GSPC_EXECUTABLE", "")
    if env:
        path = Path(env)
        if path.is_file():
            return path
        fail(f"GSPC_EXECUTABLE does not exist: {path}")
    found = shutil.which("gspc")
    if found:
        return Path(found)
    if DEFAULT_GSPC.is_file():
        return DEFAULT_GSPC
    fail(
        "GSP backend is selected, but gspc 0.5.0 was not found. "
        "Install esp-gsp-tools or set GSPC_EXECUTABLE. "
        "The LVGL JSON UI path does not use gspc."
    )


def find_simulator(explicit: str) -> Path | None:
    if explicit:
        path = Path(explicit)
        return path if path.is_file() else None
    env = os.environ.get("GSP_SIM_EXECUTABLE", "")
    if env and Path(env).is_file():
        return Path(env)
    found = shutil.which("gsp_sim_host")
    return Path(found) if found else None


def compile_bundle(gspc: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(gspc),
        "compile",
        *[str(scene) for scene in SCENES],
        *GSP_PROFILE,
        "--deployable",
        "-o",
        str(destination),
        "--symbol",
        "device_control",
    ]
    print(" ".join(command))
    completed = subprocess.run(command, check=False)
    if completed.returncode != 0:
        fail(f"gspc failed with exit code {completed.returncode}")
    header = destination.read_bytes()[:4]
    if header != b"GSPB":
        fail(f"Compiled bundle is not a deployable GSPB: {destination}")


def stage_package(bundle: Path, version: str, staging: Path) -> Path:
    if staging.exists():
        shutil.rmtree(staging)
    shutil.copytree(SOURCE_PACKAGE, staging)
    staged_bundle = staging / "res" / "device_control.gspb"
    shutil.copyfile(bundle, staged_bundle)
    manifest_path = staging / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("gui", {}).get("backend") != "gsp":
        fail("Staged manifest.gui.backend must be gsp")
    if manifest.get("runtime", {}).get("type") != "JavaScript":
        fail("Staged manifest.runtime.type must be JavaScript")
    profile_path = staging / "res" / "profile.json"
    profile = json.loads(profile_path.read_text())
    if profile.get("gui_backend") != "gsp":
        fail(
            "Staged profile.json gui_backend must be gsp to match manifest.gui.backend, "
            f"got {profile.get('gui_backend')!r}"
        )
    root = profile.get("root")
    if not isinstance(root, str) or not root:
        fail("Staged profile.json root must be the deployable GSP bundle")
    resource_dir = (staging / "res").resolve()
    root_path = (resource_dir / root).resolve()
    if root_path != staged_bundle.resolve():
        fail(
            f"Staged profile.json root {root!r} must name the compiled bundle "
            f"res/{staged_bundle.name}, not another file"
        )
    if not root_path.is_file():
        fail(f"Staged profile.json root {root!r} does not exist in the package")
    if "screen_flows" in profile or "preload_dom" in profile:
        fail("Staged profile.json must not use LVGL screen_flows or preload_dom")
    manifest["package"]["version"] = version
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    app_js = staging / "app" / "app.js"
    app_js.write_text(app_js.read_text().replace('var PACKAGE_VERSION = "0.1.0";', f'var PACKAGE_VERSION = "{version}";', 1))
    return staging


def pack_bpk(staging: Path, version: str, output: Path) -> Path:
    package_id = json.loads((staging / "manifest.json").read_text())["package"]["id"]
    output.parent.mkdir(parents=True, exist_ok=True)
    bpk = output / f"{package_id}.debug.{version}.bpk"
    if bpk.exists():
        bpk.unlink()
    with zipfile.ZipFile(bpk, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(staging.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(staging).as_posix())
    return bpk


def preview_bundle(bundle: Path, simulator: Path | None) -> None:
    print(
        "Scene preview renders the compiled GSP bundle and pointer input. "
        "It does not run the JavaScript app, call Display, or install a .bpk."
    )
    if simulator is None:
        fail(
            "gsp_sim_host was not found. Install esp-gsp-tools and set GSP_SIM_EXECUTABLE "
            "before --preview."
        )
    command = [str(simulator), "--bundle", str(bundle), "--frames", "0"]
    print(" ".join(command))
    completed = subprocess.run(command, check=False)
    if completed.returncode != 0:
        fail(f"gsp_sim_host failed with exit code {completed.returncode}")


def corrupt_bundle(bpk: Path, destination: Path) -> None:
    destination.write_bytes(bpk.read_bytes())
    with zipfile.ZipFile(destination, "a") as archive:
        name = "res/device_control.gspb"
        data = bytearray(archive.read(name))
    if len(data) < 8 or data[:4] != b"GSPB":
        fail("Package has no deployable GSP bundle to damage")
    data[16] ^= 0xFF
    # Rewrite the member. ZipFile cannot replace in place reliably, so rebuild.
    temp = destination.with_suffix(".tmp.zip")
    with zipfile.ZipFile(bpk, "r") as source, zipfile.ZipFile(temp, "w", compression=zipfile.ZIP_DEFLATED) as target:
        for info in source.infolist():
            payload = bytes(data) if info.filename == name else source.read(info.filename)
            target.writestr(info.filename, payload)
    temp.replace(destination)
    print(f"Wrote damaged package {destination}")
    print("Install should fail while opening the bundle, with a CRC or structure error.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui-backend", default="gsp", choices=["gsp", "lvgl"])
    parser.add_argument("--execution-backend", default="JavaScript")
    parser.add_argument("--from-figma", action="store_true")
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--gspc", default="")
    parser.add_argument("--simulator", default="")
    parser.add_argument("--preview", action="store_true")
    parser.add_argument("--corrupt-into", type=Path)
    parser.add_argument("--output", type=Path, default=APP_ROOT / "build")
    args = parser.parse_args()
    reject_unsupported_combination(args)

    gspc = find_gspc(args.gspc)
    bundle = args.output / "device_control.gspb"
    compile_bundle(gspc, bundle)
    if args.preview:
        preview_bundle(bundle, find_simulator(args.simulator))
    staging = args.output / "staging"
    stage_package(bundle, args.version, staging)
    bpk = pack_bpk(staging, args.version, args.output)
    print(f"Packed {bpk}")
    print("Install on a running GSP host:")
    print(f"  brookesia-usb install {bpk}")
    print("Update by packing a higher version and installing again.")
    print("Uninstall from the host:")
    print('  brookesia-usb call SystemCore ListApps')
    print('  brookesia-usb call SystemCore UninstallApp \'{"AppId": <id>}\'')
    if args.corrupt_into:
        corrupt_bundle(bpk, args.corrupt_into)


if __name__ == "__main__":
    main()
