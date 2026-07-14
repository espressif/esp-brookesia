/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

#include "gfx.h"
#include "expression_emote.h"
#include "brookesia/expression_emote/macro_configs.h"
#if !BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "../priv_include/emote_defs.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/expression_emote/emote.hpp"

namespace esp_brookesia::expression {

using EmoteHelper = service::helper::ExpressionEmote;
using DisplayService = service::Display;

static bool get_native_source_data(const Emote::AssetSource &source, emote_data_t &native_data);
static bool get_native_message_event(const std::string &event, std::string &native_event);
static std::vector<gfx_obj_t *> get_native_objs(void *handle, Emote::GFX_ObjectType type);
static size_t get_pixel_format_bytes(DisplayService::PixelFormat pixel_format);

inline static emote_handle_t get_native_handle(void *handle)
{
    return static_cast<emote_handle_t>(handle);
}

bool Emote::native_notify_flush_finished()
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return false;
    }

    emote_notify_flush_finished(get_native_handle(native_handle_));

    return true;
}

std::string Emote::get_component_version()
{
    return make_version(
               BROOKESIA_EXPRESSION_EMOTE_VER_MAJOR, BROOKESIA_EXPRESSION_EMOTE_VER_MINOR,
               BROOKESIA_EXPRESSION_EMOTE_VER_PATCH
           );
}

bool Emote::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_EXPRESSION_EMOTE_VER_MAJOR, BROOKESIA_EXPRESSION_EMOTE_VER_MINOR,
        BROOKESIA_EXPRESSION_EMOTE_VER_PATCH
    );

    return true;
}

bool Emote::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (DisplayService::Helper::is_available()) {
        BROOKESIA_CHECK_FALSE_RETURN(setup_display_source(), false, "Failed to setup Emote Display source");
    }
    BROOKESIA_CHECK_FALSE_RETURN(is_configured(), false, "Emote is not configured");

    BROOKESIA_LOGD("Emote start with config: %1%", BROOKESIA_DESCRIBE_TO_STR(config_));

    auto flush_cb = +[](int x_start, int y_start, int x_end, int y_end, const void *data, emote_handle_t handle) {
        // BROOKESIA_LOG_TRACE_GUARD();

        auto *self = static_cast<Emote *>(emote_get_user_data(handle));
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid user data");

        const bool auto_notify_flush_finished = self->display_source_id_ != 0;
        lib_utils::FunctionGuard notify_guard([handle, auto_notify_flush_finished]() {
            if (auto_notify_flush_finished) {
                emote_notify_flush_finished(handle);
            }
        });

        EmoteHelper::FlushReadyEventParam param = {
            .x_start = x_start,
            .y_start = y_start,
            .x_end = x_end,
            .y_end = y_end,
            .data = data,
        };
        self->publish_event(BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventId::FlushReady), service::EventItemMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventFlushReadyParam::Param),
                BROOKESIA_DESCRIBE_TO_JSON(param).as_object(),
            },
        });

        if (auto_notify_flush_finished) {
            auto ret = self->submit_display_frame(x_start, y_start, x_end, y_end, data);
            if (ret == ESP_ERR_NOT_ALLOWED) {
                BROOKESIA_LOGD("Emote frame dropped because source is not active");
            } else {
                BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to submit Emote Display frame");
            }
        }
    };
    emote_config_t native_config = {
        .flags = {
            .swap = config_.flag_swap_color_bytes,
            .double_buffer = config_.flag_double_buffer,
            .buff_dma = config_.flag_buff_dma,
            .buff_spiram = config_.flag_buff_spiram,
        },
        .gfx_emote = {
            .h_res = static_cast<int>(config_.h_res),
            .v_res = static_cast<int>(config_.v_res),
            .fps = static_cast<int>(config_.fps),
        },
        .buffers = {
            .buf_pixels = config_.buf_pixels,
        },
        .task = {
            .task_priority = config_.task_priority,
            .task_stack = config_.task_stack,
            .task_affinity = (config_.task_affinity >= CONFIG_SOC_CPU_CORES_NUM) ? -1 : config_.task_affinity,
            .task_stack_in_ext = config_.task_stack_in_ext,
        },
        .flush_cb = flush_cb,
        .update_cb = nullptr,
        .user_data = this,
    };
    native_handle_ = emote_init(&native_config);
    BROOKESIA_CHECK_NULL_RETURN(native_handle_, false, "Failed to initialize native emote");

    if ((display_source_id_ != 0) && !display_output_name_.empty()) {
        auto active_source_result = DisplayService::get_instance().get_active_source(display_output_name_);
        if (active_source_result.has_value() && (active_source_result.value() == DISPLAY_SOURCE_NAME)) {
            (void)refresh_native();
        }
    }

    return true;
}

