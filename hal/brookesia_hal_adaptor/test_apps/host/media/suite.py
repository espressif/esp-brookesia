# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise resolved media dependency C sources with fake V4L2 and capture drivers.

Use the resolved managed_components directory (esp_capture 0.8.4 and
av_processor 0.6.6); the retained patches must already be applied by the project's
normal configure step. Sources are copied without edits.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile


COMMON = '''#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_TIMEOUT 0x107
#define CONFIG_IDF_TARGET_ESP32S31 1
#define CONFIG_VIDEO_PROCESSOR_ENABLE 1
#define ESP_LOGE(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGI ESP_LOGE
#define ESP_LOGW ESP_LOGE
#define ESP_LOGD ESP_LOGE
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(n) (n)
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
typedef void *EventGroupHandle_t;
typedef void *esp_gmf_oal_thread_t;
typedef int EventBits_t;
void *xSemaphoreCreateCounting(int maximum, int initial);
int xSemaphoreTake(void *handle, unsigned timeout);
int xSemaphoreGive(void *handle);
void vSemaphoreDelete(void *handle);
void vTaskDelay(unsigned ticks);
void vTaskDelete(void *handle);
void *xQueueCreate(unsigned count, unsigned size);
int xQueueReceive(void *queue, void *data, unsigned ticks);
int xQueueSend(void *queue, const void *data, unsigned ticks);
void vQueueDelete(void *queue);
void *xEventGroupCreate(void);
int xEventGroupWaitBits(void *group, int bits, int clear, int all, unsigned ticks);
int xEventGroupSetBits(void *group, int bits);
void vEventGroupDelete(void *group);
int64_t esp_timer_get_time(void);
void *esp_gmf_oal_calloc(size_t count, size_t size);
void *esp_gmf_oal_malloc(size_t size);
void esp_gmf_oal_free(void *pointer);
int esp_gmf_oal_thread_create(void **out, const char *name, void (*fn)(void *), void *arg,
                              size_t stack, int priority, bool external, int core);
void esp_gmf_oal_thread_delete(void *handle);
typedef void *esp_video_dec_handle_t;
typedef struct { int out_fmt; } esp_video_dec_cfg_t;
typedef struct { uint32_t width, height; } esp_video_codec_resolution_t;
typedef struct { uint8_t *data; size_t size; uint32_t pts; } esp_video_dec_in_frame_t;
typedef esp_video_dec_in_frame_t esp_video_dec_out_frame_t;
typedef struct { esp_video_codec_resolution_t res; } esp_video_codec_frame_info_t;
#define ESP_VC_ERR_OK 0
void esp_video_dec_close(void *handle);
int esp_video_dec_open(const esp_video_dec_cfg_t *config, void **out);
int esp_video_dec_process(void *handle, esp_video_dec_in_frame_t *in, esp_video_dec_out_frame_t *out);
int esp_video_dec_get_frame_info(void *handle, esp_video_codec_frame_info_t *info);
void esp_video_dec_register_default(void);
void esp_video_enc_register_default(void);
size_t esp_video_codec_get_image_size(int format, const esp_video_codec_resolution_t *res);
void *esp_video_codec_align_alloc(size_t align, size_t size, uint32_t *actual);
'''

CAPTURE = '''#pragma once
#include "host_stubs.h"
#include "esp_capture_types.h"
#include "esp_capture_video_src_if.h"
#include "esp_capture_video_v4l2_src.h"
typedef void *esp_capture_handle_t;
typedef void *esp_capture_sink_handle_t;
typedef void esp_capture_audio_src_if_t;
typedef struct { esp_capture_video_info_t video_info; } esp_capture_sink_cfg_t;
typedef struct { esp_capture_video_src_if_t *video_src; esp_capture_sync_mode_t sync_mode; } esp_capture_cfg_t;
#define ESP_CAPTURE_RUN_MODE_ALWAYS 1
#define ESP_CAPTURE_RUN_MODE_ONESHOT 2
esp_capture_err_t esp_capture_open(const esp_capture_cfg_t *cfg, void **out);
esp_capture_err_t esp_capture_sink_setup(void *capture, int index, const esp_capture_sink_cfg_t *cfg, void **out);
esp_capture_err_t esp_capture_start(void *capture);
esp_capture_err_t esp_capture_stop(void *capture);
esp_capture_err_t esp_capture_close(void *capture);
esp_capture_err_t esp_capture_sink_enable(void *sink, int mode);
esp_capture_err_t esp_capture_sink_acquire_frame(void *sink, esp_capture_stream_frame_t *frame, bool no_wait);
esp_capture_err_t esp_capture_sink_release_frame(void *sink, esp_capture_stream_frame_t *frame);
'''


def run(managed_components: Path):
    """Run all 17 media cases with self-cleaning sources, stubs and executables."""
    here = Path(__file__).resolve().parent
    capture = managed_components / 'espressif__esp_capture'
    processor = managed_components / 'jason-mao__av_processor'
    with tempfile.TemporaryDirectory(prefix='media-patches-') as temporary:
        temp = Path(temporary)
        (temp / 'host_stubs.h').write_text(COMMON)
        headers = ('sdkconfig.h', 'esp_err.h', 'esp_log.h', 'esp_video_init.h', 'esp_cache.h',
                   'freertos/FreeRTOS.h', 'freertos/semphr.h', 'freertos/task.h', 'freertos/queue.h',
                   'esp_timer.h', 'driver/gpio.h', 'esp_gmf_oal_thread.h', 'esp_gmf_oal_mem.h',
                   'esp_video_dec.h', 'esp_video_dec_reg.h', 'esp_video_enc_default.h',
                   'esp_video_codec_utils.h', 'esp_video_dec_default.h', 'esp_video_codec_version.h',
                   'esp_video_codec_types.h')
        for name in headers:
            target = temp / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#include "host_stubs.h"\n')
        for name in ('esp_capture.h', 'esp_capture_sink.h', 'esp_capture_defaults.h'):
            (temp / name).write_text(CAPTURE)
        (temp / 'capture_os.h').write_text('#define capture_calloc calloc\n')
        (temp / 'esp_video_ioctl.h').write_text('#define VIDIOC_S_DQBUF_TIMEOUT 0x10000000UL\n')
        shutil.copy(capture / 'impl/capture_video_src/capture_video_v4l2_src.c', temp)
        shutil.copy(processor / 'src/video_processor.c', temp)
        shutil.copy(processor / 'include/video_processor.h', temp)
        includes = [temp, capture / 'include', capture / 'interface', capture / 'include/impl']
        for name, cases in (
            ('v4l2', ('uyvy', 'yuyv', 'both_uyvy_first', 'both_yuyv_first', 'none', 'uyvy_420', 'yuyv_420')),
            ('av_processor', ('frame_stop', 'frame_close', 'stream_stop', 'caps_failure', 'open_failure',
                              'sink_failure', 'start_failure', 'thread_failure', 'stop_error', 'close_error')),
        ):
            executable = temp / name
            command = ['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                       '-Wno-unused-parameter', '-fsanitize=undefined', '-ffunction-sections',
                       '-fdata-sections', '-Wl,--gc-sections', '-g']
            if name == 'av_processor':
                # Upstream debug logging prints an int frame size with %zu;
                # LP64 host format warnings are unrelated to cleanup behavior.
                command += ['-Wno-format']
            for include in includes:
                command += ['-I', str(include)]
            subprocess.run(command + [str(here / f'test_{name}.c'), '-o', str(executable)], check=True)
            for case in cases:
                subprocess.run([str(executable), case], check=True, timeout=5)
                print(f'PASS {name} {case}')
