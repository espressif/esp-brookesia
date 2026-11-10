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
from ctypes.util import find_library
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
        (build / 'esp_err.h').write_text(
            '#pragma once\ntypedef int esp_err_t;\n'
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
        unfixed = core[:start] + (
            '    const auto &recorder_info = recorder_iface_->get_info();\n'
            '    const auto &player_config = config_.playback.player;\n\n') + core[end:]
        (build / 'production.inc').write_text(bodies.replace(core, unfixed))
        subprocess.run(command, check=True)
        subprocess.run([str(binary), '--reproduce-unfixed'], check=True, timeout=10)
        rejected = subprocess.run([str(binary), '--clock-regression'], check=False, timeout=10)
        if rejected.returncode != 1:
            raise AssertionError(f'Unfixed implementation escaped regression: {rejected.returncode}')
        print('PASS: rejected pre-fix implementation fails the shared-I2S regression', flush=True)
        run_lifecycle(build, audio, here, sdk, codec_includes, codec_object)


def run_lifecycle(build, audio, here, sdk, codec_includes, codec_object):
    """Exercise real lifecycle bodies with a worker that must finish STOPPED first."""
    fixture = (here / 'test_main.cpp').read_text().split('#include "production.inc"')[0]
    fixture = fixture.replace('#define BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL 1',
                              '#define BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL 0')
    fixture = fixture.replace('namespace audio {',
                              'namespace audio {\nenum class PlayState { Idle, Playing, Paused };')
    fixture = fixture.replace('std::function<void()>;', 'std::function<void(PlayState)>;')
    fixture = fixture.replace('using EventCallback = std::function<void(PlayState)>;\n};', '''
    using EventCallback = std::function<void(PlayState)>;
    virtual ~PlaybackIface() = default;
    virtual bool open(EventCallback) = 0;
    virtual void close() = 0;
    virtual bool play(const std::string &) = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool stop() = 0;
    virtual bool is_opened() const = 0;
};''')
    start = fixture.index('class AudioProcessorCore {')
    end = fixture.index('static audio_manager_config_t captured_manager;', start)
    header = (audio / 'processor_impl.hpp').read_text()
    declaration = header[header.index('class AudioProcessorCore {'):
                         header.index('#if BROOKESIA_HAL_ADAPTOR_AUDIO_ENABLE_PROCESSOR_RECORDING_IMPL\nclass AudioEncoderImpl:')]
    declaration = declaration.replace('private:', 'public:')
    fixture = fixture[:start] + (
        'namespace audio { using CodecRecorderIface = Recorder; }\n'
        'struct AudioProcessorConfig { AudioProcessorPlaybackConfig playback; };\n') + \
        declaration + fixture[end:]
    fixture = fixture.replace('class AudioProcessorTypeConverter {\npublic:',
                              'class AudioProcessorTypeConverter {\npublic:\n'
                              'static auto to_playback_handle(void *p) { return '
                              'static_cast<audio_play_handle_t>(p); }')
    fixture = ('#include <atomic>\n#include <chrono>\n#include <future>\n#include <mutex>\n'
               '#include <thread>\n#include "boost/thread.hpp"\n#include "boost/thread/condition_variable.hpp"\n'
               '#define BROOKESIA_THREAD_CONFIG_GUARD(...) ((void)0)\n'
               'constexpr size_t PLAYBACK_CLOSE_THREAD_STACK_SIZE=8*1024;\n'
               '#define BROOKESIA_LOGD(...) ((void)0)\n' + fixture)
    (build / 'lifecycle_fixture.inc').write_text(fixture)
    converter = (audio / 'processor_type_converter.cpp').read_text()
    converter_bodies = ''.join(function(converter, signature) for signature in (
        'audio_task_config_t AudioProcessorTypeConverter::convert(',
        'audio_mixer_gain_config_t AudioProcessorTypeConverter::convert(',
        'audio_playback_config_t AudioProcessorTypeConverter::convert('))
    signatures = ('AudioProcessorCore::AudioProcessorCore(', 'AudioProcessorCore::~AudioProcessorCore()',
                  'bool AudioProcessorCore::acquire(', 'void AudioProcessorCore::release()',
                  'void AudioProcessorCore::clear_playback_callback()',
                  'void AudioProcessorCore::disable_playback_callback()',
                  'void AudioProcessorCore::wait_playback_callbacks()',
                  'bool AudioProcessorCore::inside_playback_callback()',
                  'void AudioProcessorCore::retain_after_deferred_close_failure(',
                  'bool AudioProcessorCore::open_common(', 'void AudioProcessorCore::close_common()',
                  'void AudioProcessorCore::on_playback_event(', 'AudioPlaybackImpl::AudioPlaybackImpl(',
                  'AudioPlaybackImpl::~AudioPlaybackImpl()', 'bool AudioPlaybackImpl::open(',
                  'void AudioPlaybackImpl::close()')
    source = (audio / 'processor_impl.cpp').read_text()
    current = converter_bodies + ''.join(function(source, signature) for signature in signatures) + '''
bool AudioPlaybackImpl::play(const std::string &) { return false; }
bool AudioPlaybackImpl::pause() { return false; }
bool AudioPlaybackImpl::resume() { return false; }
bool AudioPlaybackImpl::stop() { return false; }
'''
    command = [shutil.which('c++') or 'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pthread',
               '-Wno-missing-field-initializers', f'-I{build}', f'-I{here}', f'-I{sdk}', *codec_includes,
               str(here / 'lifecycle_main.cpp'), str(codec_object),
               f'-l:{find_library("boost_thread") or "libboost_thread.so"}', '-o', str(build / 'lifecycle')]
    (build / 'production.inc').write_text(current)
    subprocess.run(command, check=True)
    subprocess.run([str(build / 'lifecycle')], check=True, timeout=10)
    # Restore only the pre-fix callback lock. The first close must hang waiting
    # for a worker blocked on that lifecycle mutex, exactly as in the CI logs.
    callback = function(source, 'void AudioProcessorCore::on_playback_event(')
    old_callback = callback.replace('std::lock_guard lock(playback_callback_mutex_);',
                                    'std::lock_guard lock(mutex_);')
    (build / 'production.inc').write_text(current.replace(callback, old_callback))
    subprocess.run(command, check=True)
    try:
        subprocess.run([str(build / 'lifecycle')], check=True, timeout=2)
    except subprocess.TimeoutExpired:
        print('PASS: restoring the old callback/lifecycle lock reproduces the stop deadlock', flush=True)
    else:
        raise AssertionError('Old callback lock escaped the audio stop deadlock regression')