void Emote::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    clear_display_source();

    if (native_handle_) {
        auto result = emote_deinit(get_native_handle(native_handle_));
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to deinitialize native emote");
        });
        native_handle_ = nullptr;
    }

    return;
}

std::expected<void, std::string> Emote::function_set_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = BROOKESIA_DESCRIBE_FROM_JSON(config, config_);
    if (!result) {
        return std::unexpected("Invalid config: " + BROOKESIA_DESCRIBE_TO_STR(config));
    }

    is_configured_ = true;

    return {};
}

std::expected<void, std::string> Emote::function_load_assets_source(const boost::json::object &source)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: source(%1%)", BROOKESIA_DESCRIBE_TO_STR(source));

    AssetSource source_data;
    emote_data_t native_data;
    auto result = BROOKESIA_DESCRIBE_FROM_JSON(source, source_data);
    if (!result || !get_native_source_data(source_data, native_data)) {
        return std::unexpected("Invalid source: " + BROOKESIA_DESCRIBE_TO_STR(source));
    }

    auto load_result = emote_mount_and_load_assets(get_native_handle(native_handle_), &native_data);
    if (load_result != ESP_OK) {
        return std::unexpected(
                   "Failed to load assets from source: " + BROOKESIA_DESCRIBE_TO_STR(source) + ", error: " +
                   std::string(esp_err_to_name(load_result))
               );
    }
    return {};
}

std::expected<void, std::string> Emote::function_set_emoji(const std::string &emoji)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: emoji(%1%)", emoji);

    auto set_result = emote_set_anim_emoji(get_native_handle(native_handle_), emoji.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set emoji: " + BROOKESIA_DESCRIBE_TO_STR(emoji) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    if (!set_obj_visible(GFX_ObjectType::Emoji, true)) {
        return std::unexpected("Failed to show emoji: " + BROOKESIA_DESCRIBE_TO_STR(emoji));
    }

    return {};
}

std::expected<void, std::string> Emote::function_hide_emoji()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto hide_result = set_obj_visible(GFX_ObjectType::Emoji, false);
    if (!hide_result) {
        return std::unexpected("Failed to hide emoji");
    }

    return {};
}

std::expected<void, std::string> Emote::function_refresh_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Refresh all the screen");

    auto refresh_result = emote_notify_all_refresh(get_native_handle(native_handle_));
    if (refresh_result != ESP_OK) {
        return std::unexpected(
                   "Failed to refresh all, error: " + std::string(esp_err_to_name(refresh_result))
               );
    }

    return {};
}

bool Emote::setup_display_source()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    display_service_binding_ = service::ServiceManager::get_instance().bind(DisplayService::Helper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(display_service_binding_.is_valid(), false, "Failed to bind Display service");

    auto outputs = DisplayService::get_instance().get_outputs();
    BROOKESIA_CHECK_FALSE_RETURN(!outputs.empty(), false, "No Display output is available");

    const auto &output = outputs.front();
    display_output_name_ = output.name;
    display_pixel_format_ = output.pixel_format;

    bool swap_color_bytes = false;
    auto panel_handles = hal::acquire_interfaces<hal::display::PanelIface>();
    for (auto &panel_handle : panel_handles) {
        if (panel_handle.instance_name() != output.panel_instance) {
            continue;
        }
        hal::display::PanelIface::DriverSpecific panel_specific;
        if (panel_handle->get_driver_specific(panel_specific)) {
            swap_color_bytes = panel_specific.bus_type == hal::display::PanelIface::BusType::Generic;
        }
        break;
    }

    if (!is_configured_) {
        config_ = Config{
            .h_res = output.width,
            .v_res = output.height,
            .buf_pixels = static_cast<size_t>(output.width * 16),
            .fps = 30,
            .task_priority = 5,
            .task_stack = 8 * 1024,
            .task_affinity = 1,
            .task_stack_in_ext = false,
            .flag_swap_color_bytes = swap_color_bytes,
            .flag_double_buffer = false,
            .flag_buff_dma = true,
        };
        is_configured_ = true;
    } else {
        if (config_.h_res == 0) {
            config_.h_res = output.width;
        }
        if (config_.v_res == 0) {
            config_.v_res = output.height;
        }
        if (config_.buf_pixels == 0) {
            config_.buf_pixels = static_cast<size_t>(config_.h_res * 16);
        }
        if (config_.fps == 0) {
            config_.fps = 30;
        }
        if (config_.task_priority == 0) {
            config_.task_priority = 5;
        }
        if (config_.task_stack == 0) {
            config_.task_stack = 8 * 1024;
        }
    }

    DisplayService::SourceInfo source = {
        .name = std::string(DISPLAY_SOURCE_NAME),
        .role = std::string(DISPLAY_SOURCE_ROLE),
        .preferred_outputs = {display_output_name_},
        .priority = 0,
    };
    auto register_result = DisplayService::get_instance().register_source(std::move(source));
    BROOKESIA_CHECK_FALSE_RETURN(
        register_result.has_value(), false, "Failed to register Emote Display source: %1%", register_result.error()
    );
    display_source_id_ = register_result.value();

    auto request_result = DisplayService::get_instance().request_output(display_source_id_, display_output_name_);
    BROOKESIA_CHECK_FALSE_RETURN(
        request_result.has_value(), false, "Failed to request Emote Display output: %1%", request_result.error()
    );

    display_source_state_connection_ = DisplayService::get_instance().connect_source_state_changed(
    [this](const std::string & source_name, const std::string & output_name, DisplayService::SourceState state) {
        if ((source_name != DISPLAY_SOURCE_NAME) || (output_name != display_output_name_)) {
            return;
        }
        if (state == DisplayService::SourceState::Granted) {
            (void)refresh_native();
        }
    }
                                       );
    BROOKESIA_CHECK_FALSE_RETURN(
        display_source_state_connection_.connected(), false,
        "Failed to subscribe Display SourceStateChanged for Emote source"
    );

    return true;
}

