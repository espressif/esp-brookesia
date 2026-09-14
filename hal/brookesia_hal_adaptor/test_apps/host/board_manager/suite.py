# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check patch configuration and Board Manager 0.5.15 cleanup failures.

Resolved dependencies are read from the supplied managed_components directory.
Source copies, driver stubs and executables stay in a temporary directory.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
ADAPTOR = HERE.parents[2]
COMMON_HEADERS = {
    'esp_err.h': '''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
static inline const char *esp_err_to_name(int e) { (void)e; return "injected"; }
''',
    'esp_log.h': '''#pragma once
#include <stdio.h>
#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGW ESP_LOGE
#define ESP_LOGI ESP_LOGE
#define ESP_LOGD ESP_LOGE
''',
}
PERIPH_HEADERS = {
    'esp_check.h': '',
    'esp_board_extra_func_entry.h': '#pragma once\n',
    'esp_board_find_utils.h': '''#pragma once
#include "esp_board_periph.h"
const esp_board_periph_desc_t *esp_board_find_periph_desc(const char *name);
esp_board_periph_entry_t *esp_board_find_periph_handle(const char *type, esp_board_periph_role_t role);
''',
}
DVP_HEADERS = {
    'esp_video_init.h': '''#pragma once
#include "esp_err.h"
#define CONFIG_SOC_LCDCAM_CAM_SUPPORTED 1
typedef struct { int data_io[8]; } esp_cam_ctlr_dvp_pin_config_t;
typedef struct {
    struct { bool init_sccb; void *i2c_handle; uint32_t freq; } sccb_config;
    int reset_pin, pwdn_pin;
    esp_cam_ctlr_dvp_pin_config_t dvp_pin;
    uint32_t xclk_freq;
} esp_video_init_dvp_config_t;
typedef struct { const esp_video_init_dvp_config_t *dvp; } esp_video_init_config_t;
int esp_video_init(const esp_video_init_config_t *config);
int esp_video_deinit(void);
''',
    'driver/gpio.h': '#pragma once\ntypedef int gpio_num_t;\n',
    'esp_cam_ctlr_dvp.h': '#pragma once\n',
    'esp_cam_sensor_xclk.h': '#pragma once\ntypedef int esp_cam_sensor_xclk_config_t;\n',
    'esp_video_device.h': '#define ESP_VIDEO_DVP_DEVICE_NAME "/dev/video2"\n',
    'esp_board_periph.h': '''#pragma once
int esp_board_periph_ref_handle(const char *name, void **handle);
int esp_board_periph_unref_handle(const char *name);
''',
    'esp_board_device.h': '''#pragma once
int esp_board_device_get_config_by_handle(void *handle, void **config);
''',
    'esp_board_entry.h': '''#pragma once
typedef struct {
    int (*init_func)(void *config, int size, void **handle);
    int (*deinit_func)(void *handle);
} esp_board_entry_desc_t;
const esp_board_entry_desc_t *esp_board_entry_find_subtype_desc(const char *device, const char *subtype);
#define ESP_BOARD_SUBTYPE_ENTRY_IMPLEMENT(...)
''',
}
PATCHES = (
    ('esp_board_manager', 'espressif__esp_board_manager', 'esp_board_manager_periph_deinit_retry'),
    ('esp_board_manager', 'espressif__esp_board_manager', 'esp_board_manager_dvp_camera_deinit'),
    ('esp_video', 'espressif__esp_video', 'esp_video_dvp_deinit_order'),
    ('esp_capture', 'espressif__esp_capture', 'esp_capture_v4l2_uyvy_support'),
    ('av_processor', 'jason-mao__av_processor', 'av_processor_frame_mode_stop'),
    ('media_lib_sal', 'espressif__media_lib_sal', 'media_lib_sal_esp_tls_idf6'),
)


def _copy_sources(component: Path, destination: Path, files):
    for name in files:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(component / name, target)


def _prepare_patch(source: Path, patch: Path, *, pristine=False):
    command = ['git', 'apply', '--ignore-space-change']
    patched = subprocess.run(command + ['--reverse', '--check', str(patch)],
                             cwd=source, capture_output=True).returncode == 0
    if patched and pristine:
        subprocess.run(command + ['--reverse', str(patch)], cwd=source, check=True)
    elif not patched and not pristine:
        subprocess.run(command + [str(patch)], cwd=source, check=True)


