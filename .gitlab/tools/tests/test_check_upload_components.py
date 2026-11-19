# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Regression tests for `.gitlab/tools/check_upload_components.py`."""

import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest


REPO_ROOT = Path(__file__).resolve().parents[3]
_SPEC = importlib.util.spec_from_file_location(
    'check_upload_components',
    Path(__file__).resolve().parents[1] / 'check_upload_components.py',
)
check_upload_components = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(check_upload_components)


def _write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding='utf-8')


def _build_workflow(directories_block: str, namespace: str = 'espressif') -> str:
    return (
        'name: Push components\n'
        'jobs:\n'
        '  upload_components:\n'
        '    runs-on: ubuntu-latest\n'
        '    steps:\n'
        '      - uses: actions/checkout@master\n'
        '      - name: Upload components to component service\n'
        '        uses: espressif/upload-components-ci-action@v1\n'
        '        with:\n'
        '          # Upload dependencies before their consumers.\n'
        '          directories: >\n'
        f'{directories_block}'
        f'          namespace: "{namespace}"\n'
    )


def _run_check(repo_root: Path, workflow_rel: Path = Path('.github/workflows/upload_component.yml')):
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        rc = check_upload_components.check_upload_components(repo_root, workflow_rel)
    return rc, stdout.getvalue(), stderr.getvalue()


