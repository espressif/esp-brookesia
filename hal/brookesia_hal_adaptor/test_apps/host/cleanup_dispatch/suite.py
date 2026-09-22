# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise the production Board Manager facade with arbitrary cleanup providers."""

import os
from pathlib import Path
import signal
import shutil
import subprocess
import tempfile


def run():
    here = Path(__file__).resolve().parent
    root = here.parents[4]
    adaptor = root / 'hal/brookesia_hal_adaptor'
    custom = root / 'hal/brookesia_hal_boards/components/mosaico/brookesia_hal_custom'
    utils = root / 'utils/brookesia_lib_utils'
    compiler = os.environ.get('CXX') or shutil.which('g++-13') or 'c++'
    cases = ('passthrough', 'registration', 'allocation_failure', 'late_registration',
             'first_failure', 'retained_retry', 'failed_lookup', 'live_handle',
             'restart_required', 'error_priority', 'global_error', 'global_priority')
    with tempfile.TemporaryDirectory(prefix='brookesia-cleanup-dispatch-') as directory:
        build = Path(directory)
        for filename in ('esp_err.h', 'esp_board_manager.h', 'esp_board_periph.h', 'esp_board_device.h'):
            (build / filename).write_text('#include "sdk_stubs.hpp"\n')
        (build / 'sdkconfig.h').write_text('')
        (build / 'esp_blockdev.h').write_text('#pragma once\ntypedef void *esp_blockdev_handle_t;\n')
        (build / 'driver').mkdir()
        (build / 'driver/gpio.h').write_text('#pragma once\ntypedef int gpio_num_t;\n')
        log = build / 'brookesia/lib_utils/log.hpp'
        log.parent.mkdir(parents=True)
        log.write_text('#pragma once\n#include <cassert>\n#define BROOKESIA_LOGE(...) ((void)0)\n')
        includes = (build, here, adaptor / 'include', adaptor / 'src',
                    root / 'hal/brookesia_hal_interface/include', utils / 'include',
                    custom / 'include', custom / 'src')
        flags = ['-std=c++23', '-Wall', '-Wextra', '-Werror', '-pthread',
                 '-fsanitize=undefined', '-ffunction-sections', '-fdata-sections']
        flags += [f'-I{path}' for path in includes]

        # Keep harness assertions enabled even when production sources use NDEBUG.
        harnesses = {}
        for name, defines in (
                ('generic', []),
                ('mosaico', ['-DTEST_MOSAICO_REGISTRATION=1']),
                ('startup_failure', ['-DTEST_MOSAICO_REGISTRATION=1', '-DTEST_STARTUP_REGISTRATION_FAILURE=1'])):
            obj = build / f'{name}.o'
            subprocess.run([compiler, *flags, *defines, '-c', str(here / 'test_main.cpp'),
                            '-o', str(obj)], check=True)
            harnesses[name] = str(obj)

        for method in (0, 1, 2):
            for ndebug in (False, True):
                label = f'check method {method}, assertions {"off" if ndebug else "on"}'
                production_flags = [*flags, f'-DBROOKESIA_UTILS_CHECK_HANDLE_METHOD={method}']
                if ndebug:
                    production_flags.append('-DNDEBUG')
                manager = build / 'board_manager.o'
                subprocess.run([compiler, *production_flags, '-c',
                                str(adaptor / 'src/private/board_manager.cpp'), '-o', str(manager)], check=True)
                executable = build / 'test'
                subprocess.run([compiler, *flags, harnesses['generic'], str(manager),
                                '-Wl,--gc-sections', '-o', str(executable)], check=True)
                selected_cases = cases if method == 0 and not ndebug else ('allocation_failure',)
                for case in selected_cases:
                    subprocess.run([str(executable), case], check=True, timeout=5)
                    print(f'PASS cleanup dispatch: {case} ({label})', flush=True)

                # Preserve the real startup registrar from an archive exactly as firmware does.
                obj = build / 'device_cleanup.o'
                archive = build / 'libboard.a'
                subprocess.run([compiler, *production_flags,
                                '-DMOSAICO_DEVICE_CLEANUP_PLUGIN_SYMBOL=mosaico_device_cleanup_plugin_symbol',
                                '-c', str(custom / 'src/board/device_cleanup.cpp'), '-o', str(obj)], check=True)
                subprocess.run(['ar', 'rcs', str(archive), str(obj)], check=True)
                link_flags = ['-Wl,--gc-sections,-u,mosaico_device_cleanup_plugin_symbol']
                subprocess.run([compiler, *flags, harnesses['mosaico'], str(manager), str(archive),
                                *link_flags, '-o', str(executable)], check=True)
                subprocess.run([str(executable), 'mosaico_registration'], check=True, timeout=5)
                print(f'PASS cleanup dispatch: linked Mosaico startup registration ({label})', flush=True)

                subprocess.run([compiler, *flags, harnesses['startup_failure'], str(manager), str(archive),
                                *link_flags, '-Wl,--wrap=brookesia_hal_board_manager_register_device_cleanup',
                                '-o', str(executable)], check=True)
                result = subprocess.run([str(executable), 'startup_failure'], timeout=5)
                if result.returncode != -signal.SIGABRT:
                    raise RuntimeError(f'Startup must abort on registration failure ({label}); '
                                       f'exit code: {result.returncode}')
                print(f'PASS cleanup dispatch: failed Mosaico startup registration ({label})', flush=True)
