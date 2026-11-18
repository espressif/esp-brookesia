# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run production SR requirements registration and official ESP-DL generation.

Only the IDF CMake property access boundary is faked. No target compilation or
model inference is performed; unsupported metadata fails rather than skipping.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import yaml


IDF_PROPERTIES = '''
cmake_minimum_required(VERSION 3.22)
function(idf_build_get_property output property)
    get_property(value GLOBAL PROPERTY "idf_${property}")
    set(${output} "${value}" PARENT_SCOPE)
endfunction()
function(idf_build_set_property property value)
    set_property(GLOBAL PROPERTY "idf_${property}" "${value}" ${ARGN})
endfunction()
function(idf_component_get_property output component property)
    if(property STREQUAL "COMPONENT_VERSION")
        set(${output} "${SR_VERSION}" PARENT_SCOPE)
    elseif(property STREQUAL "COMPONENT_DIR")
        set(${output} "${DL_DIR}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unexpected IDF property ${property}")
    endif()
endfunction()
set_property(GLOBAL PROPERTY idf_BUILD_COMPONENTS "${SR_NAME};${DL_NAME}")
set(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR ON)
include("${DL_DIR}/cmake/compile_register.cmake")
if(NATIVE_REGISTRATION)
    esp_dl_compile_add_yml("${SR_REQ}")
endif()
include("${PRODUCTION_BLOCK}")
if(APP_REQ)
    esp_dl_compile_add_yml("${APP_REQ}")
endif()
idf_build_get_property(requirements ESPDL_COMPILE_REQS)
file(WRITE "${RESULT}" "${requirements}")
'''


def run(managed_components):
    here = Path(__file__).resolve().parent
    root = here.parents[4]
    adaptor = root / 'hal/brookesia_hal_adaptor'
    dl = managed_components / 'espressif__esp-dl'
    cmake = shutil.which('cmake')
    if cmake is None:
        raise FileNotFoundError('cmake is required for DL configuration regressions')
    source = (adaptor / 'CMakeLists.txt').read_text()
    start = source.index('if(BROOKESIA_HAL_ADAPTOR_NEEDS_AV_PROCESSOR)',
                         source.index('idf_component_register('))
    end = source.index('#\n# Patch resolved dependencies', start)
    sr_req = adaptor / 'tools/esp_sr_2_5_3_compile_req.yml'
    request = yaml.safe_load(sr_req.read_text())
    ops = yaml.safe_load((dl / 'spec/ops.yml').read_text())['ops']
    kernels = yaml.safe_load((dl / 'spec/kernels.yml').read_text())
    # The SDK ignores unknown kernels; this regression must reject stale metadata.
    assert set(request['ops']) <= set(ops), 'Unknown SR operator requirement'
    assert set(request['kernels']) <= set(kernels['symbols']), 'Unknown SR kernel requirement'
    assert request['kernel_abi'] == kernels['kernel_abi'], 'Unexpected SR/DL kernel ABI'
    with tempfile.TemporaryDirectory(prefix='brookesia-dl-config-') as directory:
        build = Path(directory)
        block = build / 'production.cmake'
        block.write_text(source[start:end])
        fixture = build / 'fixture.cmake'
        fixture.write_text(IDF_PROPERTIES)
        app = build / 'application.compile_req.yml'
        app.write_text(f"kernel_abi: {request['kernel_abi']}\nops: [Relu]\nkernels: []\n")
        for alias in ('local', 'managed'):
            sr_name, dl_name = ('esp-sr', 'esp-dl') if alias == 'local' else (
                'espressif__esp-sr', 'espressif__esp-dl')
            for target in ('esp32p4', 'esp32s3', 'esp32s31', 'esp32c5'):
                output = build / f'{alias}-{target}'
                output.mkdir()
                command = [cmake, f'-DSR_NAME={sr_name}', f'-DDL_NAME={dl_name}', '-DSR_VERSION=2.5.3',
                           f'-DDL_DIR={dl}', f'-DCOMPONENT_DIR={adaptor}', f'-DPRODUCTION_BLOCK={block}',
                           f'-DSR_REQ={sr_req}', f'-DRESULT={output / "requirements.txt"}']
                subprocess.run(command + ['-P', str(fixture)], check=True, capture_output=True, text=True)
                requirements = (output / 'requirements.txt').read_text().split(';')
                assert requirements == [str(sr_req)], f'{alias}/{target}: missing/duplicate registration'
                generate(dl, target, output, requirements)
                header = (output / 'dl_compile_config.h').read_text()
                assert '#define DL_COMPILE_ALL 0' in header
                for op in request['ops']:
                    assert f'#define DL_OP_{op} 1' in header
                assert '#define DL_OP_Relu 1' not in header
                subprocess.run(command + [f'-DAPP_REQ={app}', '-P', str(fixture)],
                               check=True, capture_output=True, text=True)
                union = (output / 'requirements.txt').read_text().split(';')
                assert union == [str(sr_req), str(app)], 'Application requirements must join the SR set'
                generate(dl, target, output, union)
                assert '#define DL_OP_Relu 1' in (output / 'dl_compile_config.h').read_text()
            # New SR owns registration: the adaptor must not duplicate its native request.
            subprocess.run([arg if not arg.startswith('-DSR_VERSION=') else '-DSR_VERSION=2.5.5'
                            for arg in command] + ['-DNATIVE_REGISTRATION=ON', '-P', str(fixture)],
                           check=True, capture_output=True, text=True)
            assert (output / 'requirements.txt').read_text() == str(sr_req)
            subprocess.run([arg if not arg.startswith('-DSR_VERSION=') else '-DSR_VERSION=2.5.5'
                            for arg in command] + ['-P', str(fixture)],
                           check=True, capture_output=True, text=True)
            assert (output / 'requirements.txt').read_text() == '', 'Backport must apply only to SR 2.5.3'
    print('PASS: DL registration aliases, P4/S3/S31/C5 generation, model requirement union and SR version gate',
          flush=True)


def generate(dl, target, output, requirements):
    command = [sys.executable, str(dl / 'cmake/compile_finalize.py'), '--ops', str(dl / 'spec/ops.yml'),
               '--kernels', str(dl / 'spec/kernels.yml'), '--target', target, '--component-dir', str(dl)]
    for option, filename in [('header', 'dl_compile_config.h'), ('srcs', 'dl_compile_srcs.cmake'),
                             ('kernel-inc', 'dl_kernel.inc'), ('conv-select', 'dl_conv_select.inc'),
                             ('module-includes-inc', 'dl_module_includes.inc'),
                             ('module-register-inc', 'dl_module_register.inc')]:
        command += [f'--{option}', str(output / filename)]
    subprocess.run(command + ['--req', *requirements], check=True)
