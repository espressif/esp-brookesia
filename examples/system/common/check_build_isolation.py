# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Check the actual system example outputs after an ordinary full IDF build."""

import argparse
import json
from pathlib import Path

from component_manager import local_manifest_hash


def check_build(build):
    """Reject backend dependency or generated-output leakage."""
    build = build.resolve()
    description = json.loads((build / "project_description.json").read_text())
    config = json.loads((build / "config/sdkconfig.json").read_text())
    gsp = config.get("BROOKESIA_GUI_BACKEND_GSP", False)
    assert config.get("BROOKESIA_GUI_BACKEND_LVGL") is not gsp, "Exactly one backend must be selected"
    if description["project_name"] == "example_system_super":
        assert not gsp, "Super supports LVGL only"
        assert "BROOKESIA_GUI_BACKEND_GSP" not in config, "Unsupported GSP choice must stay hidden"

    assert Path(description["config_file"]).is_relative_to(build), "sdkconfig must be build-local"
    assert (build / "dependencies.lock").is_file(), "Missing build-local dependency resolution"
    manifest_stamp = build / "dependencies/local_manifests.sha256"
    assert manifest_stamp.is_file(), "Missing local dependency manifest stamp"
    assert manifest_stamp.read_text() == local_manifest_hash(build / "dependencies.lock"), \
        "Local dependency manifests changed without reconfiguration"
    components = description["build_component_info"]
    component_dirs = [Path(info["dir"]) for info in components.values()]
    for directory in component_dirs:
        if "managed_components" in directory.parts:
            assert directory.is_relative_to(build / "dependencies"), f"Shared download state: {directory}"

    def has_component(name):
        return any(directory.name in (name, f"espressif__{name}", f"lvgl__{name}") for directory in component_dirs)

    commands = (build / "compile_commands.json").read_text()
    map_path = next(build.glob("example_system_*.map"), None)
    assert map_path is not None, f"Missing example_system_*.map link map in {build}"
    link_map = map_path.read_text()
    stage = build / "littlefs_stage"
    if gsp:
        assert has_component("brookesia_gui_gsp") and has_component("esp-gsp")
        for name in ("brookesia_gui_lvgl", "brookesia_system_super", "lvgl", "esp_lvgl_adapter"):
            assert not has_component(name), f"LVGL dependency entered GSP: {name}"
        assert not (stage / "system/super").exists() and not (stage / "system/fonts").exists()
        assert not any("examples/system/super" in str(directory) for directory in component_dirs)
        assert "examples/system/super/" not in commands, "GSP consumes Super example sources"
    else:
        assert has_component("brookesia_gui_lvgl") and has_component("brookesia_system_super")
        for name in ("brookesia_gui_gsp", "esp-gsp"):
            assert not has_component(name), f"GSP dependency entered LVGL: {name}"
        for name in ("gsp_shell_dark.cpp", "gsp_appearance.cpp", "gsp_dark_bundle.cpp"):
            assert name not in commands, f"GSP-only source compiled for LVGL: {name}"
        assert "libbrookesia_gui_gsp.a" not in link_map and "libespressif__esp-gsp.a" not in link_map
        assert not list(build.glob("esp-idf/*/gsp_gen_*")), "GSP resources generated for LVGL"
        assert (stage / "system/super").is_dir() and (stage / "system/fonts").is_dir()
    assert (stage / "apps/brookesia.general.settings/res/i18n").is_dir()
    assert (build / "littlefs_data.bin").is_file(), "Full filesystem image was not built"
    assert (build / description["app_bin"]).is_file(), "Firmware was not built"
    return {"project": description["project_name"], "backend": "gsp" if gsp else "lvgl", "build": str(build)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    print(json.dumps(check_build(args.build)))


if __name__ == "__main__":
    main()
