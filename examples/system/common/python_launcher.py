# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Adapt legacy IDF's Python command without changing its Python environment."""

import os
from pathlib import Path
import shlex
import shutil
import sys


def create_launcher(destination, python):
    """Create a single executable path for IDF's quoted and unquoted calls."""
    if not Path(python).is_absolute():
        resolved_python = shutil.which(python)
        if not resolved_python:
            raise FileNotFoundError(f'Python interpreter not found: {python}')
        python = resolved_python
    helper = str(Path(__file__).resolve())
    lock = str(destination.parent / 'dependencies.lock')
    if os.name == 'nt':
        destination = destination.with_suffix('.cmd')
        # CMake executes batch launchers using cmd /c. Preserve the original
        # command's quoting via %*, as Python's Windows launchers do.
        content = (f'@"{python}" "{helper}" --python "{python}" --lock-path "{lock}" %*\r\n'
                   '@exit /b %errorlevel%\r\n')
    else:
        destination = destination.with_suffix('.sh')
        content = (f'#!/bin/sh\nexec {shlex.quote(python)} {shlex.quote(helper)} '
                   f'--python {shlex.quote(python)} --lock-path {shlex.quote(lock)} "$@"\n')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(content.encode())
    destination.chmod(0o755)
    return destination


def command(python, arguments, lock):
    """Intercept only IDF's component-manager module; forward every other call."""
    # Bootloader subprojects inherit PYTHON but have their own manager state.
    # They must keep the SDK's ordinary invocation, including an empty lock path.
    lock_arguments = [argument.split('=', 1)[1] for argument in arguments if argument.startswith('--lock_path=')]
    # CMake uses forward slashes on Windows; pathlib produces backslashes.
    selected_lock = os.path.normcase(os.path.normpath(lock))
    if (arguments[:2] == ['-m', 'idf_component_manager.prepare_components'] and
            any(value and os.path.normcase(os.path.normpath(value)) == selected_lock for value in lock_arguments)):
        wrapper = str(Path(__file__).with_name('component_manager.py'))
        return [python, wrapper, *arguments[1:]]
    return [python, *arguments]


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--create':
        print(create_launcher(Path(sys.argv[2]), sys.argv[3]))
    elif len(sys.argv) >= 5 and sys.argv[1] == '--python' and sys.argv[3] == '--lock-path':
        python = sys.argv[2]
        os.execv(python, command(python, sys.argv[5:], sys.argv[4]))
    else:
        raise SystemExit('Expected --create DESTINATION PYTHON or --python PYTHON --lock-path LOCK [ARGUMENT ...]')


if __name__ == '__main__':
    main()
