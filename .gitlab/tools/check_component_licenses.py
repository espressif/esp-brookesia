#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Check that each ESP-IDF component manifest has a sibling license.txt.

import argparse
import sys
from pathlib import Path
from typing import Iterable, List


EXCLUDED_DIR_NAMES = {'components', 'managed_components', 'build', 'main'}


def is_excluded_path(path: Path) -> bool:
    return any(part in EXCLUDED_DIR_NAMES or part.startswith('build_') for part in path.parts)


def iter_component_manifests(repo_root: Path) -> Iterable[Path]:
    for manifest in repo_root.rglob('idf_component.yml'):
        relative_path = manifest.relative_to(repo_root)
        if is_excluded_path(relative_path):
            continue
        yield manifest


def find_missing_license_dirs(repo_root: Path) -> List[Path]:
    missing_license_dirs = []
    for manifest in iter_component_manifests(repo_root):
        component_dir = manifest.parent
        if not (component_dir / 'license.txt').is_file():
            missing_license_dirs.append(component_dir.relative_to(repo_root))
    return sorted(missing_license_dirs)


def check_component_licenses(repo_root: Path) -> int:
    missing_license_dirs = find_missing_license_dirs(repo_root)
    if not missing_license_dirs:
        print('All ESP-IDF components have license.txt')
        return 0

    print('The following ESP-IDF component directories are missing license.txt:')
    for component_dir in missing_license_dirs:
        print(f'  - {component_dir}')

    return 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Check that each idf_component.yml has a sibling license.txt'
    )
    parser.add_argument(
        '--repo-root',
        default='.',
        type=Path,
        help='Repository root to scan, defaults to current directory',
    )
    args = parser.parse_args()

    sys.exit(check_component_licenses(args.repo_root.resolve()))
