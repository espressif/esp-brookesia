/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_lvgl/display_source.hpp"
#include "brookesia/gui_lvgl/backend.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "esp_lv_adapter.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_manager.hpp"
#include "port/private/multi_touch_pointer.hpp"
#include "port/private/threading.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {
namespace {

using DisplayService = service::Display;
using PixelFormat = DisplayService::PixelFormat;

constexpr uint8_t DOUBLE_FRAME_BUFFER_COUNT = 2;
constexpr uint8_t TRIPLE_FRAME_BUFFER_COUNT = 3;

size_t bytes_per_pixel(PixelFormat pixel_format)
{
    switch (pixel_format) {
    case PixelFormat::RGB565:
        return 2;
    case PixelFormat::RGB888:
        return 3;
    default:
        return 0;
    }
}

esp_lv_adapter_tear_avoid_mode_t select_tear_avoid_mode(uint8_t frame_buffer_count)
{
    if (frame_buffer_count >= TRIPLE_FRAME_BUFFER_COUNT) {
        return ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL;
    }
    if (frame_buffer_count == DOUBLE_FRAME_BUFFER_COUNT) {
        return ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT;
    }

    return ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
}

const char *tear_avoid_mode_to_name(esp_lv_adapter_tear_avoid_mode_t mode)
{
    switch (mode) {
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE:
        return "NONE";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL:
        return "DOUBLE_FULL";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL:
        return "TRIPLE_FULL";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT:
        return "DOUBLE_DIRECT";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL:
        return "TRIPLE_PARTIAL";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC:
        return "TE_SYNC";
    case ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_PARTIAL:
        return "DOUBLE_PARTIAL";
    default:
        return "UNKNOWN";
    }
}

} // namespace

class DisplaySourceImpl {
public:
    bool start(const DisplaySourceConfig &config);
    void stop_timers() {}
    void release_display_service();
    void stop();

    bool is_started() const
    {
        return started_;
    }

    lv_display_t *display() const
    {
        return display_;
    }

    lv_indev_t *input() const
    {
        return pointer_inputs_.primary_input();
    }

    std::string output_name() const
    {
        return output_name_;
    }

    uint32_t source_id() const
    {
        return source_id_;
    }

    uint32_t width() const
    {
        return output_width_;
    }

    uint32_t height() const
    {
        return output_height_;
    }

private:
    bool acquire_hal_interfaces();
    bool bind_display_service();
    bool select_output(std::string_view preferred_output_name);
    bool register_display_source(const DisplaySourceConfig &config);
    bool start_lvgl_adapter(const DisplaySourceConfig &config);
    bool register_panel_event_bridge();
    bool unregister_panel_event_bridge();
    esp_err_t submit_frame(int x_start, int y_start, int x_end, int y_end, const void *data);

    static bool panel_event_callback(uint8_t event, bool in_isr, void *user_ctx);
    static esp_err_t draw_bitmap_callback(
        lv_display_t *disp, esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end,
        const void *color_map, void *user_ctx
    );
    DisplaySourceConfig config_{};
    service::ServiceBinding display_service_binding_;
    esp_brookesia::lib_utils::connection source_state_connection_;
    hal::InterfaceHandle<hal::display::PanelIface> panel_iface_;
    hal::display::PanelIface::EventDispatcher panel_event_dispatcher_;
    uint32_t panel_event_listener_id_ = 0;
    PixelFormat pixel_format_ = PixelFormat::Max;

    bool started_ = false;
    lv_display_t *display_ = nullptr;
    MultiTouchPointerManager pointer_inputs_;
    std::string output_name_;
    uint32_t source_id_ = 0;
    uint32_t output_width_ = 0;
    uint32_t output_height_ = 0;
};

DisplaySource::DisplaySource()
    : impl_(std::make_unique<DisplaySourceImpl>())
{
}

DisplaySource::~DisplaySource() = default;

