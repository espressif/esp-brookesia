/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_video_dec_mjpeg.h"
#include "private/utils.hpp"
#include "private/type_converter.hpp"

namespace esp_brookesia::service {

esp_capture_format_id_t TypeConverter::convert(const Helper::EncoderSinkFormat &format)
{
    switch (format) {
    case Helper::EncoderSinkFormat::H264:        return ESP_CAPTURE_FMT_ID_H264;
    case Helper::EncoderSinkFormat::MJPEG:       return ESP_CAPTURE_FMT_ID_MJPEG;
    case Helper::EncoderSinkFormat::RGB565:      return ESP_CAPTURE_FMT_ID_RGB565;
    case Helper::EncoderSinkFormat::RGB888:      return ESP_CAPTURE_FMT_ID_RGB888;
    case Helper::EncoderSinkFormat::BGR888:      return ESP_CAPTURE_FMT_ID_BGR888;
    case Helper::EncoderSinkFormat::YUV420:      return ESP_CAPTURE_FMT_ID_YUV420;
    case Helper::EncoderSinkFormat::YUV422:      return ESP_CAPTURE_FMT_ID_YUV422;
    case Helper::EncoderSinkFormat::O_UYY_E_VYY: return ESP_CAPTURE_FMT_ID_O_UYY_E_VYY;
    default:                                      return ESP_CAPTURE_FMT_ID_NONE;
    }
}

esp_capture_sink_cfg_t TypeConverter::convert(const Helper::EncoderSinkInfo &sink_info)
{
    return {
        .audio_info = {},
        .video_info = {
            .format_id = convert(sink_info.format),
            .width     = sink_info.width,
            .height    = sink_info.height,
            .fps       = sink_info.fps,
        },
    };
}

video_capture_config_t TypeConverter::convert(
    const Helper::EncoderConfig &config, void *camera_config,
    video_capture_frame_callback_t capture_frame_cb, void *capture_frame_cb_ctx
)
{
    video_capture_config_t capture_config = {};
    capture_config.camera_config        = camera_config;
    capture_config.sink_num             = config.sinks.size();
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(capture_config.sink_num, 0, DEFAULT_VID_MAX_PATH, {
        capture_config.sink_num = DEFAULT_VID_MAX_PATH;
    }, {
        BROOKESIA_LOGW("The sink number is out of range, will be truncated to %1%", DEFAULT_VID_MAX_PATH);
    });
    for (size_t i = 0; i < capture_config.sink_num; i++) {
        capture_config.sink_cfg[i] = convert(config.sinks[i]);
    }
    capture_config.capture_frame_cb     = capture_frame_cb;
    capture_config.capture_frame_cb_ctx = capture_frame_cb_ctx;
    capture_config.stream_mode          = config.enable_stream_mode;
    return capture_config;
}

esp_video_codec_type_t TypeConverter::convert(const Helper::DecoderSourceFormat &format)
{
    switch (format) {
    case Helper::DecoderSourceFormat::H264:  return ESP_VIDEO_CODEC_TYPE_H264;
    case Helper::DecoderSourceFormat::MJPEG: return ESP_VIDEO_CODEC_TYPE_MJPEG;
    default:                                  return ESP_VIDEO_CODEC_TYPE_NONE;
    }
}

esp_video_codec_pixel_fmt_t TypeConverter::convert(const Helper::DecoderSinkFormat &format)
{
    switch (format) {
    case Helper::DecoderSinkFormat::RGB565_LE:   return ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_LE;
    case Helper::DecoderSinkFormat::RGB565_BE:   return ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_BE;
    case Helper::DecoderSinkFormat::RGB888:      return ESP_VIDEO_CODEC_PIXEL_FMT_RGB888;
    case Helper::DecoderSinkFormat::BGR888:      return ESP_VIDEO_CODEC_PIXEL_FMT_BGR888;
    case Helper::DecoderSinkFormat::YUV420P:     return ESP_VIDEO_CODEC_PIXEL_FMT_YUV420P;
    case Helper::DecoderSinkFormat::YUV422P:     return ESP_VIDEO_CODEC_PIXEL_FMT_YUV422P;
    case Helper::DecoderSinkFormat::YUV422:      return ESP_VIDEO_CODEC_PIXEL_FMT_YUV422;
    case Helper::DecoderSinkFormat::UYVY422:     return ESP_VIDEO_CODEC_PIXEL_FMT_UYVY422;
    case Helper::DecoderSinkFormat::O_UYY_E_VYY: return ESP_VIDEO_CODEC_PIXEL_FMT_O_UYY_E_VYY;
    default:                                      return ESP_VIDEO_CODEC_PIXEL_FMT_NONE;
    }
}

uint32_t TypeConverter::convert(const Helper::DecoderSourceFormat &format, bool enable_hw_acceleration)
{
    switch (format) {
    case Helper::DecoderSourceFormat::MJPEG:
        return enable_hw_acceleration ? ESP_VIDEO_DEC_HW_MJPEG_TAG : ESP_VIDEO_DEC_SW_MJPEG_TAG;
    default:
        return 0;
    }
}

video_render_config_t TypeConverter::convert(
    const Helper::DecoderConfig &config, void *camera_config,
    video_render_decode_callback_t decode_cb, void *decode_cb_ctx
)
{
    (void)camera_config;
    return {
        .decode_cfg = {
            .codec_type = convert(config.source_format),
            .codec_cc   = convert(config.source_format, config.enable_hw_acceleration),
            .out_fmt    = convert(config.sink_format),
            .codec_spec_info = nullptr,
            .codec_spec_info_size = 0,
        },
        .resolution = {
            .width  = config.width,
            .height = config.height,
        },
        .decode_cb     = decode_cb,
        .decode_cb_ctx = decode_cb_ctx,
        .frame_mode    = !config.enable_stream_mode,
    };
}

} // namespace esp_brookesia::service
