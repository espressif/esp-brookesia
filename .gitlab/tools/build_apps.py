# SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
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

from idf_build_apps import App, build_apps, find_apps, setup_logging
from idf_build_apps.app import CMakeApp

logger = logging.getLogger('idf_build_apps')
IDF_PATH = os.getenv("IDF_PATH", "")

PROJECT_ROOT = Path(__file__).parent.parent.parent.absolute()
APPS_BUILD_PER_JOB = 30
IGNORE_WARNINGS = [
    r"warning: unused variable 'head'",
    r"WARNING: The following Kconfig variables were used in",
    r"unknown kconfig symbol",
]

class CustomApp(CMakeApp):
    build_system: t.Literal['custom'] = 'custom'  # Must be unique to identify your custom app type

    def _build(
        self,
        *,
        manifest_rootpath: t.Optional[str] = None,
        modified_components: t.Optional[t.List[str]] = None,
        modified_files: t.Optional[t.List[str]] = None,
        check_app_dependencies: bool = False,
    ) -> None:
        if self.is_board_manager_project():
            self._pre_hook()
        else:
            print('== This is not a board manager project, running normal build')
        super()._build(
            manifest_rootpath=manifest_rootpath,
            modified_components=modified_components,
            modified_files=modified_files,
            check_app_dependencies=check_app_dependencies,
        )

    def is_board_manager_project(self) -> bool:
        main_yml_path = Path(self.work_dir)/"main"/"idf_component.yml"
        with open(main_yml_path) as f:
            return 'esp_board_manager' in f.read()

    def get_board_name_from_sdkconfig(self, sdkconfig_path: str) -> str:
        board_name = ''
        sdkconfig_path = Path(sdkconfig_path).name
        if sdkconfig_path.startswith('sdkconfig.ci.board.'):
            board_name = sdkconfig_path[len('sdkconfig.ci.board.'):]
        return board_name

    def clear_project_generated_files(self):
        proj_path = Path(self.work_dir).absolute()
        shutil.rmtree(proj_path/"managed_components", ignore_errors=True)
        shutil.rmtree(proj_path/"components"/"gen_bmgr_codes", ignore_errors=True)
        (proj_path/'board_manager.defaults').unlink(missing_ok=True)

    def _pre_hook(self):
        board_name = self.get_board_name_from_sdkconfig(self.sdkconfig_files[1])
        print(f'== Pre build hook for app: {self.name} at \'{self.work_dir}\' for target: {self.target}, board: {board_name}, argv: {sys.argv}, sdkconfig_files: {self.sdkconfig_files}')
        self.clear_project_generated_files()
        if board_name == '':
            print(f'== No board name found, skip the pre build hook')
            return # no board name found, skip the pre build hook
        subprocess.run([sys.executable, f"{IDF_PATH}/tools/idf.py", "gen-bmgr-config", "-c", str(Path(self.work_dir).absolute()/"boards"), "-b", board_name], cwd=self.work_dir)

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
        check_warnings=True,
        no_preserve=False,
        default_build_targets=default_build_targets,
        manifest_files=[
            str(Path(PROJECT_ROOT)/'.build-rules.yml'),
        ],
        build_system=CustomApp,
    )
    return apps


def main(args):  # type: (argparse.Namespace) -> None
    default_build_targets = args.default_build_targets.split(',') if args.default_build_targets else None
    apps = get_cmake_apps(args.paths, args.target, args.config, default_build_targets)

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

    ret_code = build_apps(
        apps_to_build,
        parallel_count=args.parallel_count,
        parallel_index=args.parallel_index,
        dry_run=False,
        collect_size_info=args.collect_size_info,
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
        default=['sdkconfig.defaults=defaults', 'sdkconfig.ci.*=', '=defaults'],
        action='append',
        help='Adds configurations (sdkconfig file names) to build. This can either be '
        'FILENAME[=NAME] or FILEPATTERN. FILENAME is the name of the sdkconfig file, '
        'relative to the project directory, to be used. Optional NAME can be specified, '
        'which can be used as a name of this configuration. FILEPATTERN is the name of '
        'the sdkconfig file, relative to the project directory, with at most one wildcard. '
        'The part captured by the wildcard is used as the name of the configuration.',
    )
    parser.add_argument(
        '--parallel-count', default=1, type=int, help='Number of parallel build jobs.'
    )
    parser.add_argument(
        '--parallel-index',
        default=1,
        type=int,
        help='Index (1-based) of the job, out of the number specified by --parallel-count.',
    )
    parser.add_argument(
        '--collect-size-info',
        type=argparse.FileType('w'),
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