bool DisplaySource::start(const DisplaySourceConfig &config)
{
    return impl_->start(config);
}

void DisplaySource::stop_timers()
{
    impl_->stop_timers();
}

void DisplaySource::release_display_service()
{
    impl_->release_display_service();
}

void DisplaySource::stop()
{
    impl_->stop();
}

bool DisplaySource::is_started() const
{
    return impl_->is_started();
}

lv_display_t *DisplaySource::display() const
{
    return impl_->display();
}

lv_indev_t *DisplaySource::input() const
{
    return impl_->input();
}

std::string DisplaySource::output_name() const
{
    return impl_->output_name();
}

uint32_t DisplaySource::source_id() const
{
    return impl_->source_id();
}

uint32_t DisplaySource::width() const
{
    return impl_->width();
}

uint32_t DisplaySource::height() const
{
    return impl_->height();
}

bool DisplaySourceImpl::start(const DisplaySourceConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (started_) {
        return true;
    }

    config_ = config;
    BROOKESIA_CHECK_FALSE_RETURN(acquire_hal_interfaces(), false, "Failed to acquire display HAL interfaces");
    BROOKESIA_CHECK_FALSE_RETURN(bind_display_service(), false, "Failed to bind Display service");
    BROOKESIA_CHECK_FALSE_RETURN(select_output(config.output_name), false, "Failed to select Display output");
    BROOKESIA_CHECK_FALSE_RETURN(register_display_source(config), false, "Failed to register LVGL Display source");
    BROOKESIA_CHECK_FALSE_RETURN(start_lvgl_adapter(config), false, "Failed to start LVGL adapter Display source");

    started_ = true;
    return true;
}

void DisplaySourceImpl::stop()
{
    source_state_connection_.disconnect();
    (void)unregister_panel_event_bridge();

    if (is_timer_managed_by_port()) {
        lock_thread();
        pointer_inputs_.stop();
        unlock_thread();
    } else {
        pointer_inputs_.stop();
    }
    if (display_ != nullptr) {
        (void)esp_lv_adapter_unregister_display(display_);
        display_ = nullptr;
    }
    release_display_service();
    started_ = false;
}

void DisplaySourceImpl::release_display_service()
{
    if (source_id_ != 0) {
        auto &display_service = DisplayService::get_instance();
        if (!output_name_.empty()) {
            (void)display_service.release_output(source_id_, output_name_);
        }
        (void)display_service.unregister_source(source_id_);
    }
    display_service_binding_.release();
    source_id_ = 0;
    output_name_.clear();
    output_width_ = 0;
    output_height_ = 0;
    pixel_format_ = PixelFormat::Max;
}

bool DisplaySourceImpl::acquire_hal_interfaces()
{
    auto panel_iface = hal::acquire_first_interface<hal::display::PanelIface>();
    BROOKESIA_CHECK_NULL_RETURN(panel_iface, false, "Failed to acquire display panel interface");
    panel_iface_ = std::move(panel_iface);

    return true;
}

