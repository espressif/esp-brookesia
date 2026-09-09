/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

using esp_err_t = int;
using gpio_num_t = int;
constexpr gpio_num_t GPIO_NUM_34 = 34;
constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;
constexpr int ESP_ERR_INVALID_STATE = 0x103;
inline const char *esp_err_to_name(int result)
{
    return result == ESP_OK ? "ESP_OK" : "injected failure";
}
struct esp_board_info {};
using esp_board_info_t = esp_board_info;
struct dev_camera_config_t {
    const char *sub_type;
};
struct dev_camera_handle_t {
    const char *dev_path;
};
#define ESP_BOARD_DEVICE_NAME_CAMERA "camera"
#define ESP_VIDEO_DVP_DEVICE_NAME "/dev/video2"
#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME "/dev/video0"
#define ESP_VIDEO_SPI_DEVICE_NAME "/dev/video3"
struct esp_capture_video_v4l2_src_cfg_t {
    char dev_name[64];
    uint8_t buf_count;
};
constexpr int ESP_CAPTURE_STREAM_TYPE_VIDEO = 1;
struct esp_capture_stream_frame_t {
    int stream_type;
    uint64_t pts;
    uint8_t *data;
    size_t size;
};
using video_capture_handle_t = void *;
using video_render_handle_t = void *;
using esp_capture_format_id_t = int;
using esp_video_codec_type_t = int;
using esp_video_codec_pixel_fmt_t = int;
struct esp_capture_sink_cfg_t {};
using video_capture_frame_callback_t = void (*)(void *, int, esp_capture_stream_frame_t *);
using video_render_decode_callback_t = void (*)(void *);
struct video_render_config_t {};
struct video_capture_config_t {
    video_capture_frame_callback_t callback;
    void *context;
};
video_capture_handle_t video_capture_open(const video_capture_config_t *config);
void video_capture_close(video_capture_handle_t handle);
int video_capture_start(video_capture_handle_t handle);
int video_capture_stop(video_capture_handle_t handle);
int video_capture_fetch_frame_acquire(video_capture_handle_t handle, int index, esp_capture_stream_frame_t *frame);
int video_capture_fetch_frame_release(video_capture_handle_t handle, int index, esp_capture_stream_frame_t *frame);
