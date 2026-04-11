/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/service_helper/video.hpp"
#include "video_processor.h"

namespace esp_brookesia::service {

/**
 * @brief  Type converter class for converting helper types to audio_processor types
 */
class TypeConverter {
public:
    using Helper = helper::Video;

    static esp_capture_format_id_t convert(const Helper::EncoderSinkFormat &format);
    static esp_capture_sink_cfg_t convert(const Helper::EncoderSinkInfo &sink_info);
    static video_capture_config_t convert(
        const Helper::EncoderConfig &config, void *camera_config, video_capture_frame_callback_t capture_frame_cb,
        void *capture_frame_cb_ctx
    );
    static video_capture_handle_t to_capture_handle(void *encoder_handle)
    {
        return reinterpret_cast<video_capture_handle_t>(encoder_handle);
    }

    static esp_video_codec_type_t convert(const Helper::DecoderSourceFormat &format);
    static esp_video_codec_pixel_fmt_t convert(const Helper::DecoderSinkFormat &format);
    static uint32_t convert(const Helper::DecoderSourceFormat &format, bool enable_hw_acceleration);
    static video_render_config_t convert(
        const Helper::DecoderConfig &config, void *camera_config,
        video_render_decode_callback_t decode_cb, void *decode_cb_ctx
    );
    static video_render_handle_t to_render_handle(void *decoder_handle)
    {
        return reinterpret_cast<video_render_handle_t>(decoder_handle);
    }
};

} // namespace esp_brookesia::service