bool DisplaySourceImpl::bind_display_service()
{
    BROOKESIA_CHECK_FALSE_RETURN(DisplayService::Helper::is_available(), false, "Display service is not available");

    display_service_binding_ = service::ServiceManager::get_instance().bind(DisplayService::Helper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(display_service_binding_.is_valid(), false, "Failed to bind Display service");

    return true;
}

bool DisplaySourceImpl::select_output(std::string_view preferred_output_name)
{
    auto outputs = DisplayService::get_instance().get_outputs();
    BROOKESIA_CHECK_FALSE_RETURN(!outputs.empty(), false, "No Display output is available");

    const auto panel_instance_name = panel_iface_.instance_name();
    const auto *selected_output = &outputs.front();
    if (!preferred_output_name.empty()) {
        auto selected_it = std::find_if(outputs.begin(), outputs.end(), [preferred_output_name](const auto & output) {
            return output.name == preferred_output_name;
        });
        BROOKESIA_CHECK_FALSE_RETURN(selected_it != outputs.end(), false, "Preferred Display output is not available");
        selected_output = &(*selected_it);
    } else {
        for (const auto &output : outputs) {
            if (output.panel_instance == panel_instance_name) {
                selected_output = &output;
                break;
            }
        }
    }

    output_name_ = selected_output->name;
    output_width_ = selected_output->width;
    output_height_ = selected_output->height;
    pixel_format_ = selected_output->pixel_format;
    BROOKESIA_LOGI(
        "LVGL Display source uses output %1% (%2%x%3%, panel=%4%)", output_name_,
        selected_output->width, selected_output->height, selected_output->panel_instance
    );

    return true;
}

bool DisplaySourceImpl::register_display_source(const DisplaySourceConfig &config)
{
    DisplayService::SourceInfo source = {
        .name = config.source_name,
        .role = config.source_role,
        .preferred_outputs = {output_name_},
        .priority = 0,
    };
    auto register_result = DisplayService::get_instance().register_source(std::move(source));
    BROOKESIA_CHECK_FALSE_RETURN(
        register_result.has_value(), false, "Failed to register LVGL Display source: %1%", register_result.error()
    );
    source_id_ = register_result.value();

    auto request_result = DisplayService::get_instance().request_output(source_id_, output_name_);
    BROOKESIA_CHECK_FALSE_RETURN(
        request_result.has_value(), false, "Failed to request LVGL Display output: %1%", request_result.error()
    );

    return true;
}

bool DisplaySourceImpl::start_lvgl_adapter(const DisplaySourceConfig &config)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
#pragma GCC diagnostic pop
    adapter_config.task_priority = config.task_priority;
    adapter_config.task_core_id = config.task_core_id;
    adapter_config.task_stack_size = config.task_stack_size;
    adapter_config.tick_period_ms = config.tick_period_ms;
    adapter_config.task_min_delay_ms = config.task_min_delay_ms;
    adapter_config.task_max_delay_ms = config.task_max_delay_ms;
    adapter_config.stack_in_psram = config.stack_in_psram;
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_lv_adapter_init(&adapter_config), false, "Failed to initialize LVGL adapter");

    hal::display::PanelIface::DriverSpecific panel_specific;
    BROOKESIA_CHECK_FALSE_RETURN(
        panel_iface_->get_driver_specific(panel_specific), false, "Failed to get panel driver data"
    );
    panel_event_dispatcher_ = panel_specific.event_dispatcher;

    const auto &display_info = panel_iface_->get_info();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_display_config_t display_config = {};
    switch (panel_specific.bus_type) {
    case hal::display::PanelIface::BusType::Generic:
        display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                             reinterpret_cast<esp_lcd_panel_handle_t>(panel_specific.panel_handle),
                             reinterpret_cast<esp_lcd_panel_io_handle_t>(panel_specific.io_handle),
                             static_cast<uint16_t>(display_info.h_res),
                             static_cast<uint16_t>(display_info.v_res),
                             ESP_LV_ADAPTER_ROTATE_0
                         );
        break;
    case hal::display::PanelIface::BusType::MIPI:
        display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                             reinterpret_cast<esp_lcd_panel_handle_t>(panel_specific.panel_handle),
                             reinterpret_cast<esp_lcd_panel_io_handle_t>(panel_specific.io_handle),
                             static_cast<uint16_t>(display_info.h_res),
                             static_cast<uint16_t>(display_info.v_res),
                             ESP_LV_ADAPTER_ROTATE_0
                         );
        display_config.tear_avoid_mode = select_tear_avoid_mode(panel_specific.frame_buffer_count);
        break;
    case hal::display::PanelIface::BusType::RGB:
        display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                             reinterpret_cast<esp_lcd_panel_handle_t>(panel_specific.panel_handle),
                             reinterpret_cast<esp_lcd_panel_io_handle_t>(panel_specific.io_handle),
                             static_cast<uint16_t>(display_info.h_res),
                             static_cast<uint16_t>(display_info.v_res),
                             ESP_LV_ADAPTER_ROTATE_0
                         );
        display_config.tear_avoid_mode = select_tear_avoid_mode(panel_specific.frame_buffer_count);
        break;
    default:
        BROOKESIA_LOGE("Unsupported LVGL panel bus type: %1%", panel_specific.bus_type);
        return false;
    }
    display_config.profile.require_double_buffer = config.require_double_buffer;
    display_config.profile.buffer_height = config.buffer_height;
