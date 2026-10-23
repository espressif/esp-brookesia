# SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""
This file is used in CI generate binary files for different kinds of apps
"""

import argparse
import sys
import os
import re
import logging
from pathlib import Path
from typing import List
import typing as t
import subprocess
import shutil
import shlex
import tempfile
from contextlib import contextmanager

from idf_build_apps import App, build_apps, find_apps, setup_logging
from idf_build_apps.app import CMakeApp
from idf_build_apps.constants import BuildStatus
from idf_build_apps.utils import BuildError, subprocess_run

logger = logging.getLogger('idf_build_apps')
IDF_PATH = os.getenv('IDF_PATH', '')

PROJECT_ROOT = Path(__file__).parent.parent.parent.absolute()
APPS_BUILD_PER_JOB = 30
# Targets that require ``idf.py --preview`` to be set
PREVIEW_TARGETS = {'esp32s31'}
IGNORE_WARNINGS = [
    r"warning: unused variable 'head'",
    r"WARNING: The following Kconfig variables were used in",
    r"unknown kconfig symbol",
    r"Warning: esp_board_manager dependency not found",
    # Temporary workaround for `espressif__esp_video` Kconfig symbols warnings
    r"warning: 'ISP_AWB_SAMPLE_POINT_BEFORE_CCM' is deprecated: Use ISP_AWB_SAMPLE_POINT_0 instead",
    r"warning: 'ISP_AE_SAMPLE_POINT_AFTER_DEMOSAIC' is deprecated: Use ISP_AE_SAMPLE_POINT_0 instead",
    r"warning: 'overlay_dst_cache_pixel_bytes' defined but not used",
    # Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
    r"warning: 'strncpy' output may be truncated copying 31 bytes from a string of length 31",
    r"warning: 'strncpy' output may be truncated copying 79 bytes from a string of length 79",
]


@contextmanager
def limit_ninja_jobs(jobs: t.Optional[int]):
    if jobs is None:
        yield
        return
    ninja = shutil.which('ninja')
    if ninja is None:
        raise BuildError('Ninja is required when --build-jobs is set')
    original_path = os.environ.get('PATH', '')
    original_jobs = os.environ.get('IDF_PY_BUILD_JOBS')
    with tempfile.TemporaryDirectory(prefix='brookesia-ci-ninja-') as wrapper_dir:
        # IDF 6.0 has no -j option. PATH also covers bootloader's nested Ninja.
        wrapper = Path(wrapper_dir) / 'ninja'
        wrapper.write_text(f'#!/bin/sh\nexec {shlex.quote(str(Path(ninja).resolve()))} -j {jobs} "$@"\n', encoding='utf-8')
        wrapper.chmod(0o755)
        os.environ['PATH'] = wrapper_dir + os.pathsep + original_path
        os.environ['IDF_PY_BUILD_JOBS'] = str(jobs)
        logger.info('Limiting each Ninja invocation to %d parallel jobs', jobs)
        try:
            yield
        finally:
            os.environ['PATH'] = original_path
            if original_jobs is None:
                os.environ.pop('IDF_PY_BUILD_JOBS', None)
            else:
                os.environ['IDF_PY_BUILD_JOBS'] = original_jobs


def positive_integer(value: str) -> int:
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError('must be a positive integer')
    return result


class CustomApp(CMakeApp):
    build_system: t.Literal['custom'] = 'custom'  # Must be unique to identify your custom app type

    def _pre_build(self) -> None:
        if self.is_system_example() and (Path(self.work_dir) / 'sdkconfig').exists():
            raise BuildError(
                f'System example CI builds require a checkout without {Path(self.work_dir) / "sdkconfig"}. '
                'Use idf.py -B <build-dir> to preserve an existing configuration.'
            )
        super()._pre_build()

    def _build(
        self,
        *,
        manifest_rootpath: t.Optional[str] = None,
        modified_components: t.Optional[t.List[str]] = None,
        modified_files: t.Optional[t.List[str]] = None,
        check_app_dependencies: bool = False,
    ) -> None:
        system_example = self.is_system_example()
        if system_example:
            # These projects generate sdkconfig inside each build directory.
            self.copy_sdkconfig = False
            subprocess_run(
                [
                    sys.executable, '-B', '-m', 'unittest', 'discover',
                    '-s', str(PROJECT_ROOT / 'examples/system/common'),
                    '-p', 'test_component_manager.py',
                ],
                log_fs=self.build_log_path,
                check=True,
                cwd=self.work_dir,
            )
        self._pre_hook()
        super()._build(
            manifest_rootpath=manifest_rootpath,
            modified_components=modified_components,
            modified_files=modified_files,
            check_app_dependencies=check_app_dependencies,
        )
        if system_example and self.build_status == BuildStatus.SUCCESS:
            subprocess_run(
                [
                    sys.executable,
                    str(PROJECT_ROOT / 'examples/system/common/check_build_isolation.py'),
                    str(Path(self.build_path).resolve()),
                ],
                log_fs=self.build_log_path,
                check=True,
                cwd=self.work_dir,
            )

    def is_system_example(self) -> bool:
        return Path(self.app_dir).resolve() in {
            PROJECT_ROOT / 'examples/system/super',
            PROJECT_ROOT / 'examples/system/gsp_app',
        }

    def is_board_manager_project(self) -> bool:
        main_yml_path = Path(self.work_dir) / 'main' / 'idf_component.yml'
        with open(main_yml_path) as f:
            content = f.read()
        return 'esp_board_manager' in content or 'brookesia_hal_boards' in content

    def get_board_name_from_sdkconfig(self, sdkconfig_path: str) -> str:
        board_name = ''
        sdkconfig_name = Path(sdkconfig_path).name
        if sdkconfig_name.startswith('sdkconfig.ci.board.'):
            board_name = sdkconfig_name[len('sdkconfig.ci.board.'):]
        if board_name:
            return board_name
        try:
            for line in Path(sdkconfig_path).read_text(encoding='utf-8').splitlines():
                marker = '# ci-board:'
                if line.strip().startswith(marker):
                    return line.split(':', 1)[1].strip()
        except OSError:
            return ''
        return ''

    def clear_project_generated_files(self):
        proj_path = Path(self.work_dir).absolute()
        # Remove directories
        shutil.rmtree(proj_path / 'managed_components', ignore_errors=True)
        shutil.rmtree(proj_path / 'components' / 'gen_bmgr_codes', ignore_errors=True)
        shutil.rmtree(proj_path / 'build', ignore_errors=True)
        # Remove files
        (proj_path / 'dependencies.lock').unlink(missing_ok=True)
        (proj_path / 'sdkconfig').unlink(missing_ok=True)
        (proj_path / 'board_manager.defaults').unlink(missing_ok=True)

    def _pre_hook(self):
        board_name = ''
        if self.is_board_manager_project():
            for sdkconfig_path in self.sdkconfig_files:
                board_name = self.get_board_name_from_sdkconfig(sdkconfig_path)
                if board_name:
                    break
        print(f'== Pre build hook for app: {self.name} at \'{self.work_dir}\' for target: {self.target}, board:'
              f'{board_name}, argv: {sys.argv}, sdkconfig_files: {self.sdkconfig_files}')
        if not self.is_system_example():
            self.clear_project_generated_files()
        if board_name == '':
            print(f'== No board name found, skip setting board manager config')
            return  # no board name found, skip the pre build hook
        subprocess.run(
            [sys.executable, f'{IDF_PATH}/tools/idf.py', 'gen-bmgr-config', '-b', board_name],
            cwd=self.work_dir,
            check=True,
        )
        if self.is_system_example():
            # CMakeApp supplies the target and build directory to the real build.
            return
        set_target_cmd = [sys.executable, f'{IDF_PATH}/tools/idf.py']
        if self.target in PREVIEW_TARGETS:
            set_target_cmd.append('--preview')
        set_target_cmd.extend(['set-target', self.target])
        subprocess.run(set_target_cmd, cwd=self.work_dir, check=True)


def _get_idf_version():
    if os.environ.get('IDF_VERSION'):
        return os.environ.get('IDF_VERSION')
    version_path = os.path.join(os.environ['IDF_PATH'], 'tools/cmake/version.cmake')
    regex = re.compile(r'^\s*set\s*\(\s*IDF_VERSION_([A-Z]{5})\s+(\d+)')
    ver = {}
    with open(version_path) as f:
        for line in f:
            m = regex.match(line)
            if m:
                ver[m.group(1)] = m.group(2)
    return '{}.{}'.format(int(ver['MAJOR']), int(ver['MINOR']))


def get_cmake_apps(
    paths,
    target,
    config_rules_str,
    default_build_targets,
):  # type: (List[str], str, List[str]) -> List[App]
    idf_ver = _get_idf_version()
    apps = find_apps(
        paths,
        recursive=True,
        target=target,
        build_dir=f'{idf_ver}/build_@t_@w',
        config_rules_str=config_rules_str,
        build_log_filename='build_log.txt',
        size_json_filename='size.json',
        check_warnings=False,
        no_preserve=False,
        default_build_targets=default_build_targets,
        manifest_files=[
            str(Path(PROJECT_ROOT) / '.build-rules.yml'),
        ],
        build_system=CustomApp,
    )
    return apps


def main(args):  # type: (argparse.Namespace) -> None
    default_build_targets = args.default_build_targets.split(',') if args.default_build_targets else None
    # Handle default config values if not provided
    if args.config:
        # Support both semicolon-separated strings and multiple --config arguments
        config_list = []
        for config_item in args.config:
            # Split by semicolon if present, otherwise use as-is
            if ';' in config_item:
                config_list.extend([item.strip() for item in config_item.split(';') if item.strip()])
            else:
                config_list.append(config_item)
    else:
        config_list = ['sdkconfig.defaults=defaults', 'sdkconfig.ci.*=', '=defaults']
    apps = get_cmake_apps(args.paths, args.target, config_list, default_build_targets)

    if args.find:
        if args.output:
            os.makedirs(os.path.dirname(os.path.realpath(args.output)), exist_ok=True)
            with open(args.output, 'w') as fw:
                for app in apps:
                    fw.write(app.to_json() + '\n')
        else:
            for app in apps:
                print(app)

        sys.exit(0)

    if args.exclude_apps:
        apps_to_build = [app for app in apps if app.name not in args.exclude_apps]
    else:
        apps_to_build = apps[:]

    logger.info('Found %d apps after filtering', len(apps_to_build))
    logger.info(
        'Suggest setting the parallel count to %d for this build job',
        len(apps_to_build) // APPS_BUILD_PER_JOB + 1,
    )

    with limit_ninja_jobs(args.build_jobs):
        ret_code = build_apps(
            apps_to_build,
            parallel_count=args.parallel_count,
            parallel_index=args.parallel_index,
            dry_run=False,
            collect_size_info_filename=args.collect_size_info,
            keep_going=True,
            ignore_warning_strs=IGNORE_WARNINGS,
            copy_sdkconfig=True,
            no_preserve=False,
        )

    sys.exit(ret_code)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Build all the apps for different test types. Will auto remove those non-test apps binaries',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument('paths', nargs='*', help='Paths to the apps to build.')
    parser.add_argument(
        '-t', '--target',
        default='all',
        help='Build apps for given target. could pass "all" to get apps for all targets',
    )
    parser.add_argument(
        '--config',
        default=None,
        action='append',
        help='Adds configurations (sdkconfig file names) to build. This can either be '
        'FILENAME[=NAME] or FILEPATTERN. FILENAME is the name of the sdkconfig file, '
        'relative to the project directory, to be used. Optional NAME can be specified, '
        'which can be used as a name of this configuration. FILEPATTERN is the name of '
        'the sdkconfig file, relative to the project directory, with at most one wildcard. '
        'The part captured by the wildcard is used as the name of the configuration. '
        'Can be specified multiple times to add multiple configurations, or use semicolon '
        'to separate multiple configs in a single argument: --config="config1;config2;config3". '
        'If not specified, defaults to: sdkconfig.defaults=defaults, sdkconfig.ci.*=, =defaults. '
    )
    parser.add_argument(
        '--parallel-count', default=1, type=int, help='Number of parallel build jobs.'
    )
    parser.add_argument(
        '--build-jobs', type=positive_integer,
        help='Maximum parallel jobs per Ninja invocation, including the bootloader. Default: Ninja chooses.',
    )
    parser.add_argument(
        '--parallel-index',
        default=1,
        type=int,
        help='Index (1-based) of the job, out of the number specified by --parallel-count.',
    )
    parser.add_argument(
        '--collect-size-info',
        type=str,
        help='If specified, the test case name and size info json will be written to this file',
    )
    parser.add_argument(
        '--exclude-apps',
        nargs='*',
        help='Exclude build apps',
    )
    parser.add_argument(
        '--default-build-targets',
        default=None,
        help='default build targets used in manifest files',
    )
    parser.add_argument(
        '-v', '--verbose',
        action='count', default=0,
        help='Show verbose log message',
    )
    parser.add_argument(
        '--find',
        action='store_true',
        help='Find the buildable applications. If enable this option, build options will be ignored.',
    )
    parser.add_argument(
        '-o', '--output',
        help='Print the found apps to the specified file instead of stdout'
    )

    arguments = parser.parse_args()
    if not arguments.paths:
        arguments.paths = [PROJECT_ROOT]
    setup_logging(verbose=arguments.verbose)  # Info
    main(arguments)
