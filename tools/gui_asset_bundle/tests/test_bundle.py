# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import struct
import tempfile
import unittest
from pathlib import Path

from brookesia_gui_asset_bundle.bundle import (
    ALIGNMENT,
    HEADER_SIZE,
    MAGIC,
    VERSION,
    BundleError,
    collect_assets,
    pack_assets,
    read_bundle,
    verify_bundle,
)


class BundleTestCase(unittest.TestCase):
    def setUp(self):
        self._temp = tempfile.TemporaryDirectory()
        self.addCleanup(self._temp.cleanup)
        self.root = Path(self._temp.name) / "res"
        self.root.mkdir()
        self.out = Path(self._temp.name) / "assets.bundle"

    def write(self, relative, content):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content if isinstance(content, bytes) else content.encode("utf-8"))
        return path

    def make_tree(self):
        self.write("root.json", '{"type": "root"}')
        self.write("screens/home.json", '{"type": "viewScreen"}')
        self.write("screens/deep/detail.json", '{"type": "viewScreen", "n": 2}')
        self.write("images/icon.bin", b"\x00\x01\x02")


class PackTests(BundleTestCase):
    def test_header_and_index_match_the_format(self):
        self.make_tree()
        stats = pack_assets(self.root, self.out)
        blob = self.out.read_bytes()

        magic, version, flags, count, index_size, payload_offset = struct.unpack_from("<4sIIIII", blob, 0)
        self.assertEqual(magic, MAGIC)
        self.assertEqual(version, VERSION)
        self.assertEqual(flags, 0)
        self.assertEqual(count, 3)
        self.assertEqual(stats.entries, 3)
        self.assertEqual(stats.total_bytes, len(blob))
        expected_index = sum(4 + len(rel.encode("utf-8")) + 16 for rel in (
            "root.json", "screens/deep/detail.json", "screens/home.json"
        ))
        self.assertEqual(index_size, expected_index)
        self.assertEqual(payload_offset, HEADER_SIZE + index_size + ((-(HEADER_SIZE + index_size)) % ALIGNMENT))
        self.assertEqual(payload_offset % ALIGNMENT, 0)
        self.assertEqual(len(blob), payload_offset + stats.payload_bytes)

    def test_payload_round_trips(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        bundle = read_bundle(self.out)

        self.assertEqual(
            bundle.paths(),
            ["root.json", "screens/deep/detail.json", "screens/home.json"],
        )
        self.assertEqual(bundle.read("root.json"), b'{"type": "root"}')
        self.assertEqual(bundle.read("screens/deep/detail.json"), b'{"type": "viewScreen", "n": 2}')
        with self.assertRaises(KeyError):
            bundle.read("images/icon.bin")

    def test_every_payload_offset_is_aligned(self):
        self.write("a.json", b"x" * 5)
        self.write("b.json", b"y" * 7)
        self.write("c.json", b"z" * 9)
        pack_assets(self.root, self.out)
        bundle = read_bundle(self.out)

        for entry in bundle.entries:
            self.assertEqual(entry.offset % ALIGNMENT, 0, entry.path)

    def test_packing_is_deterministic(self):
        self.make_tree()
        second = Path(self._temp.name) / "second.bundle"
        pack_assets(self.root, self.out)
        pack_assets(self.root, second)
        self.assertEqual(self.out.read_bytes(), second.read_bytes())

    def test_default_include_only_takes_json(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        self.assertEqual(len(read_bundle(self.out).entries), 3)

        pack_assets(self.root, self.out, patterns=("*",))
        self.assertEqual(len(read_bundle(self.out).entries), 4)

    def test_include_glob_matches_nested_paths(self):
        self.make_tree()
        pack_assets(self.root, self.out, patterns=("screens/*",))
        self.assertEqual(read_bundle(self.out).paths(), ["screens/deep/detail.json", "screens/home.json"])

    def test_empty_tree_produces_a_valid_bundle(self):
        stats = pack_assets(self.root, self.out)
        self.assertEqual(stats.entries, 0)
        bundle = read_bundle(self.out)
        self.assertEqual(bundle.entries, ())

    def test_prefix_sibling_sorts_like_runtime_string_order(self):
        # '.' (0x2E) < '/' (0x2F), so the file sorts before the directory sibling.
        # Path.parts order is the other way around and would break binary search.
        self.write("images/icon/x.png", b"nested")
        self.write("images/icon.png", b"flat")
        pack_assets(self.root, self.out, patterns=("*",))
        bundle = read_bundle(self.out)

        self.assertEqual(bundle.paths(), ["images/icon.png", "images/icon/x.png"])
        self.assertEqual(bundle.read("images/icon.png"), b"flat")
        self.assertEqual(bundle.read("images/icon/x.png"), b"nested")

    def test_output_inside_the_tree_is_not_packed(self):
        self.write("root.json", "{}")
        inside = self.root / "assets.bundle"
        pack_assets(self.root, inside, patterns=("*",))
        self.assertEqual(read_bundle(inside).paths(), ["root.json"])

    def test_collect_assets_rejects_a_missing_root(self):
        with self.assertRaises(BundleError):
            collect_assets(Path(self._temp.name) / "missing")


class ReadTests(BundleTestCase):
    def test_invalid_utf8_path_is_a_bundle_error(self):
        self.write("root.json", b"{}")
        pack_assets(self.root, self.out, patterns=("*",))
        data = bytearray(self.out.read_bytes())
        path_at = data.find(b"root.json")
        self.assertGreater(path_at, 0)
        data[path_at] = 0xFF
        self.out.write_bytes(data)
        with self.assertRaises(BundleError) as caught:
            read_bundle(self.out)
        self.assertIn("UTF-8", str(caught.exception))

    def test_rejects_bad_magic(self):
        self.out.write_bytes(b"NOPE" + b"\x00" * 32)
        with self.assertRaises(BundleError):
            read_bundle(self.out)

    def test_rejects_unsupported_version(self):
        self.out.write_bytes(struct.pack("<4sIIIII", MAGIC, 99, 0, 0, 0, HEADER_SIZE))
        with self.assertRaises(BundleError):
            read_bundle(self.out)

    def test_rejects_truncated_payload(self):
        self.write("root.json", "0123456789")
        pack_assets(self.root, self.out)
        bundle = read_bundle(self.out)
        entry = bundle.entries[0]
        end = bundle.payload_offset + entry.offset + entry.size
        self.out.write_bytes(self.out.read_bytes()[:end - 1])
        with self.assertRaises(BundleError):
            read_bundle(self.out)

    def test_rejects_short_file(self):
        self.out.write_bytes(b"BGB1")
        with self.assertRaises(BundleError):
            read_bundle(self.out)

    def test_rejects_corrupted_payload(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        blob = bytearray(self.out.read_bytes())
        bundle = read_bundle(self.out)
        entry = bundle.entries[0]
        start = bundle.payload_offset + entry.offset
        # Flip one byte inside the payload, keeping the structure and length intact.
        blob[start] ^= 0xFF
        self.out.write_bytes(blob)
        with self.assertRaises(BundleError) as caught:
            read_bundle(self.out)
        self.assertIn("CRC", str(caught.exception))


class VerifyTests(BundleTestCase):
    def test_matching_tree_has_no_problems(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        self.assertEqual(verify_bundle(self.out, self.root), [])

    def test_detects_changed_source(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        self.write("root.json", '{"type": "changed"}')
        problems = verify_bundle(self.out, self.root)
        self.assertTrue(any("differs" in problem for problem in problems), problems)

    def test_detects_missing_and_extra_entries(self):
        self.make_tree()
        pack_assets(self.root, self.out)
        (self.root / "screens" / "home.json").unlink()
        self.write("screens/added.json", "{}")

        problems = verify_bundle(self.out, self.root)
        joined = "\n".join(problems)
        self.assertIn("no source file: screens/home.json", joined)
        self.assertIn("missing from the bundle: screens/added.json", joined)


if __name__ == "__main__":
    unittest.main()
