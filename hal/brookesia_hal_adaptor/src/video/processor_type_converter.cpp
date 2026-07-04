/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_video_dec_mjpeg.h"
#include "private/utils.hpp"
#include "processor_type_converter.hpp"

namespace esp_brookesia::hal {

esp_capture_format_id_t VideoProcessorTypeConverter::convert(const video::EncoderSinkFormat &format)
{
    switch (format) {
    case video::EncoderSinkFormat::H264:        return ESP_CAPTURE_FMT_ID_H264;
    case video::EncoderSinkFormat::MJPEG:       return ESP_CAPTURE_FMT_ID_MJPEG;
    case video::EncoderSinkFormat::RGB565:      return ESP_CAPTURE_FMT_ID_RGB565;
    case video::EncoderSinkFormat::RGB888:      return ESP_CAPTURE_FMT_ID_RGB888;
    case video::EncoderSinkFormat::BGR888:      return ESP_CAPTURE_FMT_ID_BGR888;
    case video::EncoderSinkFormat::YUV420:      return ESP_CAPTURE_FMT_ID_YUV420;
    case video::EncoderSinkFormat::YUV422:      return ESP_CAPTURE_FMT_ID_YUV422;
    case video::EncoderSinkFormat::O_UYY_E_VYY: return ESP_CAPTURE_FMT_ID_O_UYY_E_VYY;
    default:                                  return ESP_CAPTURE_FMT_ID_NONE;
    }
}

esp_capture_sink_cfg_t VideoProcessorTypeConverter::convert(const video::EncoderSinkInfo &sink_info)
{
    return {
        .audio_info = {},
        .video_info = {
            .format_id = convert(sink_info.format),
            .width = sink_info.width,
            .height = sink_info.height,
            .fps = sink_info.fps,
        },
    };
}

video_capture_config_t VideoProcessorTypeConverter::convert(
    const video::EncoderConfig &config, void *camera_config, video_capture_frame_callback_t capture_frame_cb,
    void *capture_frame_cb_ctx
)
{
    video_capture_config_t capture_config = {};
    capture_config.camera_config = camera_config;
    capture_config.sink_num = config.sinks.size();
    BROOKESIA_CHECK_OUT_RANGE_EXECUTE(capture_config.sink_num, 0, DEFAULT_VID_MAX_PATH, {
        capture_config.sink_num = DEFAULT_VID_MAX_PATH;
    }, {
        BROOKESIA_LOGW("The sink number is out of range, will be truncated to %1%", DEFAULT_VID_MAX_PATH);
    });
    for (size_t i = 0; i < capture_config.sink_num; i++) {
        capture_config.sink_cfg[i] = convert(config.sinks[i]);
    }
    if (config.source.has_value()) {
        auto &source = config.source.value();
        capture_config.source_fixed_format_id = source.fixed_format.has_value() ?
                                                convert(source.fixed_format.value()) : ESP_CAPTURE_FMT_ID_NONE;
        capture_config.source_fixed_width = source.fixed_width.value_or(0);
        capture_config.source_fixed_height = source.fixed_height.value_or(0);
    }
    capture_config.capture_frame_cb = capture_frame_cb;
    capture_config.capture_frame_cb_ctx = capture_frame_cb_ctx;
    capture_config.stream_mode = config.enable_stream_mode;

    return capture_config;
}

esp_video_codec_type_t VideoProcessorTypeConverter::convert(const video::DecoderSourceFormat &format)
{
    switch (format) {
    case video::DecoderSourceFormat::H264:  return ESP_VIDEO_CODEC_TYPE_H264;
    case video::DecoderSourceFormat::MJPEG: return ESP_VIDEO_CODEC_TYPE_MJPEG;
    default:                              return ESP_VIDEO_CODEC_TYPE_NONE;
    }
}

esp_video_codec_pixel_fmt_t VideoProcessorTypeConverter::convert(const video::DecoderSinkFormat &format)
{
    switch (format) {
    case video::DecoderSinkFormat::RGB565_LE:   return ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_LE;
    case video::DecoderSinkFormat::RGB565_BE:   return ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_BE;
    case video::DecoderSinkFormat::RGB888:      return ESP_VIDEO_CODEC_PIXEL_FMT_RGB888;
    case video::DecoderSinkFormat::BGR888:      return ESP_VIDEO_CODEC_PIXEL_FMT_BGR888;
    case video::DecoderSinkFormat::YUV420P:     return ESP_VIDEO_CODEC_PIXEL_FMT_YUV420P;
    case video::DecoderSinkFormat::YUV422P:     return ESP_VIDEO_CODEC_PIXEL_FMT_YUV422P;
    case video::DecoderSinkFormat::YUV422:      return ESP_VIDEO_CODEC_PIXEL_FMT_YUV422;
    case video::DecoderSinkFormat::UYVY422:     return ESP_VIDEO_CODEC_PIXEL_FMT_UYVY422;
    case video::DecoderSinkFormat::O_UYY_E_VYY: return ESP_VIDEO_CODEC_PIXEL_FMT_O_UYY_E_VYY;
    default:                                  return ESP_VIDEO_CODEC_PIXEL_FMT_NONE;
    }
}

uint32_t VideoProcessorTypeConverter::convert(
    const video::DecoderSourceFormat &format, bool enable_hw_acceleration
)
{
    switch (format) {
    case video::DecoderSourceFormat::MJPEG:
        return enable_hw_acceleration ? ESP_VIDEO_DEC_HW_MJPEG_TAG : ESP_VIDEO_DEC_SW_MJPEG_TAG;
    default:
        return 0;
    }
}

video_render_config_t VideoProcessorTypeConverter::convert(
    const video::DecoderConfig &config, video_render_decode_callback_t decode_cb, void *decode_cb_ctx
)
{
    return {
        .decode_cfg = {
            .codec_type = convert(config.source_format),
            .codec_cc = convert(config.source_format, config.enable_hw_acceleration),
            .out_fmt = convert(config.sink_format),
            .codec_spec_info = nullptr,
            .codec_spec_info_size = 0,
        },
        .resolution = {
            .width = config.width,
            .height = config.height,
        },
        .decode_cb = decode_cb,
        .decode_cb_ctx = decode_cb_ctx,
        .frame_mode = !config.enable_stream_mode,
    };
}

} // namespace esp_brookesia::hal
