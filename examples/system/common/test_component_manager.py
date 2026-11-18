# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Exercise build-local manager state; fake only the official module entry point."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import yaml


spec = importlib.util.spec_from_file_location(
    'system_component_manager', Path(__file__).with_name('component_manager.py'))
manager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manager)
launcher_spec = importlib.util.spec_from_file_location(
    'system_python_launcher', Path(__file__).with_name('python_launcher.py'))
launcher = importlib.util.module_from_spec(launcher_spec)
launcher_spec.loader.exec_module(launcher)


def snapshot(directory):
    return {str(path.relative_to(directory)): path.read_bytes()
            for path in directory.rglob('*') if path.is_file()}


class ComponentManagerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='brookesia-component-manager-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        (self.project / 'sdkconfig').write_text('CONFIG_IDF_TARGET="esp32p4"\n')
        self.local = self.project / 'components' / 'parent'
        self.nested = self.local / 'nested'
        self.nested.mkdir(parents=True)
        (self.local / 'idf_component.yml').write_text(
            'version: "1.0.0"\ndependencies:\n  child:\n    override_path: nested\n')
        (self.nested / 'idf_component.yml').write_text(
            'version: "1.0.0"\ndependencies:\n  vendor/codec: ">=1.0.0"\n')
        cached = self.project / 'managed_components' / 'vendor__codec'
        cached.mkdir(parents=True)
        (cached / 'idf_component.yml').write_text('version: "1.0.0"\n')
        (cached / 'download.bin').write_bytes(b'validated-download\x00\xff')
        locked = {
            'manifest_hash': 'original-project-hash',
            'dependencies': {
                'parent': {'source': {'type': 'local', 'path': 'components/parent'}},
                'child': {'source': {'type': 'local', 'path': 'components/parent/nested'}},
                'vendor/codec': {'source': {'type': 'service', 'service_url': 'https://example.invalid'},
                                 'version': '1.0.0'},
            },
        }
        (self.project / 'dependencies.lock').write_text(yaml.safe_dump(locked))
        self.original = snapshot(self.project)
        self.outputs = [self.root / 'build-super', self.root / 'build-gsp']
        for output in self.outputs:
            output.mkdir()
            (output / 'config').mkdir()
            (output / 'config/sdkconfig.json').write_text('{}\n')

    def invoke(self, output, runner, command='prepare_dependencies', legacy=False, use_sdk_json=None):
        args = ['wrapper', 'idf_component_manager.prepare_components',
                f'--project_dir={self.project}',
                f'--lock_path={output / "dependencies.lock"}', '--interface_version=3', command]
        if legacy:
            args.append(f'--sdkconfig_json_file={output / "config/sdkconfig.json"}')
        else:
            args.append(f'--build_dir={output}')
        if use_sdk_json is not None:
            args.append(f'--use_sdk_json={use_sdk_json}')
        with patch.object(sys, 'argv', args), patch.object(manager.runpy, 'run_module', side_effect=runner) as run:
            manager.main()
        run.assert_called_once_with('idf_component_manager.prepare_components', run_name='__main__')

    def test_legacy_prepare_without_build_dir_is_isolated(self):
        output = self.outputs[0]
        self.invoke(output, self.official_success(output, '0' * 64), legacy=True)
        manager.finalize_state(output / 'dependencies.lock')
        self.assertEqual(snapshot(self.project), self.original)
        self.assertEqual((output / 'dependencies/local_manifests.sha256').read_text(),
                         manager.local_manifest_hash(output / 'dependencies.lock'))

    def test_python_launcher_preserves_interpreter_arguments_and_failure(self):
        executable = launcher.create_launcher(self.root / "with space's" / 'python', sys.executable)
        arguments = ['space value', 'quote\"value', "single'value", '$HOME; literal']
        script = 'import json,sys; print(json.dumps([sys.executable, sys.argv[1:]])); sys.exit(17)'
        result = subprocess.run([str(executable), '-c', script, *arguments], capture_output=True, text=True)
        self.assertEqual(result.returncode, 17, result.stderr)
        self.assertEqual(json.loads(result.stdout), [sys.executable, arguments])
        module = 'idf_component_manager.prepare_components'
        lock = str(self.root / 'dependencies.lock')
        arguments = ['-m', module, '--project_dir=x', f'--lock_path={lock}']
        self.assertEqual(launcher.command(sys.executable, arguments, lock),
                         [sys.executable, str(Path(__file__).with_name('component_manager.py')),
                          *arguments[1:]])
        self.assertEqual(launcher.command(sys.executable, ['-m', module + '_other'], lock),
                         [sys.executable, '-m', module + '_other'])
        for other_lock in ['', '/bootloader/dependencies.lock']:
            arguments = ['-m', module, f'--lock_path={other_lock}']
            self.assertEqual(launcher.command(sys.executable, arguments, lock), [sys.executable, *arguments])

    def test_common_rejects_external_component_wrapper(self):
        common = Path(__file__).resolve().parents[1] / 'common.cmake'
        script = self.root / 'external-wrapper.cmake'
        script.write_text(f'include("{common}")\n')
        environment = dict(os.environ, IDF_COMPONENT_WRAPPER='/external/wrapper.py')
        result = subprocess.run(['cmake', '-P', str(script)], env=environment,
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('an external wrapper is set', ' '.join(result.stderr.split()))

    def configure(self, output, runner):
        self.invoke(output, runner)
        self.assertFalse((output / 'dependencies/local_manifests.sha256').exists())
        manager.finalize_state(output / 'dependencies.lock')

    def official_success(self, output, expected_hash):
        def run(module, run_name):
            self.assertEqual(module, 'idf_component_manager.prepare_components')
            self.assertEqual(run_name, '__main__')
            self.assertIn(f'--project_dir={output / "dependencies"}', sys.argv)
            self.assertIn(f'--lock_path={output / "dependencies.lock"}', sys.argv)
            self.assertIn('--interface_version=3', sys.argv)
            self.assertEqual((output / 'dependencies/managed_components/vendor__codec/download.bin').read_bytes(),
                             b'validated-download\x00\xff')
            lock = output / 'dependencies.lock'
            locked = yaml.safe_load(lock.read_text())
            self.assertEqual(locked['manifest_hash'], expected_hash)
            # The SDK rewrites lock paths relative to the actual lock directory.
            # The next wrapper invocation must resolve these from lock.parent.
            for name, target in [('parent', self.local), ('child', self.nested)]:
                source = locked['dependencies'][name]['source']
                self.assertEqual((lock.parent / source['path']).resolve(), target)
                source['path'] = os.path.relpath(target, lock.parent)
            locked['manifest_hash'] = 'official-resolved-hash'
            lock.write_text(yaml.safe_dump(locked))
            raise SystemExit(0)
        return run

    def test_seed_reuse_and_nested_manifest_invalidation_are_output_local(self):
        first, second = self.outputs
        for output in self.outputs:
            self.configure(output, self.official_success(output, '0' * 64))
            self.assertTrue((output / 'dependencies/local_manifests.sha256').is_file())
        other_before = snapshot(second)
        first_before = snapshot(first)
        self.configure(first, self.official_success(first, 'official-resolved-hash'))
        self.assertEqual(snapshot(first), first_before)
        self.assertEqual(snapshot(second), other_before)
        # Change only a nested dependency constraint, without changing its version.
        (self.nested / 'idf_component.yml').write_text(
            'version: "1.0.0"\ndependencies:\n  vendor/codec: ">=2.0.0"\n')
        self.configure(first, self.official_success(first, '0' * 64))
        self.assertNotEqual((first / 'dependencies/local_manifests.sha256').read_bytes(),
                            first_before['dependencies/local_manifests.sha256'])
        self.assertEqual(snapshot(second), other_before)
        expected_project = dict(self.original)
        expected_project['components/parent/nested/idf_component.yml'] = (
            self.nested / 'idf_component.yml').read_bytes()
        self.assertEqual(snapshot(self.project), expected_project)
        # A transitive optional dependency can also change through sdkconfig.
        (first / 'config/sdkconfig.json').write_text('{"VIDEO_PROCESSOR_ENABLE": true}\n')
        self.configure(first, self.official_success(first, '0' * 64))
        self.assertEqual(snapshot(second), other_before)
        self.configure(first, self.official_success(first, 'official-resolved-hash'))

    def test_official_failure_propagates_without_success_stamp(self):
        for output, code in zip(self.outputs, [2, 'resolver failed']):
            with self.subTest(code=code):
                self.configure(output, self.official_success(output, '0' * 64))

                def fail(module, run_name):
                    raise SystemExit(code)
                with self.assertRaises(SystemExit) as caught:
                    self.invoke(output, fail)
                self.assertEqual(caught.exception.code, code)
                self.assertFalse((output / 'dependencies/local_manifests.sha256').exists())
                self.assertEqual(snapshot(self.project), self.original)

    def test_kconfig_retry_reuses_lock_until_final_configuration(self):
        for output, legacy in zip(self.outputs, [False, True]):
            with self.subTest(legacy=legacy):
                self.configure(output, self.official_success(output, '0' * 64))
                lock = output / 'dependencies.lock'
                stamp = output / 'dependencies/local_manifests.sha256'
                lock_before = lock.read_bytes()
                stamp_before = stamp.read_bytes()

                def retry(module, run_name):
                    self.assertIn('--use_sdk_json=FALSE', sys.argv)
                    self.assertEqual(lock.read_bytes(), lock_before)
                    raise SystemExit(10)

                with self.assertRaises(SystemExit) as caught:
                    self.invoke(output, retry, legacy=legacy, use_sdk_json='FALSE')
                self.assertEqual(caught.exception.code, 10)
                self.assertEqual(stamp.read_bytes(), stamp_before)

                if legacy:
                    # IDF 6.0 injects requirements between its two discovery
                    # passes; successful injection must preserve the retry cache.
                    self.invoke(output, self.official_success(output, 'official-resolved-hash'),
                                'inject_requirements', legacy=True)
                    self.assertEqual(stamp.read_bytes(), stamp_before)

                def complete(module, run_name):
                    self.assertIn('--use_sdk_json=TRUE', sys.argv)
                    self.assertEqual(lock.read_bytes(), lock_before)
                    if legacy:
                        raise SystemExit(0)

                self.invoke(output, complete, legacy=legacy, use_sdk_json='TRUE')
                self.assertFalse(stamp.exists())
                self.assertEqual(lock.read_bytes(), lock_before)
                manager.finalize_state(lock)
                self.assertEqual(stamp.read_bytes(), stamp_before)
                self.assertEqual(snapshot(self.project), self.original)

    def test_failure_after_kconfig_retry_removes_previous_stamp(self):
        for output, command in zip(self.outputs, ['prepare_dependencies', 'inject_requirements']):
            with self.subTest(command=command):
                self.configure(output, self.official_success(output, '0' * 64))
                with self.assertRaises(SystemExit) as caught:
                    self.invoke(output, SystemExit(10), use_sdk_json='FALSE')
                self.assertEqual(caught.exception.code, 10)
                self.assertTrue((output / 'dependencies/local_manifests.sha256').exists())
                with self.assertRaisesRegex(RuntimeError, 'resolver failed'):
                    self.invoke(output, RuntimeError('resolver failed'), command, use_sdk_json='TRUE')
                self.assertFalse((output / 'dependencies/local_manifests.sha256').exists())
                self.assertEqual(snapshot(self.project), self.original)

    def run_cmake(self, output, tail):
        # Exercise the actual common.cmake deferred hook. Only IDF's build
        # property lookup is a stub here; a real IDF project covers that boundary.
        common = Path(__file__).resolve().parents[1] / 'common.cmake'
        source = self.root / ('cmake-' + output.name)
        source.mkdir()
        (source / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.22)
include("{common}")
project(final_dependency_state NONE)
function(idf_build_get_property destination property)
    if(property STREQUAL "PYTHON")
        set(${{destination}} "{sys.executable}" PARENT_SCOPE)
    elseif(property STREQUAL "DEPENDENCIES_LOCK")
        set(${{destination}} "{output / 'dependencies.lock'}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unexpected property: ${{property}}")
    endif()
endfunction()
if(EXISTS "{output / 'dependencies/local_manifests.sha256'}")
    message(FATAL_ERROR "Dependency state was recorded before final Kconfig")
endif()
{tail}
''')
        environment = dict(os.environ)
        environment.pop('IDF_COMPONENT_WRAPPER', None)
        return subprocess.run(['cmake', '-S', str(source), '-B', str(output)],
                              capture_output=True, text=True, env=environment)

    def test_cmake_records_final_configuration_and_preserves_other_output(self):
        first, second = self.outputs
        self.configure(second, self.official_success(second, '0' * 64))
        other_before = snapshot(second)
        self.invoke(first, self.official_success(first, '0' * 64))
        preliminary = manager.local_manifest_hash(first / 'dependencies.lock')
        final_json = first / 'config/sdkconfig.json'
        result = self.run_cmake(first, f'file(WRITE "{final_json}" "{{\\"NEW_COMPONENT_CONFIG\\": true}}\\n")')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        final_hash = manager.local_manifest_hash(first / 'dependencies.lock')
        self.assertNotEqual(final_hash, preliminary)
        self.assertEqual((first / 'dependencies/local_manifests.sha256').read_text(), final_hash)
        self.assertEqual(snapshot(second), other_before)
        self.assertEqual(snapshot(self.project), self.original)

    def test_injection_preserves_resolved_lock_without_early_stamp(self):
        output = self.outputs[0]
        self.invoke(output, self.official_success(output, '0' * 64))
        self.invoke(output, self.official_success(output, 'official-resolved-hash'), 'inject_requirements')
        self.assertFalse((output / 'dependencies/local_manifests.sha256').exists())
        manager.finalize_state(output / 'dependencies.lock')
        self.assertEqual(yaml.safe_load((output / 'dependencies.lock').read_text())['manifest_hash'],
                         'official-resolved-hash')

    def test_finalizer_requires_lock_and_final_kconfig(self):
        first, second = self.outputs
        with self.assertRaises(FileNotFoundError):
            manager.finalize_state(first / 'dependencies.lock')
        self.invoke(second, self.official_success(second, '0' * 64))
        (second / 'config/sdkconfig.json').unlink()
        result = self.run_cmake(second, '')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Final dependency lock and Kconfig JSON required', result.stderr)
        self.assertFalse((second / 'dependencies/local_manifests.sha256').exists())

    def test_cmake_failure_does_not_record_success(self):
        output = self.outputs[0]
        self.configure(output, self.official_success(output, '0' * 64))
        self.invoke(output, self.official_success(output, 'official-resolved-hash'))
        result = self.run_cmake(output, 'message(FATAL_ERROR "Final configuration failed")')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Final configuration failed', result.stderr)
        self.assertFalse((output / 'dependencies/local_manifests.sha256').exists())


if __name__ == '__main__':
    unittest.main()