#pragma GCC diagnostic pop
    BROOKESIA_LOGI(
        "LVGL Display source uses bus %1%, frame buffers=%2%, tear mode=%3%",
        panel_specific.bus_type, panel_specific.frame_buffer_count,
        tear_avoid_mode_to_name(display_config.tear_avoid_mode)
    );

    // Disable adapter-owned callback registration before display creation. The callback slot is owned by
    // Brookesia HAL while this display source is active, and disabling after registration is too late.
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_lv_adapter_set_default_display_idf_callback_registration_enabled(false),
        false, "Failed to disable default LVGL adapter IDF callback registration"
    );
    {
        lib_utils::FunctionGuard restore_display_callback_default([]() {
            (void)esp_lv_adapter_set_default_display_idf_callback_registration_enabled(true);
        });
        display_ = esp_lv_adapter_register_display(&display_config);
    }
    BROOKESIA_CHECK_NULL_RETURN(display_, false, "Failed to register LVGL display");

    // Route LVGL flushes through the service/HAL draw path so they serialize with other
    // sources (e.g. emote) under the HAL panel mutex. Without this, the adapter's default
    // flush calls esp_lcd_panel_draw_bitmap directly (no lock), and during a source handover
    // it can interleave SPI bus transactions with emote's draw_bitmap_sync -> garbled display.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lv_adapter_draw_bitmap_callbacks_t draw_callbacks = {
        .custom_draw_bitmap = DisplaySourceImpl::draw_bitmap_callback,
    };
