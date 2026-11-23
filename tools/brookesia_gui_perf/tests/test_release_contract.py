# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Read-only release-contract check between public API and the CHANGELOG.

A component bump that ships new public entry points without a matching release note
makes the registry tell users the wrong "first available" version. Keep each contract
pinned to the release that introduced it, even when newer releases are added.
"""

import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

CONTRACTS = (
    (
        ROOT / "system/brookesia_system_core/include/brookesia/system_core/system/system.hpp",
        ROOT / "system/brookesia_system_core/CHANGELOG.md",
        "0.8.6",
        (
            "system_gui_load_json",
            "system_gui_mount_screen",
            "system_gui_unmount_screen",
            "system_gui_unload",
            "system_gui_load_theme_file",
            "system_gui_set_theme",
        ),
    ),
)

NATIVE_VIEW_CONTRACTS = (
    (
        ROOT / "gui/brookesia_gui_interface/include/brookesia/gui_interface/runtime.hpp",
        ROOT / "gui/brookesia_gui_interface/CHANGELOG.md",
        "0.8.8",
        ("with_native_object",),
    ),
    (
        ROOT / "gui/brookesia_gui_lvgl/include/brookesia/gui_lvgl/backend.hpp",
        ROOT / "gui/brookesia_gui_lvgl/CHANGELOG.md",
        "0.8.10",
        ("get_native_object",),
    ),
    (
        ROOT / "system/brookesia_system_core/include/brookesia/system_core/app/gui_runtime.hpp",
        ROOT / "system/brookesia_system_core/CHANGELOG.md",
        "0.8.8",
        ("with_native_view",),
    ),
)


def release_section(changelog: Path, version: str) -> str:
    text = changelog.read_text(encoding="utf-8")
    match = re.search(r"^## v" + re.escape(version) + r"(?:[ \t]+-[^\n]*)?\n(.*?)(?=^## v|\Z)", text, re.S | re.M)
    return match.group(1) if match else ""


class ReleaseContractTest(unittest.TestCase):
    def test_release_section_does_not_include_adjacent_releases(self):
        with tempfile.TemporaryDirectory() as temporary:
            changelog = Path(temporary) / "CHANGELOG.md"
            changelog.write_text("## v0.8.7 - 2026-10-28\nnew\n\n## v0.8.6 - 2026-10-27\nold\n", encoding="utf-8")
            self.assertEqual(release_section(changelog, "0.8.6").strip(), "old")
            self.assertEqual(release_section(changelog, "0.8.5"), "")

    def _assert_names_in_release(self, contracts):
        for header, changelog, version, names in contracts:
            text = header.read_text(encoding="utf-8")
            section = release_section(changelog, version)
            self.assertNotEqual(section, "", "missing v%s in %s" % (version, changelog.parent.name))
            for name in names:
                self.assertIn(name, text, "%s is not declared in %s" % (name, header.name))
                self.assertIn(
                    name,
                    section,
                    "%s is public in %s but missing from the v%s %s section; a reader of "
                    "the registry would not learn when it became available"
                    % (name, header.name, version, changelog.parent.name),
                )

    def test_public_api_is_described_in_its_release(self):
        self._assert_names_in_release(CONTRACTS)

    def test_native_view_api_is_described_in_its_release(self):
        self._assert_names_in_release(NATIVE_VIEW_CONTRACTS)

    def test_process_timers_change_is_recorded(self):
        """The refresh-path fix shipped in GUI LVGL v0.8.7."""
        changelog = ROOT / "gui/brookesia_gui_lvgl/CHANGELOG.md"
        self.assertIn("process_timers", release_section(changelog, "0.8.7"))


if __name__ == "__main__":
    unittest.main()
