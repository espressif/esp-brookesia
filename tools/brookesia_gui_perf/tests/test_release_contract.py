# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Read-only release-contract check between public API and the CHANGELOG.

A component bump that ships new public entry points without a matching release note
makes the registry tell users the wrong "first available" version, and a later edit
can silently drop an entry again. This test pins the API names this MR adds to the
newest changelog section of the component that owns them.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

CONTRACTS = (
    (
        ROOT / "system/brookesia_system_core/include/brookesia/system_core/system/system.hpp",
        ROOT / "system/brookesia_system_core/CHANGELOG.md",
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


def newest_section(changelog: Path) -> str:
    text = changelog.read_text(encoding="utf-8")
    match = re.search(r"^## v[^\n]*\n(.*?)(?=^## v|\Z)", text, re.S | re.M)
    return match.group(1) if match else ""


class ReleaseContractTest(unittest.TestCase):
    def test_public_api_is_described_in_the_newest_changelog(self):
        for header, changelog, names in CONTRACTS:
            text = header.read_text(encoding="utf-8")
            section = newest_section(changelog)
            for name in names:
                self.assertIn(name, text, "%s is not declared in %s" % (name, header.name))
                self.assertIn(
                    name,
                    section,
                    "%s is public in %s but missing from the newest %s section; a reader of "
                    "the registry would not learn when it became available"
                    % (name, header.name, changelog.parent.name),
                )

    def test_process_timers_change_is_recorded(self):
        """The refresh-path fix lives in gui_lvgl and must be in its newest section."""
        changelog = ROOT / "gui/brookesia_gui_lvgl/CHANGELOG.md"
        self.assertIn("process_timers", newest_section(changelog))


if __name__ == "__main__":
    unittest.main()