#pragma GCC diagnostic pop
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_lv_adapter_set_draw_bitmap_callbacks(display_, &draw_callbacks, this),
        false, "Failed to set LVGL draw bitmap callback"
    );
    if ((panel_specific.draw_x_align_bytes > 1) || (panel_specific.draw_y_align_bytes > 1)) {
        const uint16_t rounder_data = static_cast<uint16_t>(
                                          (panel_specific.draw_x_align_bytes << 8) | panel_specific.draw_y_align_bytes
                                      );
        auto rounder_event_cb = [](lv_event_t *event) {
            auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
            const uint16_t rounder_data = static_cast<uint16_t>(
                                              reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event))
                                          );
            const uint8_t draw_x_align_byte = static_cast<uint8_t>((rounder_data >> 8) & 0xFF) - 1;
            const uint8_t draw_y_align_byte = static_cast<uint8_t>(rounder_data & 0xFF) - 1;
            if ((area == nullptr) || (draw_x_align_byte >= 8) || (draw_y_align_byte >= 8)) {
                return;
            }
            if (draw_x_align_byte > 0) {
                const auto x1 = area->x1;
                const auto x2 = area->x2;
                area->x1 = (x1 >> draw_x_align_byte) << draw_x_align_byte;
                area->x2 = ((x2 >> draw_x_align_byte) << draw_x_align_byte) + ((1 << draw_x_align_byte) - 1);
            }
            if (draw_y_align_byte > 0) {
                const auto y1 = area->y1;
                const auto y2 = area->y2;
                area->y1 = (y1 >> draw_y_align_byte) << draw_y_align_byte;
                area->y2 = ((y2 >> draw_y_align_byte) << draw_y_align_byte) + ((1 << draw_y_align_byte) - 1);
            }
        };
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA,
                                reinterpret_cast<void *>(static_cast<std::uintptr_t>(rounder_data)));
    }

    const auto input_count = MultiTouchPointerManager::resolve_input_count(output_name_);
    BROOKESIA_CHECK_FALSE_RETURN(
        pointer_inputs_.start(display_, nullptr, output_name_, input_count), false,
        "Failed to create LVGL multi-touch pointer inputs"
    );
    BROOKESIA_LOGI("LVGL Display source created %1% pointer input(s)", input_count);

    BROOKESIA_CHECK_FALSE_RETURN(register_panel_event_bridge(), false, "Failed to register LVGL panel event bridge");

    auto source_state_slot = [this](
                                 const std::string & source_name, const std::string & output_name,
    DisplayService::SourceState state) {
        if ((source_name != config_.source_name) || (output_name != output_name_) || (display_ == nullptr)) {
            return;
        }
        if ((state != DisplayService::SourceState::Granted) &&
                (state != DisplayService::SourceState::Dummy)) {
            return;
        }

        const bool want_dummy = (state == DisplayService::SourceState::Dummy);
        if (esp_lv_adapter_get_dummy_draw_enabled(display_) != want_dummy) {
            auto dummy_draw_ret = esp_lv_adapter_set_dummy_draw(display_, want_dummy);
            if (dummy_draw_ret != ESP_OK) {
                BROOKESIA_LOGE("Failed to set LVGL dummy draw(%1%): %2%", want_dummy, esp_err_to_name(dummy_draw_ret));
            }
        }

        // if (state == DisplayService::SourceState::Granted) {
        //     auto refresh_ret = esp_lv_adapter_refresh_now(display_);
        //     if (refresh_ret != ESP_OK) {
        //         BROOKESIA_LOGW("Failed to refresh LVGL display source after state change: %1%", esp_err_to_name(refresh_ret));
        //     }
        // }
    };
    source_state_connection_ = DisplayService::get_instance().connect_source_state_changed(source_state_slot);
    BROOKESIA_CHECK_FALSE_RETURN(
        source_state_connection_.connected(), false,
        "Failed to subscribe Display SourceStateChanged for LVGL source"
    );

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_lv_adapter_start(), false, "Failed to start LVGL adapter");

    return true;
}

bool DisplaySourceImpl::register_panel_event_bridge()
{
    if (!panel_event_dispatcher_.is_valid() || (panel_event_listener_id_ != 0)) {
        return true;
    }

    return panel_event_dispatcher_.add_listener(
               panel_event_dispatcher_.ctx, DisplaySourceImpl::panel_event_callback, this, &panel_event_listener_id_
           );
}

bool DisplaySourceImpl::unregister_panel_event_bridge()
{
    if (!panel_event_dispatcher_.is_valid() || (panel_event_listener_id_ == 0)) {
        return true;
    }

    auto result = panel_event_dispatcher_.remove_listener(panel_event_dispatcher_.ctx, panel_event_listener_id_);
    if (result) {
        panel_event_listener_id_ = 0;
    }
    return result;
}

