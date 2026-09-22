# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run host regressions without flashing hardware or modifying dependencies."""

import argparse
from pathlib import Path
import shutil
import sys

# Suites generate build files in temporary directories, not beside the sources.
sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=('core', 'dependencies', 'all'), default='core')
    parser.add_argument('--managed-components', type=Path,
                        help='Existing dependencies from an ESP-IDF project configured with Brookesia')
    args = parser.parse_args()
    if shutil.which('cc') is None:
        parser.error('A host C compiler is required')
    if args.suite != 'core':
        if args.managed_components is None or not args.managed_components.is_dir():
            parser.error('--managed-components must point to existing configured dependencies')
        args.managed_components = args.managed_components.resolve()

    if args.suite in ('core', 'all'):
        from lifecycle.suite import run as run_lifecycle
        from board_cleanup import run as run_board_cleanup
        from cleanup_dispatch.suite import run as run_cleanup_dispatch

        print('Running core regressions: lifecycle, cleanup dispatch and Mosaico cleanup', flush=True)
        run_cleanup_dispatch()
        run_lifecycle()
        run_board_cleanup()
    if args.suite in ('dependencies', 'all'):
        from board_manager.suite import run as run_board_manager
        from media.suite import run as run_media

        print(f'Running dependency regressions against {args.managed_components}', flush=True)
        run_board_manager(args.managed_components)
        run_media(args.managed_components)
    print(f'PASS: {args.suite} host regressions', flush=True)


if __name__ == '__main__':
    main()