void Emote::clear_display_source()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    display_source_state_connection_.disconnect();

    if (display_source_id_ != 0) {
        auto &display_service = DisplayService::get_instance();
        if (!display_output_name_.empty()) {
            (void)display_service.release_output(display_source_id_, display_output_name_);
        }
        (void)display_service.unregister_source(display_source_id_);
    }

    display_output_name_.clear();
    display_source_id_ = 0;
    display_pixel_format_ = DisplayService::PixelFormat::Max;
}

esp_err_t Emote::submit_display_frame(int x_start, int y_start, int x_end, int y_end, const void *data)
{
    BROOKESIA_CHECK_NULL_RETURN(data, ESP_ERR_INVALID_ARG, "Emote frame data is null");
    BROOKESIA_CHECK_FALSE_RETURN(display_source_id_ != 0, ESP_ERR_INVALID_STATE, "Emote Display source is not registered");
    BROOKESIA_CHECK_FALSE_RETURN(!display_output_name_.empty(), ESP_ERR_INVALID_STATE, "Emote Display output is not selected");
    BROOKESIA_CHECK_FALSE_RETURN((x_end > x_start) && (y_end > y_start), ESP_ERR_INVALID_ARG, "Invalid Emote frame area");

    const size_t bpp = get_pixel_format_bytes(display_pixel_format_);
    BROOKESIA_CHECK_FALSE_RETURN(bpp > 0, ESP_ERR_NOT_SUPPORTED, "Unsupported Display pixel format");

    const uint32_t width = static_cast<uint32_t>(x_end - x_start);
    const uint32_t height = static_cast<uint32_t>(y_end - y_start);
    DisplayService::FrameInfo frame = {
        .x = static_cast<uint32_t>(x_start),
        .y = static_cast<uint32_t>(y_start),
        .width = width,
        .height = height,
        .pixel_format = display_pixel_format_,
    };

    auto present_result = DisplayService::get_instance().present_frame_sync(
                              display_source_id_, display_output_name_, frame,
                              service::RawBuffer(data, static_cast<size_t>(width) * height * bpp)
                          );

    switch (present_result) {
    case DisplayService::PresentResult::Presented:
        return ESP_OK;
    case DisplayService::PresentResult::DroppedNotActive:
        return ESP_ERR_NOT_ALLOWED;
    case DisplayService::PresentResult::DroppedInvalidFrame:
    case DisplayService::PresentResult::Error:
    default:
        BROOKESIA_LOGW("Emote Display frame was not presented: %1%", BROOKESIA_DESCRIBE_ENUM_TO_STR(present_result));
        return ESP_ERR_INVALID_STATE;
    }
}