esp_err_t DisplaySourceImpl::submit_frame(int x_start, int y_start, int x_end, int y_end, const void *data)
{
    BROOKESIA_CHECK_NULL_RETURN(data, ESP_ERR_INVALID_ARG, "LVGL frame data is null");
    BROOKESIA_CHECK_FALSE_RETURN(source_id_ != 0, ESP_ERR_INVALID_STATE, "LVGL Display source is not registered");
    BROOKESIA_CHECK_FALSE_RETURN(!output_name_.empty(), ESP_ERR_INVALID_STATE, "LVGL Display output is not selected");
    BROOKESIA_CHECK_FALSE_RETURN((x_end > x_start) && (y_end > y_start), ESP_ERR_INVALID_ARG, "Invalid LVGL frame area");

    const size_t bpp = bytes_per_pixel(pixel_format_);
    BROOKESIA_CHECK_FALSE_RETURN(bpp > 0, ESP_ERR_NOT_SUPPORTED, "Unsupported Display pixel format");

    const uint32_t width = static_cast<uint32_t>(x_end - x_start);
    const uint32_t height = static_cast<uint32_t>(y_end - y_start);
    DisplayService::FrameInfo frame = {
        .x = static_cast<uint32_t>(x_start),
        .y = static_cast<uint32_t>(y_start),
        .width = width,
        .height = height,
        .pixel_format = pixel_format_,
    };

    auto present_result = DisplayService::get_instance().present_frame_sync(
                              source_id_, output_name_, frame,
                              service::RawBuffer(data, static_cast<size_t>(width) * height * bpp),
                              0
                          );

    switch (present_result) {
    case DisplayService::PresentResult::Presented:
        return ESP_OK;
    case DisplayService::PresentResult::DroppedNotActive:
        return ESP_ERR_NOT_ALLOWED;
    case DisplayService::PresentResult::DroppedInvalidFrame:
    case DisplayService::PresentResult::Error:
    default:
        BROOKESIA_LOGW("LVGL Display frame was not presented: %1%", BROOKESIA_DESCRIBE_ENUM_TO_STR(present_result));
        return ESP_ERR_INVALID_STATE;
    }
}

bool DisplaySourceImpl::panel_event_callback(uint8_t event, bool in_isr, void *user_ctx)
{
    (void)in_isr;

    auto *self = static_cast<DisplaySourceImpl *>(user_ctx);
    if ((self == nullptr) || (self->display_ == nullptr)) {
        return false;
    }

    auto event_type = static_cast<hal::display::PanelIface::EventType>(event);
    switch (event_type) {
    case hal::display::PanelIface::EventType::ColorTransDone:
        return esp_lv_adapter_display_notify_color_trans_done_from_isr(self->display_);
    case hal::display::PanelIface::EventType::FrameDone: {
        // The HAL owns the ESP-IDF panel callback slot (adapter self-registration was disabled via
        // esp_lv_adapter_set_default_display_idf_callback_registration_enabled(false)), so the adapter
        // never sees the panel's frame-buffer-complete / refresh-done interrupt and relies on this
        // forwarding instead. The HAL maps that interrupt to FrameDone, which drives two adapter paths:
        //   - notify_frame_buf_complete: releases/rotates a pipeline buffer. Buffer-switch tear-avoid
        //     modes (TRIPLE_PARTIAL / DOUBLE_DIRECT) and dummy-draw need this, otherwise the flush path
        //     blocks forever waiting for a free buffer.
        //   - notify_frame_done: signals vsync / dummy-draw frame-done handling.
        // Both must run; OR their yield hints so a higher-priority task is resumed if either requests it.
        const bool release_yield = esp_lv_adapter_display_notify_frame_buf_complete_from_isr(self->display_);
        const bool frame_yield = esp_lv_adapter_display_notify_frame_done_from_isr(self->display_);
        return release_yield || frame_yield;
    }
    default:
        return false;
    }
}

esp_err_t DisplaySourceImpl::draw_bitmap_callback(
    lv_display_t *disp, esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end,
    const void *color_map, void *user_ctx
)
{
    (void)disp;
    (void)panel;

    auto *self = static_cast<DisplaySourceImpl *>(user_ctx);
    BROOKESIA_CHECK_NULL_RETURN(self, ESP_ERR_INVALID_ARG, "LVGL Display source callback context is null");

    auto ret = self->submit_frame(x_start, y_start, x_end, y_end, color_map);
    if (ret == ESP_ERR_NOT_ALLOWED) {
        BROOKESIA_LOGD("LVGL Display frame dropped because source is not active");
    }
    return ret;
}

} // namespace esp_brookesia::gui::lvgl
