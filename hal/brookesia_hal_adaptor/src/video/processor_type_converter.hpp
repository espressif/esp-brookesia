/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "video_processor.h"

namespace esp_brookesia::hal {

class VideoProcessorTypeConverter {
public:
    static esp_capture_format_id_t convert(const video::EncoderSinkFormat &format);
    static esp_capture_sink_cfg_t convert(const video::EncoderSinkInfo &sink_info);
    static video_capture_config_t convert(
        const video::EncoderConfig &config, void *camera_config, video_capture_frame_callback_t capture_frame_cb,
        void *capture_frame_cb_ctx
    );
    static video_capture_handle_t to_capture_handle(void *encoder_handle)
    {
        return reinterpret_cast<video_capture_handle_t>(encoder_handle);
    }

    static esp_video_codec_type_t convert(const video::DecoderSourceFormat &format);
    static esp_video_codec_pixel_fmt_t convert(const video::DecoderSinkFormat &format);
    static uint32_t convert(const video::DecoderSourceFormat &format, bool enable_hw_acceleration);
    static video_render_config_t convert(
        const video::DecoderConfig &config, video_render_decode_callback_t decode_cb, void *decode_cb_ctx
    );
    static video_render_handle_t to_render_handle(void *decoder_handle)
    {
        return reinterpret_cast<video_render_handle_t>(decoder_handle);
    }
};

} // namespace esp_brookesia::hal