bool Emote::refresh_native()
{
    if (native_handle_ == nullptr) {
        return false;
    }

    auto refresh_result = emote_notify_all_refresh(get_native_handle(native_handle_));
    if (refresh_result != ESP_OK) {
        BROOKESIA_LOGE("Failed to refresh Emote after active source switch: %1%", esp_err_to_name(refresh_result));
        return false;
    }
    return true;
}

std::expected<void, std::string> Emote::function_set_animation(const std::string &animation)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: animation(%1%)", animation);

    auto set_result = emote_set_dialog_anim(get_native_handle(native_handle_), animation.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set animation: " + BROOKESIA_DESCRIBE_TO_STR(animation) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    if (!set_obj_visible(GFX_ObjectType::Animation, true)) {
        return std::unexpected("Failed to show animation: " + BROOKESIA_DESCRIBE_TO_STR(animation));
    }

    return {};
}

std::expected<void, std::string> Emote::function_insert_animation(const std::string &animation, double duration_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: animation(%1%), duration_ms(%2%)", animation, duration_ms);

    auto insert_result = emote_insert_anim_dialog(
                             get_native_handle(native_handle_), animation.c_str(), static_cast<uint32_t>(duration_ms)
                         );
    if (insert_result != ESP_OK) {
        return std::unexpected(
                   "Failed to insert animation: " + BROOKESIA_DESCRIBE_TO_STR(animation) + ", error: " +
                   std::string(esp_err_to_name(insert_result))
               );
    }

    if (!set_obj_visible(GFX_ObjectType::Animation, true)) {
        return std::unexpected("Failed to show animation: " + BROOKESIA_DESCRIBE_TO_STR(animation));
    }

    return {};
}