def _run_driver(component: Path, temporary: Path, name, files, patch_name, headers, cases):
    source = temporary / name
    _copy_sources(component, source, files)
    _prepare_patch(source, ADAPTOR / 'tools' / (patch_name + '.patch'))
    for filename, content in (COMMON_HEADERS | headers).items():
        target = source / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    executable = source / 'test'
    command = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined']
    if name == 'dvp':
        # The upstream dispatcher checks cfg_size; DVP init leaves it unused.
        command += ['-Wno-unused-parameter', '-Wno-sign-compare']
    command += ['-I', str(source), '-I', str(component / 'include'),
                str(HERE / f'test_{name}.c'), '-o', str(executable)]
    subprocess.run(command, check=True)
    for case in cases:
        subprocess.run([str(executable), case], check=True, timeout=5)
        print(f'PASS {name} {case}')


def _check_patch_configure(managed_components: Path, temporary: Path):
    functions = (ADAPTOR / 'CMakeLists.txt').read_text().split(
        'set(BROOKESIA_HAL_ADAPTOR_NEEDS_BOARD_MANAGER FALSE)', 1)[0]
    script = temporary / 'test.cmake'
    for short, qualified, patch_name in PATCHES:
        patch = ADAPTOR / 'tools' / (patch_name + '.patch')
        files = re.findall(r'^--- a/(.+)$', patch.read_text(), re.M)
        source = temporary / patch_name
        _copy_sources(managed_components / qualified, source, files)
        _prepare_patch(source, patch, pristine=True)
        for alias in (short, qualified):
            boilerplate = f'''cmake_minimum_required(VERSION 3.16)
{functions}
find_package(Git REQUIRED)
function(idf_build_get_property output property)
    set(${{output}} "{alias}" PARENT_SCOPE)
endfunction()
function(idf_component_get_property output component property)
    set(${{output}} "{source}" PARENT_SCOPE)
endfunction()
'''
            apply = f'brookesia_hal_adaptor_apply_component_patch("{alias}" "{patch}" "{files[0]}" REQUIRED)\n'
            script.write_text(boilerplate + apply + apply)
            subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True)
        subprocess.run(['git', 'apply', '--reverse', '--check', '--ignore-space-change', str(patch)],
                       cwd=source, check=True, capture_output=True)
        print('PASS apply/idempotence/both aliases:', patch_name)

    # Corrupt the final patch's source to check the same CMake entry points fail closed.
    target = source / files[-1]
    target.write_text('unrecognized local source\n')
    script.write_text(boilerplate + apply)
    result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
    assert result.returncode and 'patch does not apply' in result.stderr
    assert target.read_text() == 'unrecognized local source\n'
    print('PASS required mismatch fails without overwriting unknown changes')
    script.write_text(boilerplate + f'''
brookesia_hal_adaptor_check_retired_patch("{alias}" "{files[0]}" "unrecognized local source")
''')
    result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
    assert result.returncode and 'retired workaround' in result.stderr
    assert target.read_text() == 'unrecognized local source\n'
    print('PASS retired workaround blocks configuration without source changes')


def run(managed_components: Path):
    """Run peripheral, camera and CMake checks without changing the dependencies."""
    component = managed_components / 'espressif__esp_board_manager'
    with tempfile.TemporaryDirectory(prefix='brookesia-bmgr-test-') as directory:
        temporary = Path(directory)
        _run_driver(component, temporary, 'periph', ('src/esp_board_periph.c',),
                    'esp_board_manager_periph_deinit_retry', PERIPH_HEADERS,
                    ('retained', 'consumed', 'missing_callback', 'overflow', 'batch'))
        _run_driver(component, temporary, 'dvp',
                    [f'devices/dev_camera/{name}' for name in
                     ('dev_camera.c', 'dev_camera_sub_dvp.c', 'dev_camera.h')],
                    'esp_board_manager_dvp_camera_deinit', DVP_HEADERS,
                    ('video_retry', 'retained', 'consumed', 'init_failure', 'other_subtype'))
        _check_patch_configure(managed_components, temporary)
