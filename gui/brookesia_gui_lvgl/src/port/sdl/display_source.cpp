/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_lvgl/display_source.hpp"
#include "brookesia/gui_lvgl/backend.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#if !defined(BROOKESIA_GUI_LVGL_WASM_PORT)
#include <thread>
#endif
#include <utility>
#include <vector>

#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
#include "emscripten/emscripten.h"
#include "brookesia/gui_interface/wasm/gui_task_queue.hpp"
#include "brookesia/hal_wasm/display/device.hpp"
#endif
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_manager.hpp"
#include "lvgl/lvgl.h"
#include "port/private/multi_touch_pointer.hpp"
#include "port/private/threading.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {
namespace {

using DisplayService = service::Display;
using PixelFormat = DisplayService::PixelFormat;

constexpr uint32_t DEFAULT_TICK_PERIOD_MS = 5;

uint32_t tick_get_ms()
{
    static const auto start_time = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

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

lv_color_format_t to_lv_color_format(PixelFormat pixel_format)
{
    switch (pixel_format) {
    case PixelFormat::RGB565:
        return LV_COLOR_FORMAT_RGB565;
    case PixelFormat::RGB888:
        return LV_COLOR_FORMAT_RGB888;
    default:
        return LV_COLOR_FORMAT_UNKNOWN;
    }
}

} // namespace

class DisplaySourceImpl {
public:
    bool start(const DisplaySourceConfig &config);
    void stop_timers();
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
    bool ensure_service_manager();
    bool bind_display_service();
    bool select_output(std::string_view preferred_output_name);
    bool register_display_source(const DisplaySourceConfig &config);
    bool register_lvgl_display();
    bool register_lvgl_input();
    bool start_timer_thread();
    void stop_timer_thread();
    void timer_loop(uint32_t period_ms);
#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
    static void timer_loop_callback(void *arg);
#endif
    void flush_frame(lv_display_t *display, const lv_area_t *area, uint8_t *color_map);

    static void flush_callback(lv_display_t *display, const lv_area_t *area, uint8_t *color_map);

    DisplaySourceConfig config_{};
    service::ServiceBinding display_service_binding_;
    bool lvgl_initialized_by_us_ = false;
    bool started_ = false;
    lv_display_t *display_ = nullptr;
    MultiTouchPointerManager pointer_inputs_;
    lv_group_t *group_ = nullptr;
    std::string output_name_;
    uint32_t source_id_ = 0;
    uint32_t output_width_ = 0;
    uint32_t output_height_ = 0;
    PixelFormat pixel_format_ = PixelFormat::Max;
    std::vector<uint8_t> draw_buffer_;
    std::atomic_bool stop_timer_requested_ = false;
#if !defined(BROOKESIA_GUI_LVGL_WASM_PORT)
    std::thread timer_thread_;
#endif
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

    if (!lv_is_initialized()) {
        lv_init();
        lvgl_initialized_by_us_ = true;
    }
    lv_tick_set_cb(tick_get_ms);

    lib_utils::FunctionGuard cleanup([this]() {
        stop();
    });

    BROOKESIA_CHECK_FALSE_RETURN(ensure_service_manager(), false, "Failed to prepare ServiceManager");
    BROOKESIA_CHECK_FALSE_RETURN(bind_display_service(), false, "Failed to bind Display service");
    BROOKESIA_CHECK_FALSE_RETURN(select_output(config.output_name), false, "Failed to select Display output");
    BROOKESIA_CHECK_FALSE_RETURN(register_display_source(config), false, "Failed to register LVGL Display source");
    BROOKESIA_CHECK_FALSE_RETURN(register_lvgl_display(), false, "Failed to register LVGL display");
    BROOKESIA_CHECK_FALSE_RETURN(register_lvgl_input(), false, "Failed to register LVGL input");
    BROOKESIA_CHECK_FALSE_RETURN(start_timer_thread(), false, "Failed to start LVGL timer thread");