std::expected<void, std::string> Emote::function_stop_animation()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto stop_result = emote_stop_anim_dialog(get_native_handle(native_handle_));
    if (stop_result != ESP_OK) {
        return std::unexpected(
                   "Failed to stop animation, error: " + std::string(esp_err_to_name(stop_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_wait_animation_frame_done(double timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: timeout_ms(%1%)", timeout_ms);

    auto wait_result = emote_wait_emerg_dlg_done(get_native_handle(native_handle_), static_cast<uint32_t>(timeout_ms));
    if (wait_result != ESP_OK) {
        return std::unexpected(
                   "Failed to wait animation frame done, error: " + std::string(esp_err_to_name(wait_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_set_event_msg(const std::string &event, const std::string &message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: event(%1%), message(%2%)", event, message);

    std::string native_event;
    if (!get_native_message_event(event, native_event)) {
        return std::unexpected("Invalid event: " + BROOKESIA_DESCRIBE_TO_STR(event));
    }

    auto set_result = emote_set_event_msg(get_native_handle(native_handle_), native_event.c_str(), message.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set event message: " + BROOKESIA_DESCRIBE_TO_STR(event) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_hide_event_message()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto hide_result = emote_set_event_msg(get_native_handle(native_handle_), EMOTE_MGR_EVT_OFF, "");
    if (hide_result != ESP_OK) {
        return std::unexpected(
                   "Failed to hide event message, error: " + std::string(esp_err_to_name(hide_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_set_qrcode(const std::string &qrcode)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    BROOKESIA_LOGD("Params: qrcode(%1%)", qrcode);

    auto set_result = emote_set_qrcode_data(get_native_handle(native_handle_), qrcode.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set qrcode: " + BROOKESIA_DESCRIBE_TO_STR(qrcode) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    if (!set_obj_visible(GFX_ObjectType::Qrcode, true)) {
        return std::unexpected("Failed to show qrcode: " + BROOKESIA_DESCRIBE_TO_STR(qrcode));
    }

    return {};
}

std::expected<void, std::string> Emote::function_hide_qrcode()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto hide_result = set_obj_visible(GFX_ObjectType::Qrcode, false);
    if (!hide_result) {
        return std::unexpected("Failed to hide qrcode");
    }

    if (!set_obj_visible(GFX_ObjectType::Emoji, true)) {
        BROOKESIA_LOGE("Failed to show emoji");
    }

    return {};
}

std::expected<void, std::string> Emote::function_notify_flush_finished()
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto notify_result = emote_notify_flush_finished(get_native_handle(native_handle_));
    if (notify_result != ESP_OK) {
        return std::unexpected(
                   "Failed to notify flush finished, error: " + std::string(esp_err_to_name(notify_result))
               );
    }

    return {};
}

bool Emote::set_obj_visible(GFX_ObjectType type, bool visible)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: type(%1%), visible(%2%)", BROOKESIA_DESCRIBE_TO_STR(type), BROOKESIA_DESCRIBE_TO_STR(visible)
    );

    auto gfx_handle = get_native_handle(native_handle_)->gfx_handle;
    BROOKESIA_CHECK_NULL_RETURN(gfx_handle, false, "Invalid gfx handle");

    gfx_emote_lock(gfx_handle);
    lib_utils::FunctionGuard unlock_guard([&]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        gfx_emote_unlock(gfx_handle);
    });

    auto set_objs_visible_func = [&](GFX_ObjectType type, bool visible) {
        auto objs = get_native_objs(native_handle_, type);
        for (auto obj : objs) {
            if (gfx_obj_set_visible(obj, visible) != ESP_OK) {
                BROOKESIA_LOGE(
                    "Failed to set object %1% visible: %2%", BROOKESIA_DESCRIBE_TO_STR(type),
                    BROOKESIA_DESCRIBE_TO_STR(visible)
                );
            };
        }
    };

    // Hide other objects when setting one object visible
    std::vector<GFX_ObjectType> hide_types;
    switch (type) {
    case Emote::GFX_ObjectType::Emoji:
        hide_types.push_back(Emote::GFX_ObjectType::Qrcode);
        break;
    case Emote::GFX_ObjectType::Animation:
        hide_types.push_back(Emote::GFX_ObjectType::Qrcode);
        break;
    case Emote::GFX_ObjectType::Qrcode:
        hide_types.push_back(Emote::GFX_ObjectType::Emoji);
        hide_types.push_back(Emote::GFX_ObjectType::Animation);
        break;
    default:
        break;
    }
    for (auto hide_type : hide_types) {
        set_objs_visible_func(hide_type, false);
    }

    // Set the target object visible
    set_objs_visible_func(type, visible);

    return true;
}

static bool get_native_source_data(const Emote::AssetSource &source, emote_data_t &native_data)
{
    switch (source.type) {
    case Emote::AssetSourceType::Path:
        native_data.type = EMOTE_SOURCE_PATH;
        native_data.source.path = source.source.c_str();
        native_data.flags.mmap_enable = source.flag_enable_mmap;
        break;
    case Emote::AssetSourceType::PartitionLabel:
        native_data.type = EMOTE_SOURCE_PARTITION;
        native_data.source.partition_label = source.source.c_str();
        native_data.flags.mmap_enable = source.flag_enable_mmap;
        break;
    default:
        return false;
    }
    return true;
}

static bool get_native_message_event(const std::string &event, std::string &native_event)
{
    Emote::AssetMessageType event_enum;
    auto event_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(event, event_enum);
    if (!event_result) {
        return false;
    }

    switch (event_enum) {
    case Emote::AssetMessageType::Idle:
        native_event = EMOTE_MGR_EVT_IDLE;
        break;
    case Emote::AssetMessageType::Speak:
        native_event = EMOTE_MGR_EVT_SPEAK;
        break;
    case Emote::AssetMessageType::Listen:
        native_event = EMOTE_MGR_EVT_LISTEN;
        break;
    case Emote::AssetMessageType::System:
        native_event = EMOTE_MGR_EVT_SYS;
        break;
    case Emote::AssetMessageType::User:
        native_event = EMOTE_MGR_EVT_SET;
        break;
    case Emote::AssetMessageType::Battery:
        native_event = EMOTE_MGR_EVT_BAT;
        break;
    default:
        return false;
    }
    return true;
}

static size_t get_pixel_format_bytes(DisplayService::PixelFormat pixel_format)
{
    switch (pixel_format) {
    case DisplayService::PixelFormat::RGB565:
        return 2;
    case DisplayService::PixelFormat::RGB888:
        return 3;
    default:
        return 0;
    }
}

static std::vector<gfx_obj_t *> get_native_objs(void *handle, Emote::GFX_ObjectType type)
{
    auto def_objects = get_native_handle(handle)->def_objects;
    switch (type) {
    case Emote::GFX_ObjectType::Emoji:
        return {def_objects[EMOTE_DEF_OBJ_ANIM_EYE].obj};
    case Emote::GFX_ObjectType::Animation:
        return {def_objects[EMOTE_DEF_OBJ_ANIM_EMERG_DLG].obj};
    case Emote::GFX_ObjectType::Qrcode:
        return {def_objects[EMOTE_DEF_OBJ_QRCODE].obj};
    default:
        return {};
    }
}

#if BROOKESIA_EXPRESSION_EMOTE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, Emote, Emote::get_instance().get_attributes().name, Emote::get_instance(),
    BROOKESIA_EXPRESSION_EMOTE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::expression
