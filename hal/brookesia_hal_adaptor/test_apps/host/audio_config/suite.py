# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Execute production HAL configuration functions with real SDK types and fake I/O.

Only function bodies are extracted; SDK structures come from the supplied
av_processor dependency. This verifies HAL wiring and rollback on the host,
not SDK scheduling, audio conversion, hardware or acoustic behavior.
The shared-I2S fixture also executes the resolved codec SDK pairing,
format compatibility, set_fmt and open functions with driver/OS stubs.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    end = source.index('\n}', start) + 2
    return source[start:end] + '\n'


def run(managed_components):
    here = Path(__file__).resolve().parent
    root = here.parents[4]
    audio = root / 'hal/brookesia_hal_adaptor/src/audio'
    sdk = managed_components / 'jason-mao__av_processor/include'
    if not (sdk / 'audio_processor.h').is_file():
        raise FileNotFoundError(f'av_processor headers missing: {sdk}')
    converter = (audio / 'processor_type_converter.cpp').read_text()
    signatures = (
        'audio_task_config_t AudioProcessorTypeConverter::convert(',
        'audio_mixer_gain_config_t AudioProcessorTypeConverter::convert(',
        'audio_playback_config_t AudioProcessorTypeConverter::convert(',
        'audio_feeder_config_t AudioProcessorTypeConverter::convert(',
    )
    bodies = ''.join(function(converter, signature) for signature in signatures)
    core = function((audio / 'processor_impl.cpp').read_text(), 'bool AudioProcessorCore::open_common(')
    bodies += core
    codec = managed_components / 'espressif__esp_codec_dev'
    data_source = (codec / 'platform/audio_codec_data_i2s.c').read_text()
    device_source = (codec / 'esp_codec_dev.c').read_text()
    with tempfile.TemporaryDirectory(prefix='brookesia-audio-config-') as directory:
        build = Path(directory)
        (build / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n'
                                       '#define ESP_OK 0\n#define ESP_FAIL -1\n'
                                       '#define ESP_ERR_INVALID_ARG 0x102\n#define ESP_ERR_NO_MEM 0x101\n'
                                       '#define ESP_ERR_NOT_SUPPORTED 0x106\n#define ESP_ERR_NOT_FOUND 0x105\n'
                                       '#define ESP_ERR_INVALID_STATE 0x103\n')
        (build / 'esp_gmf_afe_manager.h').write_text(
            '#pragma once\ntypedef int afe_type_t;\ntypedef int afe_mode_t;\n'
            'typedef void *esp_gmf_afe_manager_handle_t;\n'
            'typedef void *esp_gmf_element_handle_t;\n#define BIT(n) (1U << (n))\n')
        (build / 'esp_audio_simple_player_advance.h').write_text('#pragma once\n')
        (build / 'production.inc').write_text(bodies)
        data_types = data_source[data_source.index('typedef struct {'):data_source.index(
            'static esp_codec_dev_mutex_handle_t try_alloc_mutex')]
        device_types = device_source[device_source.index('typedef struct {'):device_source.index(
            'static bool _verify_codec_ready')]
        (build / 'codec_types.inc').write_text(data_types + device_types)
        sdk_functions = ''.join(function(data_source, signature) for signature in (
            'static i2s_data_t *get_paired(', 'static bool _i2s_valid_fmt(',
            'static int check_fs_compatible(', 'static int _i2s_data_set_fmt('))
        sdk_functions += ''.join(function(device_source, signature) for signature in (
            'static bool _verify_codec_ready(', 'static bool _verify_drv_ready(', 'int esp_codec_dev_open('))
        (build / 'codec_functions.inc').write_text(sdk_functions)
        codec_includes = [f'-I{codec / "include"}', f'-I{codec / "interface"}']
        codec_object = build / 'codec_shared.o'
        subprocess.run([shutil.which('cc') or 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        f'-I{build}', *codec_includes, '-c', str(here / 'codec_shared.c'),
                        '-o', str(codec_object)], check=True)
        binary = build / 'test'
        command = [shutil.which('c++') or 'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                   '-Wno-missing-field-initializers', f'-I{build}', f'-I{sdk}',
                   *codec_includes, str(here / 'test_main.cpp'), str(codec_object), '-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
        # Recreate the rejected implementation, keeping every other production
        # function unchanged. Its own apparent success must not satisfy the same
        # clock/metadata regression check that the repaired version passes.
        start = core.index('    const auto &recorder_info =')
        end = core.index('    BROOKESIA_CHECK_FALSE_RETURN(player_iface_->open', start)
        unfixed = core[:start] + ('    const auto &recorder_info = recorder_iface_->get_info();\n'
                                 '    const auto &player_config = config_.playback.player;\n\n') + core[end:]
        (build / 'production.inc').write_text(bodies.replace(core, unfixed))
        subprocess.run(command, check=True)
        subprocess.run([str(binary), '--reproduce-unfixed'], check=True, timeout=10)
        rejected = subprocess.run([str(binary), '--clock-regression'], check=False, timeout=10)
        if rejected.returncode != 1:
            raise AssertionError(f'Unfixed implementation escaped regression: {rejected.returncode}')
        print('PASS: rejected pre-fix implementation fails the shared-I2S regression', flush=True)