class CheckUploadComponentsTests(unittest.TestCase):
    def test_current_repository_passes(self):
        rc, out, err = _run_check(REPO_ROOT)
        self.assertEqual(rc, 0, msg=err)
        self.assertIn('Upload component workflow check passed', out)

    def test_manifest_requires_a_concrete_release_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest = root / 'service/network/brookesia_service_ble/idf_component.yml'
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            service/network/brookesia_service_ble;\n'),
            )
            for version in ('0.8.0', '0.8.0~1', '0.8.0~1-rc.1+build.7'):
                with self.subTest(version=version):
                    _write_file(manifest, f'version: "{version}"\n')
                    rc, _, err = _run_check(root)
                    self.assertEqual(rc, 0, msg=err)

            for version in ('0.8.*', '^0.8.0', '0.8', '0.8.0-01'):
                with self.subTest(version=version):
                    _write_file(manifest, f'version: "{version}"\n')
                    rc, _, err = _run_check(root)
                    self.assertEqual(rc, 1)
                    self.assertIn(str(manifest), err)
                    self.assertIn(repr(version), err)

    def test_inline_dependencies_mapping_catches_out_of_order(self):
        """Inline flow `dependencies: {...}` must not be skipped as empty."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(
                root / 'utils/brookesia_lib_utils/idf_component.yml',
                'version: "0.8.3"\n',
            )
            _write_file(
                root / 'runtime/brookesia_runtime_manager/idf_component.yml',
                'version: "0.8.2"\n'
                'dependencies: {espressif/brookesia_lib_utils: '
                '{version: "0.8.*", override_path: ../../utils/brookesia_lib_utils, require: public}}\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            runtime/brookesia_runtime_manager;\n'
                    '            utils/brookesia_lib_utils;\n'
                ),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('must appear earlier', err)

    def test_single_inline_dependency_catches_wrong_override_path(self):
        """Single inline dependency mapping must validate override_path."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(
                root / 'utils/brookesia_lib_utils/idf_component.yml',
                'version: "0.8.3"\n',
            )
            _write_file(
                root / 'runtime/brookesia_runtime_manager/idf_component.yml',
                'version: "0.8.2"\n'
                'dependencies:\n'
                '  espressif/brookesia_lib_utils: {version: "0.8.*", override_path: ../../utils/wrong_dir}\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            utils/brookesia_lib_utils;\n'
                    '            runtime/brookesia_runtime_manager;\n'
                ),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('override_path', err)

    def test_trailing_hash_note_in_folded_directories_scalar_is_rejected(self):
        """`# note` inside `directories: >` is literal string content in YAML."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'gui/brookesia_gui_gsp/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(root / 'runtime/brookesia_runtime_elf/idf_component.yml', 'version: "0.8.2"\n')
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            gui/brookesia_gui_gsp; # GSP\n'
                    '            runtime/brookesia_runtime_elf;\n'
                ),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('#GSPruntime/brookesia_runtime_elf', err)
            self.assertIn('directory does not exist', err)

            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            gui/brookesia_gui_gsp;\n'
                    '            runtime/brookesia_runtime_elf;\n'
                ),
            )
            rc_ok, _, err_ok = _run_check(root)
            self.assertEqual(rc_ok, 0, msg=err_ok)

    def test_namespace_mismatch_with_valid_override_path_is_rejected(self):
        """Wrong Registry namespace must fail even when override_path points into repo."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'gui/brookesia_gui_gsp/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(
                root / 'app/brookesia_app_settings/idf_component.yml',
                'version: "0.8.5"\n'
                'dependencies:\n'
                '  espressiff/brookesia_gui_gsp:\n'
                '    version: "0.8.*"\n'
                '    override_path: ../../gui/brookesia_gui_gsp\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            gui/brookesia_gui_gsp;\n'
                    '            app/brookesia_app_settings;\n'
                ),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('namespace mismatch', err)
            self.assertIn('espressiff/brookesia_gui_gsp', err)
            self.assertIn('espressif/brookesia_gui_gsp', err)

            _write_file(
                root / 'app/brookesia_app_settings/idf_component.yml',
                'version: "0.8.5"\n'
                'dependencies:\n'
                '  espressif/brookesia_gui_gsp:\n'
                '    version: "0.8.*"\n'
                '    override_path: ../../gui/brookesia_gui_gsp\n',
            )
            rc_ok, _, err_ok = _run_check(root)
            self.assertEqual(rc_ok, 0, msg=err_ok)

    def test_unprefixed_dependency_uses_default_espressif_namespace(self):
        """Unprefixed dependencies normalize to Component Manager's default `espressif` namespace."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'gui/brookesia_gui_gsp/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(
                root / 'app/brookesia_app_settings/idf_component.yml',
                'version: "0.8.5"\n'
                'dependencies:\n'
                '  brookesia_gui_gsp:\n'
                '    version: "0.8.*"\n'
                '    override_path: ../../gui/brookesia_gui_gsp\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            gui/brookesia_gui_gsp;\n'
                    '            app/brookesia_app_settings;\n',
                    namespace='espressif',
                ),
            )
            rc_ok, _, err_ok = _run_check(root)
            self.assertEqual(rc_ok, 0, msg=err_ok)

            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            gui/brookesia_gui_gsp;\n'
                    '            app/brookesia_app_settings;\n',
                    namespace='custom_ns',
                ),
            )
            rc_bad, _, err_bad = _run_check(root)
            self.assertEqual(rc_bad, 1)
            self.assertIn('namespace mismatch', err_bad)

    def test_missing_internal_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'gui/brookesia_gui_gsp/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(
                root / 'system/brookesia_system_core/idf_component.yml',
                'version: "0.8.7"\n'
                'dependencies:\n'
                '  espressif/brookesia_gui_gsp:\n'
                '    version: "0.8.*"\n'
                '    override_path: ../../gui/brookesia_gui_gsp\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            system/brookesia_system_core;\n'),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('missing from', err)
            self.assertIn('gui/brookesia_gui_gsp', err)

    def test_omitted_standalone_component_is_rejected(self):
        """Publishable components in the repo must be listed even without in-repo consumers."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'service/network/brookesia_service_bt/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(root / 'service/network/brookesia_service_ble/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            service/network/brookesia_service_bt;\n'),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('service/network/brookesia_service_ble', err)
            self.assertIn('missing from', err)

            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow(
                    '            service/network/brookesia_service_bt;\n'
                    '            service/network/brookesia_service_ble;\n'
                ),
            )
            rc_ok, out, err_ok = _run_check(root)
            self.assertEqual(rc_ok, 0, msg=err_ok)
            self.assertIn('2 components, namespace=espressif', out)

            _write_file(root / 'local/standalone/idf_component.yml', 'version: "1.0.0"\n')
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('Repository component "local/standalone"', err)

    def test_external_brookesia_prefixed_registry_dependency_passes(self):
        """External Registry packages starting with `brookesia_` (not in repo, no override_path) pass."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(
                root / 'system/brookesia_system_core/idf_component.yml',
                'version: "0.8.7"\n'
                'dependencies:\n'
                '  espressif/brookesia_not_in_repo: "0.8.*"\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            system/brookesia_system_core;\n'),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 0, msg=err)

    def test_external_registry_dependency_with_local_override_passes(self):
        cases = (
            ('../../../local/esp-gsp', True),
            ('../../local/esp-gsp', True),
            ('../../local/espressif__esp-gsp', True),
            ('../../local/esp-gsp', False),
        )
        for override, exists in cases:
            with self.subTest(override_path=override, exists=exists), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp) / 'repo'
                consumer = root / 'gui/brookesia_gui_gsp'
                if exists:
                    _write_file((consumer / override).resolve() / 'idf_component.yml', 'version: "1.4.0"\n')
                _write_file(
                    consumer / 'idf_component.yml',
                    'version: "0.8.0"\n'
                    'dependencies:\n'
                    f'  espressif/esp-gsp: {{version: "1.4.0", override_path: {override}}}\n',
                )
                _write_file(
                    root / '.github/workflows/upload_component.yml',
                    _build_workflow('            gui/brookesia_gui_gsp;\n'),
                )
                rc, _, err = _run_check(root)
                self.assertEqual(rc, 0, msg=err)

    def test_external_id_cannot_override_a_published_component(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(root / 'gui/brookesia_gui_gsp/idf_component.yml', 'version: "0.8.0"\n')
            _write_file(
                root / 'system/brookesia_system_core/idf_component.yml',
                'version: "0.8.7"\n'
                'dependencies:\n'
                '  espressif/esp-gsp: {version: "1.4.0", override_path: ../../gui/brookesia_gui_gsp}\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            gui/brookesia_gui_gsp; system/brookesia_system_core;\n'),
            )
            rc, _, err = _run_check(root)
            self.assertEqual(rc, 1)
            self.assertIn('targets published component "espressif/brookesia_gui_gsp"', err)

    def test_git_and_path_dependency_sources_report_unsupported_source(self):
        """`git` and `path` sources must be reported as unsupported sources, not missing/order errors."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write_file(
                root / 'system/brookesia_system_core/idf_component.yml',
                'version: "0.8.7"\n'
                'dependencies:\n'
                '  espressif/brookesia_foo:\n'
                '    git: "https://github.com/espressif/esp-brookesia.git"\n'
                '    path: "foo"\n'
                '    version: "main"\n',
            )
            _write_file(
                root / '.github/workflows/upload_component.yml',
                _build_workflow('            system/brookesia_system_core;\n'),
            )
            rc_git, _, err_git = _run_check(root)
            self.assertEqual(rc_git, 1)
            self.assertIn('uses unsupported "git" source', err_git)
            self.assertNotIn('missing from', err_git)
            self.assertNotIn('must appear earlier', err_git)

            _write_file(
                root / 'system/brookesia_system_core/idf_component.yml',
                'version: "0.8.7"\n'
                'dependencies:\n'
                '  espressif/brookesia_foo:\n'
                '    path: "../external_foo"\n',
            )
            rc_path, _, err_path = _run_check(root)
            self.assertEqual(rc_path, 1)
            self.assertIn('uses unsupported "path" source', err_path)
            self.assertNotIn('missing from', err_path)
            self.assertNotIn('must appear earlier', err_path)


if __name__ == '__main__':
    unittest.main()