    started_ = true;
    cleanup.release();
    return true;
}

void DisplaySourceImpl::stop_timers()
{
    stop_timer_thread();
}

void DisplaySourceImpl::stop()
{
    stop_timer_thread();

    lock_thread();
    lib_utils::FunctionGuard unlock_guard(unlock_thread);
    pointer_inputs_.stop();
    if (display_ != nullptr) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
    if (group_ != nullptr) {
        lv_group_delete(group_);
        group_ = nullptr;
    }
    unlock_guard.release();
    unlock_thread();

    release_display_service();

    pixel_format_ = PixelFormat::Max;
    draw_buffer_.clear();
    started_ = false;

    if (lvgl_initialized_by_us_ && lv_is_initialized()) {
        lv_deinit();
        lvgl_initialized_by_us_ = false;
    }
}

bool DisplaySourceImpl::ensure_service_manager()
{
    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize ServiceManager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start ServiceManager");
    return true;
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

bool DisplaySourceImpl::bind_display_service()
{
    BROOKESIA_CHECK_FALSE_RETURN(DisplayService::Helper::is_available(), false, "Display service is not available");

    display_service_binding_ =
        service::ServiceManager::get_instance().bind(DisplayService::Helper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(display_service_binding_.is_valid(), false, "Failed to bind Display service");

    return true;
}

bool DisplaySourceImpl::select_output(std::string_view preferred_output_name)
{
    auto outputs = DisplayService::get_instance().get_outputs();
    BROOKESIA_CHECK_FALSE_RETURN(!outputs.empty(), false, "No Display output is available");

    const auto *selected_output = &outputs.front();
    if (!preferred_output_name.empty()) {
        auto selected_it = std::find_if(outputs.begin(), outputs.end(), [preferred_output_name](const auto & output) {
            return output.name == preferred_output_name;
        });
        BROOKESIA_CHECK_FALSE_RETURN(selected_it != outputs.end(), false, "Preferred Display output is not available");
        selected_output = &(*selected_it);
    }

    output_name_ = selected_output->name;
    output_width_ = selected_output->width;
    output_height_ = selected_output->height;
    pixel_format_ = selected_output->pixel_format;
    BROOKESIA_LOGI(
        "LVGL SDL Display source uses output %1% (%2%x%3%, panel=%4%)", output_name_,
        output_width_, output_height_, selected_output->panel_instance
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

bool DisplaySourceImpl::register_lvgl_display()
{
    const size_t bpp = bytes_per_pixel(pixel_format_);
    const auto color_format = to_lv_color_format(pixel_format_);
    BROOKESIA_CHECK_FALSE_RETURN(bpp > 0, false, "Unsupported Display pixel format");
    BROOKESIA_CHECK_FALSE_RETURN(color_format != LV_COLOR_FORMAT_UNKNOWN, false, "Unsupported LVGL color format");
    BROOKESIA_CHECK_FALSE_RETURN(output_width_ > 0 && output_height_ > 0, false, "Invalid Display output size");

    const size_t draw_buffer_size = static_cast<size_t>(output_width_) * output_height_ * bpp;
    draw_buffer_.assign(draw_buffer_size, 0);

    lock_thread();
    lib_utils::FunctionGuard unlock_guard(unlock_thread);
    display_ = lv_display_create(static_cast<int32_t>(output_width_), static_cast<int32_t>(output_height_));
    BROOKESIA_CHECK_NULL_RETURN(display_, false, "Failed to create LVGL display");
    lv_display_set_user_data(display_, this);
    lv_display_set_color_format(display_, color_format);
    lv_display_set_flush_cb(display_, DisplaySourceImpl::flush_callback);
    lv_display_set_buffers(display_, draw_buffer_.data(), nullptr, draw_buffer_.size(), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_default(display_);

    group_ = lv_group_create();
    BROOKESIA_CHECK_NULL_RETURN(group_, false, "Failed to create LVGL input group");
    lv_group_set_default(group_);
    return true;
}

bool DisplaySourceImpl::register_lvgl_input()
{
    lock_thread();
    lib_utils::FunctionGuard unlock_guard(unlock_thread);
    const auto input_count = MultiTouchPointerManager::resolve_input_count(output_name_);
    BROOKESIA_CHECK_FALSE_RETURN(
        pointer_inputs_.start(display_, group_, output_name_, input_count), false,
        "Failed to create LVGL multi-touch pointer inputs"
    );
    return true;
}

bool DisplaySourceImpl::start_timer_thread()
{
    stop_timer_requested_.store(false);
    set_timer_managed_by_port(true);
    const uint32_t period_ms = std::max(config_.tick_period_ms, DEFAULT_TICK_PERIOD_MS);
#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
    (void)period_ms;
    emscripten_set_main_loop_arg(DisplaySourceImpl::timer_loop_callback, this, 0, false);
    return true;
#else
    try {
        timer_thread_ = std::thread([this, period_ms]() {
            timer_loop(period_ms);
        });
    } catch (const std::exception &e) {
        set_timer_managed_by_port(false);
        BROOKESIA_LOGE("Failed to create LVGL timer thread: %1%", e.what());
        return false;
    }
    return true;
#endif
}

void DisplaySourceImpl::stop_timer_thread()
{
    stop_timer_requested_.store(true);
#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
    emscripten_cancel_main_loop();
#else
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
#endif
    set_timer_managed_by_port(false);
}

void DisplaySourceImpl::timer_loop(uint32_t period_ms)
{
#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
    (void)period_ms;
    if (stop_timer_requested_.load() || !lv_is_initialized()) {
        return;
    }
    hal::DisplayWasmDevice::get_instance().poll_events();
    esp_brookesia::gui::wasm::drain_gui_tasks();
    lock_thread();
    esp_brookesia::gui::wasm::set_gui_task_blocked(true);
    auto unlock_guard = lib_utils::FunctionGuard([]() {
        esp_brookesia::gui::wasm::set_gui_task_blocked(false);
        unlock_thread();
    });
    lv_timer_handler();
    unlock_guard.release();
    esp_brookesia::gui::wasm::set_gui_task_blocked(false);
    unlock_thread();
    esp_brookesia::gui::wasm::drain_gui_tasks();
#else
    while (!stop_timer_requested_.load() && lv_is_initialized()) {
        lock_thread();
        lv_timer_handler();
        unlock_thread();
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
#endif
}

#if defined(BROOKESIA_GUI_LVGL_WASM_PORT)
void DisplaySourceImpl::timer_loop_callback(void *arg)
{
    auto *self = static_cast<DisplaySourceImpl *>(arg);
    if (self != nullptr) {
        self->timer_loop(DEFAULT_TICK_PERIOD_MS);
    }
}
#endif

void DisplaySourceImpl::flush_frame(lv_display_t *display, const lv_area_t *area, uint8_t *color_map)
{
    auto ready_guard = lib_utils::FunctionGuard([display]() {
        lv_display_flush_ready(display);
    });
    if ((area == nullptr) || (color_map == nullptr) || source_id_ == 0 || output_name_.empty()) {
        return;
    }
    if ((area->x2 < area->x1) || (area->y2 < area->y1)) {
        return;
    }

    const size_t bpp = bytes_per_pixel(pixel_format_);
    if (bpp == 0) {
        return;
    }
    const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    DisplayService::FrameInfo frame = {
        .x = static_cast<uint32_t>(std::max<int32_t>(area->x1, 0)),
        .y = static_cast<uint32_t>(std::max<int32_t>(area->y1, 0)),
        .width = width,
        .height = height,
        .pixel_format = pixel_format_,
    };

    auto present_result = DisplayService::get_instance().present_frame_sync(
                              source_id_, output_name_, frame,
                              service::RawBuffer(color_map, static_cast<size_t>(width) * height * bpp),
                              config_.frame_timeout_ms
                          );
    if ((present_result != DisplayService::PresentResult::Presented) &&
            (present_result != DisplayService::PresentResult::DroppedNotActive)) {
        BROOKESIA_LOGW("LVGL Display frame was not presented: %1%", BROOKESIA_DESCRIBE_ENUM_TO_STR(present_result));
    }
}

void DisplaySourceImpl::flush_callback(lv_display_t *display, const lv_area_t *area, uint8_t *color_map)
{
    auto *self = static_cast<DisplaySourceImpl *>(lv_display_get_user_data(display));
    if (self == nullptr) {
        lv_display_flush_ready(display);
        return;
    }
    self->flush_frame(display, area, color_map);
}

} // namespace esp_brookesia::gui::lvgl
