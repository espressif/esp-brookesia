/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#endif

#include "boost/format.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_display/service_display.hpp"

#if !BROOKESIA_SERVICE_DISPLAY_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::service {

namespace {

constexpr uint32_t INVALID_SOURCE_ID = 0;
constexpr uint32_t INVALID_TOUCH_ID = 0;
constexpr float PI = 3.14159265358979323846F;
#if defined(ESP_PLATFORM)
constexpr const char *TOUCH_INTERRUPT_TASK_NAME = "svc_display_touch_irq";
constexpr uint32_t TOUCH_INTERRUPT_TASK_STACK_SIZE = 3 * 1024;
constexpr UBaseType_t TOUCH_INTERRUPT_TASK_PRIORITY = 5;
#endif

bool is_valid_source_name(std::string_view source_name)
{
    return !source_name.empty();
}

template <typename T>
std::expected<boost::json::array, std::string> to_json_array(const T &value)
{
    auto json_value = BROOKESIA_DESCRIBE_TO_JSON(value);
    if (!json_value.is_array()) {
        return std::unexpected("Failed to serialize Display data to JSON array");
    }
    return boost::json::array(json_value.as_array());
}

uint64_t get_current_time_ms()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

uint8_t to_area_mask(Display::TouchGestureArea area)
{
    return static_cast<uint8_t>(area);
}

} // namespace

struct Display::TouchInterruptBridge {
    Display *owner = nullptr;
    uint32_t touch_id = INVALID_TOUCH_ID;
#if defined(ESP_PLATFORM)
    SemaphoreHandle_t interrupt_sem = nullptr;
    SemaphoreHandle_t task_done_sem = nullptr;
    TaskHandle_t task = nullptr;
    std::atomic_bool running = false;
#endif

    bool start(Display *display, uint32_t id, const std::shared_ptr<hal::display::TouchIface> &touch)
    {
        BROOKESIA_CHECK_NULL_RETURN(display, false, "Display touch interrupt owner is null");
        BROOKESIA_CHECK_NULL_RETURN(touch, false, "Display touch interrupt HAL handle is null");
        owner = display;
        touch_id = id;

#if defined(ESP_PLATFORM)
        interrupt_sem = xSemaphoreCreateBinary();
        BROOKESIA_CHECK_NULL_RETURN(interrupt_sem, false, "Failed to create Display touch interrupt semaphore");
        task_done_sem = xSemaphoreCreateBinary();
        if (task_done_sem == nullptr) {
            BROOKESIA_LOGE("Failed to create Display touch task done semaphore");
            stop();
            return false;
        }
        running = true;
        const auto task_ret = xTaskCreate(
                                  TouchInterruptBridge::task_entry, TOUCH_INTERRUPT_TASK_NAME,
                                  TOUCH_INTERRUPT_TASK_STACK_SIZE, this, TOUCH_INTERRUPT_TASK_PRIORITY, &task
                              );
        if (task_ret != pdPASS) {
            running = false;
            BROOKESIA_LOGE("Failed to create Display touch interrupt task");
            stop();
            return false;
        }
#endif

        if (!touch->register_interrupt_handler(TouchInterruptBridge::interrupt_handler, this)) {
            BROOKESIA_LOGE("Failed to register Display touch interrupt handler");
            stop();
            return false;
        }
        return true;
    }

    void stop()
    {
#if defined(ESP_PLATFORM)
        running = false;
        if (interrupt_sem != nullptr) {
            xSemaphoreGive(interrupt_sem);
        }
        if ((task != nullptr) && (task_done_sem != nullptr)) {
            const auto done = xSemaphoreTake(task_done_sem, pdMS_TO_TICKS(200));
            if (done != pdTRUE) {
                BROOKESIA_LOGW("Timeout waiting for Display touch interrupt task to stop");
            }
        }
        task = nullptr;
        if (interrupt_sem != nullptr) {
            vSemaphoreDelete(interrupt_sem);
            interrupt_sem = nullptr;
        }
        if (task_done_sem != nullptr) {
            vSemaphoreDelete(task_done_sem);
            task_done_sem = nullptr;
        }
#endif
        owner = nullptr;
        touch_id = INVALID_TOUCH_ID;
    }

    bool notify()
    {
        if (owner == nullptr) {
            return false;
        }
        if (!owner->schedule_touch_read(touch_id)) {
            BROOKESIA_LOGW("Failed to schedule Display touch read from interrupt");
        }
        return false;
    }

    static bool interrupt_handler(void *ctx)
    {
        auto *bridge = static_cast<TouchInterruptBridge *>(ctx);
        if (bridge == nullptr) {
            return false;
        }
#if defined(ESP_PLATFORM)
        if (bridge->interrupt_sem == nullptr) {
            return false;
        }
        BaseType_t higher_priority_task_woken = pdFALSE;
        xSemaphoreGiveFromISR(bridge->interrupt_sem, &higher_priority_task_woken);
        return higher_priority_task_woken == pdTRUE;
#else
        return bridge->notify();
#endif
    }

#if defined(ESP_PLATFORM)
    void task_loop()
    {
        while (running.load()) {
            if (xSemaphoreTake(interrupt_sem, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            if (!running.load()) {
                break;
            }
            (void)notify();
        }
        if (task_done_sem != nullptr) {
            xSemaphoreGive(task_done_sem);
        }
    }

    static void task_entry(void *arg)
    {
        auto *bridge = static_cast<TouchInterruptBridge *>(arg);
        if (bridge != nullptr) {
            bridge->task_loop();
        }
        vTaskDelete(nullptr);
    }
#endif
};

std::string Display::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_DISPLAY_VER_MAJOR, BROOKESIA_SERVICE_DISPLAY_VER_MINOR,
               BROOKESIA_SERVICE_DISPLAY_VER_PATCH
           );
}

bool Display::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_DISPLAY_VER_MAJOR, BROOKESIA_SERVICE_DISPLAY_VER_MINOR,
        BROOKESIA_SERVICE_DISPLAY_VER_PATCH
    );

    auto panel_handles = hal::acquire_interfaces<hal::display::PanelIface>();
    auto touch_handles = hal::acquire_interfaces<hal::display::TouchIface>();
    auto backlight_handles = hal::acquire_interfaces<hal::display::BacklightIface>();
    uint32_t next_touch_id = 1;

    std::lock_guard lock(mutex_);
    outputs_.clear();
    sources_.clear();
    touches_.clear();
    next_output_id_ = 1;
    next_source_id_ = 1;
    next_frame_id_ = 1;

    for (auto &panel_handle : panel_handles) {
        const auto &info = panel_handle->get_info();
        const uint32_t output_id = next_output_id_++;
        OutputContext output{};
        output.info = OutputInfo{
            .id = output_id,
            .name = (boost::format("Output%1%") % (output_id - 1)).str(),
            .width = info.h_res,
            .height = info.v_res,
            .pixel_format = info.pixel_format,
            .slot = OutputSlot::HalPanel,
            .panel_instance = panel_handle.instance_name(),
            .group_id = info.group_id,
        };
        output.panel = std::move(panel_handle);
        output.draw_mutex = std::make_shared<std::mutex>();
        output.active_source_id = INVALID_SOURCE_ID;
        output.gesture_config = build_default_touch_gesture_config_locked(output);
        output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
        output.backlight_on = false;
        BROOKESIA_LOGI(
            "Registered display output %1%: %2%x%3%, pixel_format=%4%, panel=%5%, group_id=%6%", output.info.name,
            output.info.width, output.info.height, BROOKESIA_DESCRIBE_ENUM_TO_STR(output.info.pixel_format),
            output.info.panel_instance, output.info.group_id
        );
        outputs_.emplace(output_id, std::move(output));
    }

    for (auto &touch_handle : touch_handles) {
        const auto &info = touch_handle->get_info();
        const uint32_t touch_id = next_touch_id++;
        TouchContext touch = {
            .info = TouchInfo{
                .id = touch_id,
                .name = (boost::format("Touch%1%") % (touch_id - 1)).str(),
                .x_max = info.x_max,
                .y_max = info.y_max,
                .max_points = info.max_points,
                .operation_mode = info.operation_mode,
                .touch_instance = touch_handle.instance_name(),
                .group_id = info.group_id,
                .bound_outputs = {},
            },
            .touch = std::move(touch_handle),
            .snapshot = {},
            .interrupt_bridge = nullptr,
            .poll_task_id = 0,
            .read_scheduled = false,
            .injected_points = std::nullopt,
            .injected_sequence = 0,
        };
        BROOKESIA_LOGI(
            "Registered display touch %1%: %2%x%3%, max_points=%4%, operation_mode=%5%, touch=%6%, group_id=%7%",
            touch.info.name, touch.info.x_max, touch.info.y_max, touch.info.max_points,
            BROOKESIA_DESCRIBE_ENUM_TO_STR(touch.info.operation_mode), touch.info.touch_instance, touch.info.group_id
        );
        touches_.emplace(touch_id, std::move(touch));
    }

    bind_default_touches_locked();
    bind_backlights_to_outputs_locked(std::move(backlight_handles));

    if (outputs_.empty()) {
        BROOKESIA_LOGW("No HAL display panel interface is available");
    }
    if (touches_.empty()) {
        BROOKESIA_LOGW("No HAL display touch interface is available");
    }
    if (!hal::has_interface(hal::display::BacklightIface::NAME)) {
        BROOKESIA_LOGW("No HAL display backlight interface is available");
    }

    return true;
}

void Display::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_touch_gesture_tasks();
    stop_touch_tasks();
    touch_updated_signal_.disconnect_all_slots();
    touch_gesture_signal_.disconnect_all_slots();
    source_state_changed_signal_.disconnect_all_slots();
    active_source_changed_signal_.disconnect_all_slots();
    output_registered_signal_.disconnect_all_slots();
    output_unregistered_signal_.disconnect_all_slots();
    frame_presented_signal_.disconnect_all_slots();

    std::vector<AsyncFrame> dropped_frames;
    {
        std::lock_guard lock(mutex_);
        drop_pending_frames_locked(dropped_frames);
        sources_.clear();
        touches_.clear();
        outputs_.clear();
        next_output_id_ = 1;
        next_source_id_ = 1;
        next_frame_id_ = 1;
    }
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::Error);
    }
}

bool Display::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->configure_group(get_render_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure Display render task group");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->configure_group(get_touch_task_group(), {
        .enable_serial_execution = true,
    }), false, "Failed to configure Display touch task group");
    if (!start_touch_tasks()) {
        BROOKESIA_LOGW("Failed to start one or more Display touch tasks");
    }
    std::vector<uint32_t> enabled_gesture_outputs;
    {
        std::lock_guard lock(mutex_);
        for (const auto &[output_id, output] : outputs_) {
            if (output.gesture_config.enabled) {
                enabled_gesture_outputs.push_back(output_id);
            }
        }
    }
    for (const auto output_id : enabled_gesture_outputs) {
        if (!start_touch_gesture_task(output_id)) {
            BROOKESIA_LOGW("Failed to start Display touch gesture task for output %1%", output_id);
        }
    }
    {
        std::lock_guard lock(mutex_);
        for (auto &[_, output] : outputs_) {
            if (!output.backlight) {
                continue;
            }
#if BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_ENABLE_AUTO_LOAD_DATA
            load_backlight_data_from_storage_locked(output);
#endif
            if (!apply_backlight_state_to_hal(output)) {
                BROOKESIA_LOGW("Failed to apply Display backlight state for output %1%", output.info.name);
            }
        }
    }
    return true;
}

void Display::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_touch_gesture_tasks();
    stop_touch_tasks();
    auto scheduler = get_task_scheduler();
    if ((scheduler != nullptr) && scheduler->is_running()) {
        scheduler->cancel_group(get_render_task_group());
        (void)scheduler->wait_group(get_render_task_group(), 1000);
    }

    std::vector<AsyncFrame> dropped_frames;
    {
        std::lock_guard lock(mutex_);
        drop_pending_frames_locked(dropped_frames);
    }
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::Error);
    }
}

std::vector<Display::OutputInfo> Display::get_outputs() const
{
    std::lock_guard lock(mutex_);

    std::vector<OutputInfo> outputs;
    outputs.reserve(outputs_.size());
    for (const auto &[_, output] : outputs_) {
        outputs.push_back(output.info);
    }
    return outputs;
}

std::vector<Display::SourceInfo> Display::get_sources() const
{
    std::lock_guard lock(mutex_);

    std::vector<SourceInfo> sources;
    sources.reserve(sources_.size());
    for (const auto &[_, source] : sources_) {
        sources.push_back(source.info);
    }
    return sources;
}

std::expected<Display::BufferOutputView, std::string> Display::get_buffer_output(std::string_view output_name) const
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }

    const auto &output = outputs_.at(output_id.value());
    if (output.info.slot != OutputSlot::Buffer) {
        return std::unexpected((boost::format("Display output '%1%' is not a buffer output") %
                                output.info.name).str());
    }
    if ((output.buffer.buffer.data_ptr == nullptr) || (output.buffer.stride_bytes == 0)) {
        return std::unexpected((boost::format("Display output '%1%' buffer is not available") %
                                output.info.name).str());
    }

    return BufferOutputView{
        .info = output.info,
        .buffer = output.buffer.buffer,
        .stride_bytes = output.buffer.stride_bytes,
    };
}

std::expected<uint32_t, std::string> Display::register_output(BufferOutputConfig config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (config.name.empty()) {
        return std::unexpected("Display output name cannot be empty");
    }
    const size_t bpp = bytes_per_pixel(config.pixel_format);
    if (!is_buffer_output_valid(config, bpp)) {
        return std::unexpected((boost::format("Invalid buffer display output '%1%'") % config.name).str());
    }

    OutputInfo registered_info;
    uint32_t output_id = 0;
    {
        std::lock_guard lock(mutex_);
        auto duplicate_it = std::find_if(outputs_.begin(), outputs_.end(), [&config](const auto & item) {
            return item.second.info.name == config.name;
        });
        if (duplicate_it != outputs_.end()) {
            return std::unexpected((boost::format("Display output '%1%' is already registered") % config.name).str());
        }

        output_id = next_output_id_++;
        if (next_output_id_ == INVALID_SOURCE_ID) {
            next_output_id_ = 1;
        }

        const size_t stride_bytes = (config.stride_bytes == 0) ?
                                    (static_cast<size_t>(config.width) * bpp) : config.stride_bytes;
        OutputContext output{};
        output.info = OutputInfo{
            .id = output_id,
            .name = std::move(config.name),
            .width = config.width,
            .height = config.height,
            .pixel_format = config.pixel_format,
            .slot = OutputSlot::Buffer,
            .panel_instance = {},
            .group_id = {},
        };
        output.buffer = BufferOutputContext{
            .buffer = config.buffer,
            .stride_bytes = stride_bytes,
        };
        output.draw_mutex = std::make_shared<std::mutex>();
        output.active_source_id = INVALID_SOURCE_ID;
        output.gesture_config = build_default_touch_gesture_config_locked(output);
        output.backlight_brightness = BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT;
        output.backlight_on = false;
        output.dynamic_output = true;
        registered_info = output.info;
        outputs_.emplace(output_id, std::move(output));
    }

    BROOKESIA_LOGI(
        "Registered buffer display output %1%: %2%x%3%, pixel_format=%4%", registered_info.name,
        registered_info.width, registered_info.height, BROOKESIA_DESCRIBE_ENUM_TO_STR(registered_info.pixel_format)
    );
    emit_output_registered(registered_info);
    return output_id;
}

std::expected<void, std::string> Display::unregister_output(std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (output_name.empty()) {
        return std::unexpected("Display output name cannot be empty");
    }

    uint32_t output_id = 0;
    std::shared_ptr<std::mutex> draw_mutex;
    {
        std::lock_guard lock(mutex_);
        auto parsed_output_id = find_output_id_locked(output_name);
        if (!parsed_output_id) {
            return std::unexpected(parsed_output_id.error());
        }
        output_id = parsed_output_id.value();
        const auto &output = outputs_.at(output_id);
        if (!output.dynamic_output) {
            return std::unexpected((boost::format("Display output '%1%' is not dynamic") % output.info.name).str());
        }
        draw_mutex = output.draw_mutex;
    }

    if (!draw_mutex) {
        return std::unexpected("Display output draw mutex is not available");
    }
    stop_touch_gesture_task(output_id);
    std::lock_guard draw_lock(*draw_mutex);

    std::string resolved_output_name;
    std::string active_source_name;
    std::vector<std::string> revoked_source_names;
    std::vector<AsyncFrame> dropped_frames;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(output_id);
        if (output_it == outputs_.end()) {
            return {};
        }
        auto &output = output_it->second;
        if (!output.dynamic_output) {
            return std::unexpected((boost::format("Display output '%1%' is not dynamic") % output.info.name).str());
        }
        resolved_output_name = output.info.name;
        if (output.active_source_id != INVALID_SOURCE_ID) {
            auto source_it = sources_.find(output.active_source_id);
            if (source_it != sources_.end()) {
                active_source_name = source_it->second.info.name;
                revoked_source_names.push_back(active_source_name);
            }
        }
        if (output.pending_frame.has_value()) {
            dropped_frames.push_back(std::move(output.pending_frame.value()));
            output.pending_frame.reset();
        }

        for (auto &[_, source] : sources_) {
            if (source.requested_outputs.erase(resolved_output_name) > 0) {
                if (source.info.name != active_source_name) {
                    revoked_source_names.push_back(source.info.name);
                }
            }
        }
        outputs_.erase(output_it);
        refresh_touch_bound_outputs_locked();
    }

    if (!active_source_name.empty()) {
        emit_active_source_changed(resolved_output_name, "");
    }
    for (const auto &source_name : revoked_source_names) {
        emit_source_state_changed(source_name, resolved_output_name, SourceState::Revoked);
    }
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::Error);
    }
    emit_output_unregistered(resolved_output_name);
    return {};
}

std::expected<void, std::string> Display::set_touch_gesture_config(
    uint32_t output_id, TouchGestureConfig config
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    uint32_t resolved_output_id = 0;
    bool should_start = false;
    bool should_stop = false;
    {
        std::lock_guard lock(mutex_);
        if (outputs_.find(output_id) == outputs_.end()) {
            return std::unexpected((boost::format("Display output id %1% is not available") % output_id).str());
        }
        auto &output = outputs_.at(output_id);
        auto resolved_config = resolve_touch_gesture_config_locked(output, config);
        if (!resolved_config) {
            return std::unexpected(resolved_config.error());
        }

        resolved_output_id = output_id;
        should_stop = output.gesture_config.enabled && !resolved_config->enabled;
        should_start = resolved_config->enabled;
        output.gesture_config = resolved_config.value();
        output.gesture_direction_tan_threshold =
            std::tan((static_cast<float>(output.gesture_config.threshold.direction_angle) * PI) / 180.0F);
        reset_touch_gesture_state_locked(output);
    }

    if (should_stop) {
        stop_touch_gesture_task(resolved_output_id);
    }
    if (should_start && is_running()) {
        if (!start_touch_gesture_task(resolved_output_id)) {
            return std::unexpected("Failed to start Display touch gesture task");
        }
    }
    return {};
}

std::expected<Display::TouchGestureConfig, std::string> Display::get_touch_gesture_config(
    uint32_t output_id
) const
{
    std::lock_guard lock(mutex_);

    if (outputs_.find(output_id) == outputs_.end()) {
        return std::unexpected((boost::format("Display output id %1% is not available") % output_id).str());
    }
    return outputs_.at(output_id).gesture_config;
}

std::expected<Display::TouchSnapshot, std::string> Display::get_touch_snapshot(std::string_view output_name) const
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }
    const auto touch_id = outputs_.at(output_id.value()).touch_id;
    if (touch_id == INVALID_TOUCH_ID) {
        return std::unexpected((boost::format("Display output '%1%' has no bound touch") %
                                outputs_.at(output_id.value()).info.name).str());
    }
    auto touch_it = touches_.find(touch_id);
    if (touch_it == touches_.end()) {
        return std::unexpected((boost::format("Bound display touch %1% is not available") % touch_id).str());
    }
    // Synthetic injection (if active) overrides the hardware snapshot so callers can simulate input.
    if (touch_it->second.injected_points.has_value()) {
        TouchSnapshot snapshot = touch_it->second.snapshot;
        snapshot.points = *touch_it->second.injected_points;
        // Use the per-touch injection counter (bumped by inject_touch) so every press/release is
        // observed exactly once by the input reader, which de-duplicates by sequence.
        snapshot.sequence = touch_it->second.injected_sequence;
        snapshot.updated_at_ms = get_current_time_ms();
        snapshot.valid = true;
        return snapshot;
    }
    return touch_it->second.snapshot;
}

std::expected<void, std::string> Display::inject_touch(std::string_view output_name, std::vector<TouchPoint> points)
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }
    const auto touch_id = outputs_.at(output_id.value()).touch_id;
    if (touch_id == INVALID_TOUCH_ID) {
        return std::unexpected((boost::format("Display output '%1%' has no bound touch") %
                                outputs_.at(output_id.value()).info.name).str());
    }
    auto touch_it = touches_.find(touch_id);
    if (touch_it == touches_.end()) {
        return std::unexpected((boost::format("Bound display touch %1% is not available") % touch_id).str());
    }
    // Advance past the hardware sequence so the override is always seen as a fresh frame.
    touch_it->second.injected_sequence =
        std::max(touch_it->second.injected_sequence, touch_it->second.snapshot.sequence) + 1;
    touch_it->second.injected_points = std::move(points);
    return {};
}

std::expected<void, std::string> Display::inject_touch(std::string_view output_name, int32_t x, int32_t y)
{
    std::vector<TouchPoint> points;
    points.push_back(TouchPoint{
        .x = static_cast<int16_t>(x),
        .y = static_cast<int16_t>(y),
        .pressure = 1,
        .track_id = 0,
    });
    return inject_touch(output_name, std::move(points));
}

std::expected<void, std::string> Display::clear_injected_touch(std::string_view output_name)
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }
    const auto touch_id = outputs_.at(output_id.value()).touch_id;
    if (touch_id == INVALID_TOUCH_ID) {
        return std::unexpected((boost::format("Display output '%1%' has no bound touch") %
                                outputs_.at(output_id.value()).info.name).str());
    }
    auto touch_it = touches_.find(touch_id);
    if (touch_it == touches_.end()) {
        return std::unexpected((boost::format("Bound display touch %1% is not available") % touch_id).str());
    }
    touch_it->second.injected_points.reset();
    return {};
}

std::expected<hal::display::TouchIface::DriverSpecific, std::string> Display::get_touch_driver_specific(
    std::string_view output_name
) const
{
    std::shared_ptr<hal::display::TouchIface> touch;
    std::string resolved_output_name;
    {
        std::lock_guard lock(mutex_);
        auto output_id = find_output_id_locked(output_name);
        if (!output_id) {
            return std::unexpected(output_id.error());
        }
        const auto &output = outputs_.at(output_id.value());
        resolved_output_name = output.info.name;
        if (output.touch_id == INVALID_TOUCH_ID) {
            return std::unexpected((boost::format("Display output '%1%' has no bound touch") %
                                    resolved_output_name).str());
        }
        auto touch_it = touches_.find(output.touch_id);
        if (touch_it == touches_.end()) {
            return std::unexpected((boost::format("Bound display touch %1% is not available") %
                                    output.touch_id).str());
        }
        touch = touch_it->second.touch.get();
    }

    if (!touch) {
        return std::unexpected((boost::format("Display output '%1%' touch handle is not available") %
                                resolved_output_name).str());
    }
    hal::display::TouchIface::DriverSpecific specific;
    if (!touch->get_driver_specific(specific)) {
        return std::unexpected((boost::format("Failed to get Display output '%1%' touch driver data") %
                                resolved_output_name).str());
    }
    return specific;
}

esp_brookesia::lib_utils::connection Display::connect_touch_updated(
    std::string_view output_name, const TouchUpdatedSignal::slot_type &slot
)
{
    std::string filtered_output_name(output_name);
    if (filtered_output_name.empty()) {
        std::lock_guard lock(mutex_);
        if (!outputs_.empty()) {
            filtered_output_name = outputs_.begin()->second.info.name;
        }
    }
    return touch_updated_signal_.connect([filtered_output_name, slot](const std::string & updated_output_name,
    const TouchSnapshot & snapshot) {
        if (updated_output_name == filtered_output_name) {
            slot(updated_output_name, snapshot);
        }
    });
}

esp_brookesia::lib_utils::connection Display::connect_touch_gesture(
    std::string_view output_name, const TouchGestureSignal::slot_type &slot
)
{
    std::string filtered_output_name(output_name);
    if (filtered_output_name.empty()) {
        std::lock_guard lock(mutex_);
        if (!outputs_.empty()) {
            filtered_output_name = outputs_.begin()->second.info.name;
        }
    }
    return touch_gesture_signal_.connect(
    [filtered_output_name, slot](const std::string & updated_output_name, const TouchGestureInfo & info) {
        if (updated_output_name == filtered_output_name) {
            slot(updated_output_name, info);
        }
    }
           );
}

esp_brookesia::lib_utils::connection Display::connect_frame_presented(
    std::string_view output_name, const FramePresentedSignal::slot_type &slot
)
{
    std::string filtered_output_name(output_name);
    if (filtered_output_name.empty()) {
        std::lock_guard lock(mutex_);
        if (!outputs_.empty()) {
            filtered_output_name = outputs_.begin()->second.info.name;
        }
    }
    auto filtered_slot = [filtered_output_name, slot](
                             const std::string & presented_output_name, const FrameInfo & frame
    ) {
        if (presented_output_name == filtered_output_name) {
            slot(presented_output_name, frame);
        }
    };
    return frame_presented_signal_.connect(filtered_slot);
}

std::expected<uint32_t, std::string> Display::register_source(SourceInfo source)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_valid_source_name(source.name)) {
        return std::unexpected("Display source name cannot be empty");
    }

    uint32_t assigned_source_id = INVALID_SOURCE_ID;
    {
        std::lock_guard lock(mutex_);
        auto duplicate_it = std::find_if(sources_.begin(), sources_.end(), [&source](const auto & item) {
            return item.second.info.name == source.name;
        });
        if (duplicate_it != sources_.end()) {
            return std::unexpected((boost::format("Display source '%1%' is already registered") % source.name).str());
        }

        assigned_source_id = next_source_id_++;
        if (next_source_id_ == INVALID_SOURCE_ID) {
            next_source_id_ = 1;
        }
        source.id = assigned_source_id;
        sources_.emplace(assigned_source_id, SourceContext{
            .info = source,
            .requested_outputs = {},
        });
    }

    emit_source_state_changed(source.name, "", SourceState::Registered);
    return assigned_source_id;
}

std::expected<void, std::string> Display::unregister_source(uint32_t source_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string source_name;
    std::vector<std::string> cleared_outputs;
    std::vector<AsyncFrame> dropped_frames;
    {
        std::lock_guard lock(mutex_);
        auto source_it = sources_.find(source_id);
        if (source_it == sources_.end()) {
            return std::unexpected((boost::format("Display source %1% is not registered") % source_id).str());
        }

        source_name = source_it->second.info.name;
        sources_.erase(source_it);
        for (auto &[_, output] : outputs_) {
            if (output.active_source_id == source_id) {
                output.active_source_id = INVALID_SOURCE_ID;
                cleared_outputs.push_back(output.info.name);
            }
            if (output.pending_frame.has_value() && (output.pending_frame->source_id == source_id)) {
                dropped_frames.push_back(std::move(output.pending_frame.value()));
                output.pending_frame.reset();
            }
        }
    }

    for (const auto &output_name : cleared_outputs) {
        emit_active_source_changed(output_name, "");
        emit_source_state_changed(source_name, output_name, SourceState::Revoked);
    }
    emit_source_state_changed(source_name, "", SourceState::Revoked);
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::DroppedNotActive);
    }
    return {};
}

std::expected<void, std::string> Display::unregister_source(std::string_view source_name)
{
    uint32_t source_id = INVALID_SOURCE_ID;
    {
        std::lock_guard lock(mutex_);
        auto parsed_source_id = find_source_id_locked(source_name);
        if (!parsed_source_id) {
            return std::unexpected(parsed_source_id.error());
        }
        source_id = parsed_source_id.value();
    }
    return unregister_source(source_id);
}

std::expected<void, std::string> Display::request_output(uint32_t source_id, std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string source_name;
    std::string resolved_output_name;
    {
        std::lock_guard lock(mutex_);
        auto validation = validate_source_output_locked(source_id, output_name);
        if (!validation) {
            return std::unexpected(validation.error());
        }
        auto &source = sources_.at(source_id);
        auto output_id = find_output_id_locked(output_name).value();
        resolved_output_name = outputs_.at(output_id).info.name;
        source.requested_outputs.insert(resolved_output_name);
        source_name = source.info.name;
    }

    emit_source_state_changed(source_name, resolved_output_name, SourceState::Requested);
    return {};
}

std::expected<void, std::string> Display::request_output(
    std::string_view source_name, std::string_view output_name
)
{
    uint32_t source_id = INVALID_SOURCE_ID;
    {
        std::lock_guard lock(mutex_);
        auto parsed_source_id = find_source_id_locked(source_name);
        if (!parsed_source_id) {
            return std::unexpected(parsed_source_id.error());
        }
        source_id = parsed_source_id.value();
    }
    return request_output(source_id, output_name);
}

std::expected<void, std::string> Display::release_output(uint32_t source_id, std::string_view output_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string source_name;
    std::string resolved_output_name;
    bool cleared_active = false;
    std::vector<AsyncFrame> dropped_frames;
    {
        std::lock_guard lock(mutex_);
        auto validation = validate_source_output_locked(source_id, output_name);
        if (!validation) {
            return std::unexpected(validation.error());
        }

        auto output_id = find_output_id_locked(output_name).value();
        auto &source = sources_.at(source_id);
        auto &output = outputs_.at(output_id);
        source.requested_outputs.erase(output.info.name);
        source_name = source.info.name;
        resolved_output_name = output.info.name;
        if (output.active_source_id == source_id) {
            output.active_source_id = INVALID_SOURCE_ID;
            cleared_active = true;
        }
        if (output.pending_frame.has_value() && (output.pending_frame->source_id == source_id)) {
            dropped_frames.push_back(std::move(output.pending_frame.value()));
            output.pending_frame.reset();
        }
    }

    if (cleared_active) {
        emit_active_source_changed(resolved_output_name, "");
    }
    emit_source_state_changed(source_name, resolved_output_name, SourceState::Revoked);
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::DroppedNotActive);
    }
    return {};
}

std::expected<void, std::string> Display::release_output(
    std::string_view source_name, std::string_view output_name
)
{
    uint32_t source_id = INVALID_SOURCE_ID;
    {
        std::lock_guard lock(mutex_);
        auto parsed_source_id = find_source_id_locked(source_name);
        if (!parsed_source_id) {
            return std::unexpected(parsed_source_id.error());
        }
        source_id = parsed_source_id.value();
    }
    return release_output(source_id, output_name);
}

std::expected<void, std::string> Display::set_active_source(
    std::string_view output_name, std::string_view source_name
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::string resolved_output_name;
    std::string previous_source_name;
    std::string next_source_name(source_name);
    uint32_t previous_source_id = INVALID_SOURCE_ID;
    uint32_t next_source_id = INVALID_SOURCE_ID;
    std::vector<AsyncFrame> dropped_frames;

    {
        std::lock_guard lock(mutex_);
        auto output_id = find_output_id_locked(output_name);
        if (!output_id) {
            return std::unexpected(output_id.error());
        }

        auto &output = outputs_.at(output_id.value());
        resolved_output_name = output.info.name;
        previous_source_id = output.active_source_id;
        if (previous_source_id != INVALID_SOURCE_ID) {
            previous_source_name = sources_.at(previous_source_id).info.name;
        }

        if (!source_name.empty()) {
            auto source_id = find_source_id_locked(source_name);
            if (!source_id) {
                return std::unexpected(source_id.error());
            }
            next_source_id = source_id.value();
            auto &source = sources_.at(next_source_id);
            next_source_name = source.info.name;
            source.requested_outputs.insert(resolved_output_name);
        } else {
            next_source_name.clear();
        }

        output.active_source_id = next_source_id;
        if ((previous_source_id != INVALID_SOURCE_ID) && (previous_source_id != next_source_id) &&
                output.pending_frame.has_value() && (output.pending_frame->source_id == previous_source_id)) {
            dropped_frames.push_back(std::move(output.pending_frame.value()));
            output.pending_frame.reset();
        }
    }

    if ((previous_source_id != INVALID_SOURCE_ID) && (previous_source_id != next_source_id)) {
        emit_source_state_changed(previous_source_name, resolved_output_name, SourceState::Dummy);
    }
    if (next_source_id != INVALID_SOURCE_ID) {
        emit_source_state_changed(next_source_name, resolved_output_name, SourceState::Granted);
    }
    emit_active_source_changed(resolved_output_name, next_source_name);
    for (auto &frame : dropped_frames) {
        complete_async_frame(std::move(frame), PresentResult::DroppedNotActive);
    }
    return {};
}

std::expected<std::string, std::string> Display::get_active_source(std::string_view output_name) const
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }

    const auto &output = outputs_.at(output_id.value());
    if (output.active_source_id == INVALID_SOURCE_ID) {
        return std::string();
    }

    auto source_it = sources_.find(output.active_source_id);
    if (source_it == sources_.end()) {
        return std::unexpected((boost::format("Active display source %1% is not registered") %
                                output.active_source_id).str());
    }
    return source_it->second.info.name;
}

std::expected<void, std::string> Display::set_active_source_role(
    std::string_view output_name, std::string_view role
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (role.empty()) {
        return std::unexpected("Display source role cannot be empty");
    }

    std::string source_name;
    {
        std::lock_guard lock(mutex_);
        auto source_it = std::find_if(sources_.begin(), sources_.end(), [role](const auto & item) {
            return item.second.info.role == role;
        });
        if (source_it == sources_.end()) {
            return std::unexpected((boost::format("Display source role '%1%' is not registered") %
                                    std::string(role)).str());
        }
        source_name = source_it->second.info.name;
    }

    return set_active_source(output_name, source_name);
}

std::expected<std::string, std::string> Display::get_active_role(std::string_view output_name) const
{
    std::lock_guard lock(mutex_);

    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }

    const auto &output = outputs_.at(output_id.value());
    if (output.active_source_id == INVALID_SOURCE_ID) {
        return std::string();
    }

    auto source_it = sources_.find(output.active_source_id);
    if (source_it == sources_.end()) {
        return std::unexpected((boost::format("Active display source %1% is not registered") %
                                output.active_source_id).str());
    }
    return source_it->second.info.role;
}

std::vector<std::string> Display::get_source_roles() const
{
    std::lock_guard lock(mutex_);

    std::vector<std::string> roles;
    for (const auto &[_, source] : sources_) {
        const auto &role = source.info.role;
        if (role.empty()) {
            continue;
        }
        if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
            roles.push_back(role);
        }
    }
    return roles;
}

Display::PresentResult Display::present_frame_sync(
    uint32_t source_id, std::string_view output_name, const FrameInfo &frame, const RawBuffer &data,
    uint32_t timeout_ms
)
{
    OutputDrawTarget target;
    std::shared_ptr<std::mutex> draw_mutex;
    uint32_t output_id = 0;

    {
        std::lock_guard lock(mutex_);
        if (sources_.find(source_id) == sources_.end()) {
            return PresentResult::Error;
        }
        auto parsed_output_id = find_output_id_locked(output_name);
        if (!parsed_output_id) {
            return PresentResult::Error;
        }
        output_id = parsed_output_id.value();
        auto &output = outputs_.at(output_id);
        if (output.active_source_id != source_id) {
            return PresentResult::DroppedNotActive;
        }
        if ((data.data_ptr == nullptr) || !is_frame_valid_for_output(frame, output, data.data_size)) {
            return PresentResult::DroppedInvalidFrame;
        }

        draw_mutex = output.draw_mutex;
    }

    if (!draw_mutex) {
        return PresentResult::Error;
    }

    PresentResult result = PresentResult::Error;
    {
        std::lock_guard draw_lock(*draw_mutex);
        {
            std::lock_guard lock(mutex_);
            auto output_it = outputs_.find(output_id);
            if ((output_it == outputs_.end()) || (output_it->second.active_source_id != source_id)) {
                return PresentResult::DroppedNotActive;
            }
            const auto &output = output_it->second;
            target = OutputDrawTarget{
                .info = output.info,
                .panel = output.panel.get(),
                .buffer = output.buffer,
            };
        }
        result = present_frame_to_output(target, frame, data, timeout_ms);
    }

    if (result == PresentResult::Presented) {
        emit_frame_presented(target.info.name, frame);
    }
    return result;
}

Display::PresentResult Display::present_buffer_frame_sync(
    uint32_t source_id, std::string_view output_name, const FrameInfo &frame, BufferOutputWriter writer
)
{
    if (writer == nullptr) {
        return PresentResult::Error;
    }

    std::shared_ptr<std::mutex> draw_mutex;
    uint32_t output_id = 0;

    {
        std::lock_guard lock(mutex_);
        if (sources_.find(source_id) == sources_.end()) {
            return PresentResult::Error;
        }
        auto parsed_output_id = find_output_id_locked(output_name);
        if (!parsed_output_id) {
            return PresentResult::Error;
        }
        output_id = parsed_output_id.value();
        auto &output = outputs_.at(output_id);
        if (output.info.slot != OutputSlot::Buffer) {
            return PresentResult::Error;
        }
        if (output.active_source_id != source_id) {
            return PresentResult::DroppedNotActive;
        }
        const size_t bpp = bytes_per_pixel(frame.pixel_format);
        const size_t expected_size = static_cast<size_t>(frame.width) * frame.height * bpp;
        if ((bpp == 0) || !is_frame_valid_for_output(frame, output, expected_size)) {
            return PresentResult::DroppedInvalidFrame;
        }

        draw_mutex = output.draw_mutex;
    }

    if (!draw_mutex) {
        return PresentResult::Error;
    }

    std::string presented_output_name;
    PresentResult result = PresentResult::Error;
    {
        std::lock_guard draw_lock(*draw_mutex);
        BufferOutputView view;
        {
            std::lock_guard lock(mutex_);
            auto output_it = outputs_.find(output_id);
            if ((output_it == outputs_.end()) || (output_it->second.active_source_id != source_id)) {
                return PresentResult::DroppedNotActive;
            }
            auto &output = output_it->second;
            if (output.info.slot != OutputSlot::Buffer) {
                return PresentResult::Error;
            }
            view = BufferOutputView{
                .info = output.info,
                .buffer = output.buffer.buffer,
                .stride_bytes = output.buffer.stride_bytes,
            };
        }
        result = writer(view) ? PresentResult::Presented : PresentResult::Error;
        presented_output_name = view.info.name;
    }

    if (result == PresentResult::Presented) {
        emit_frame_presented(presented_output_name, frame);
    }
    return result;
}

Display::AsyncSubmitResult Display::present_frame_async(
    uint32_t source_id, std::string_view output_name, const FrameInfo &frame, const RawBuffer &data,
    CompletionCallback on_complete, uint32_t timeout_ms
)
{
    if (on_complete == nullptr) {
        return {
            .frame_id = 0,
            .state = PresentSubmitState::Error,
        };
    }

    AsyncFrame dropped_frame;
    bool has_dropped_frame = false;
    uint32_t frame_id = 0;
    PresentSubmitState submit_state = PresentSubmitState::Queued;
    {
        std::lock_guard lock(mutex_);
        if (sources_.find(source_id) == sources_.end()) {
            return {
                .frame_id = 0,
                .state = PresentSubmitState::Error,
            };
        }
        auto parsed_output_id = find_output_id_locked(output_name);
        if (!parsed_output_id) {
            return {
                .frame_id = 0,
                .state = PresentSubmitState::Error,
            };
        }

        auto &output = outputs_.at(parsed_output_id.value());
        if (output.active_source_id != source_id) {
            return {
                .frame_id = 0,
                .state = PresentSubmitState::DroppedNotActive,
            };
        }
        if ((data.data_ptr == nullptr) || !is_frame_valid_for_output(frame, output, data.data_size)) {
            return {
                .frame_id = 0,
                .state = PresentSubmitState::DroppedInvalidFrame,
            };
        }

        frame_id = allocate_frame_id_locked();
        if (output.pending_frame.has_value()) {
            dropped_frame = std::move(output.pending_frame.value());
            has_dropped_frame = true;
        }
        output.pending_frame = AsyncFrame{
            .frame_id = frame_id,
            .source_id = source_id,
            .output_name = output.info.name,
            .frame = frame,
            .data = data,
            .timeout_ms = timeout_ms,
            .on_complete = std::move(on_complete),
        };

        if (!schedule_render_output_locked(parsed_output_id.value())) {
            output.pending_frame.reset();
            frame_id = 0;
            submit_state = PresentSubmitState::Error;
        }
    }

    if (has_dropped_frame) {
        complete_async_frame(std::move(dropped_frame), PresentResult::DroppedQueueFull);
    }

    return {
        .frame_id = frame_id,
        .state = submit_state,
    };
}

std::expected<boost::json::array, std::string> Display::function_get_outputs()
{
    return to_json_array(get_outputs());
}

std::expected<boost::json::array, std::string> Display::function_get_sources()
{
    return to_json_array(get_sources());
}

std::expected<double, std::string> Display::function_register_source(const boost::json::object &source_json)
{
    SourceInfo source;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(source_json), source)) {
        return std::unexpected("Failed to parse Display source info");
    }
    auto result = register_source(std::move(source));
    if (!result) {
        return std::unexpected(result.error());
    }
    return static_cast<double>(result.value());
}

std::expected<void, std::string> Display::function_unregister_source(const std::string &source_name)
{
    return unregister_source(source_name);
}

std::expected<void, std::string> Display::function_request_output(
    const std::string &source_name, const std::string &output_name
)
{
    return request_output(source_name, output_name);
}

std::expected<void, std::string> Display::function_release_output(
    const std::string &source_name, const std::string &output_name
)
{
    return release_output(source_name, output_name);
}

std::expected<void, std::string> Display::function_set_active_source(
    const std::string &output_name, const std::string &source_name
)
{
    return set_active_source(output_name, source_name);
}

std::expected<std::string, std::string> Display::function_get_active_source(const std::string &output_name)
{
    return get_active_source(output_name);
}

std::expected<void, std::string> Display::function_set_active_source_role(
    const std::string &output_name, const std::string &role
)
{
    return set_active_source_role(output_name, role);
}

std::expected<std::string, std::string> Display::function_get_active_role(const std::string &output_name)
{
    return get_active_role(output_name);
}

std::expected<boost::json::array, std::string> Display::function_get_source_roles()
{
    return to_json_array(get_source_roles());
}

std::expected<void, std::string> Display::function_set_touch_gesture_config(
    double output_id, const boost::json::object &config_json
)
{
    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    TouchGestureConfig config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(config_json), config)) {
        return std::unexpected("Failed to parse Display touch gesture config");
    }
    return set_touch_gesture_config(parsed_output_id.value(), config);
}

std::expected<boost::json::object, std::string> Display::function_get_touch_gesture_config(
    double output_id
)
{
    auto parsed_output_id = validate_output_id_param(output_id);
    if (!parsed_output_id) {
        return std::unexpected(parsed_output_id.error());
    }
    auto result = get_touch_gesture_config(parsed_output_id.value());
    if (!result) {
        return std::unexpected(result.error());
    }
    return BROOKESIA_DESCRIBE_TO_JSON(result.value()).as_object();
}

std::expected<uint32_t, std::string> Display::find_output_id_locked(std::string_view output_name) const
{
    if (output_name.empty()) {
        if (outputs_.empty()) {
            return std::unexpected("No Display output is available");
        }
        return outputs_.begin()->first;
    }
    auto output_it = std::find_if(outputs_.begin(), outputs_.end(), [output_name](const auto & item) {
        return item.second.info.name == output_name;
    });
    if (output_it == outputs_.end()) {
        return std::unexpected((boost::format("Display output '%1%' is not available") %
                                std::string(output_name)).str());
    }
    return output_it->first;
}

std::expected<uint32_t, std::string> Display::validate_output_id_param(double output_id) const
{
    if (!std::isfinite(output_id)) {
        return std::unexpected("Display output id must be finite");
    }
    if (output_id < 0) {
        return std::unexpected("Display output id must be non-negative");
    }
    constexpr double output_id_max = static_cast<double>(std::numeric_limits<uint32_t>::max());
    if (output_id > output_id_max) {
        return std::unexpected("Display output id is out of range");
    }
    const auto rounded = std::round(output_id);
    if (rounded != output_id) {
        return std::unexpected("Display output id must be an integer");
    }
    return static_cast<uint32_t>(rounded);
}

std::expected<uint32_t, std::string> Display::find_source_id_locked(std::string_view source_name) const
{
    if (source_name.empty()) {
        return std::unexpected("Display source name cannot be empty");
    }
    auto source_it = std::find_if(sources_.begin(), sources_.end(), [source_name](const auto & item) {
        return item.second.info.name == source_name;
    });
    if (source_it == sources_.end()) {
        return std::unexpected((boost::format("Display source '%1%' is not registered") %
                                std::string(source_name)).str());
    }
    return source_it->first;
}

std::expected<uint32_t, std::string> Display::find_touch_id_locked(std::string_view touch_name) const
{
    if (touch_name.empty()) {
        return std::unexpected("Display touch name cannot be empty");
    }
    auto touch_it = std::find_if(touches_.begin(), touches_.end(), [touch_name](const auto & item) {
        return item.second.info.name == touch_name;
    });
    if (touch_it == touches_.end()) {
        return std::unexpected((boost::format("Display touch '%1%' is not available") %
                                std::string(touch_name)).str());
    }
    return touch_it->first;
}

std::expected<void, std::string> Display::validate_source_output_locked(
    uint32_t source_id, std::string_view output_name
) const
{
    if (sources_.find(source_id) == sources_.end()) {
        return std::unexpected((boost::format("Display source %1% is not registered") % source_id).str());
    }
    auto output_id = find_output_id_locked(output_name);
    if (!output_id) {
        return std::unexpected(output_id.error());
    }
    return {};
}

Display::TouchGestureConfig Display::build_default_touch_gesture_config_locked(const OutputContext &output) const
{
    TouchGestureConfig config;
    config.enabled = false;
    config.detect_period_ms = 20;
    config.direction_lock_enabled = true;
    config.release_debounce_ms = 40;
    config.threshold.direction_horizon = static_cast<uint16_t>(
            std::max<uint32_t>(24, output.info.width / 6)
                                         );
    config.threshold.direction_vertical = static_cast<uint16_t>(
            std::max<uint32_t>(24, output.info.height / 6)
                                          );
    config.threshold.direction_angle = 45;
    config.threshold.horizontal_edge = static_cast<uint16_t>(
                                           std::max<uint32_t>(12, output.info.width / 10)
                                       );
    config.threshold.vertical_edge = static_cast<uint16_t>(
                                         std::max<uint32_t>(12, output.info.height / 10)
                                     );
    config.threshold.duration_short_ms = 220;
    config.threshold.speed_slow_px_per_ms = 0.6F;
    return config;
}

std::expected<Display::TouchGestureConfig, std::string> Display::resolve_touch_gesture_config_locked(
    const OutputContext &output, const TouchGestureConfig &config
) const
{
    auto resolved = config;
    const auto defaults = build_default_touch_gesture_config_locked(output);
    if (resolved.detect_period_ms == 0) {
        resolved.detect_period_ms = defaults.detect_period_ms;
    }
    if (resolved.release_debounce_ms == 0) {
        resolved.release_debounce_ms = defaults.release_debounce_ms;
    }
    if (resolved.threshold.direction_horizon == 0) {
        resolved.threshold.direction_horizon = defaults.threshold.direction_horizon;
    }
    if (resolved.threshold.direction_vertical == 0) {
        resolved.threshold.direction_vertical = defaults.threshold.direction_vertical;
    }
    if (resolved.threshold.direction_angle == 0) {
        resolved.threshold.direction_angle = defaults.threshold.direction_angle;
    }
    if (resolved.threshold.horizontal_edge == 0) {
        resolved.threshold.horizontal_edge = defaults.threshold.horizontal_edge;
    }
    if (resolved.threshold.vertical_edge == 0) {
        resolved.threshold.vertical_edge = defaults.threshold.vertical_edge;
    }
    if (resolved.threshold.duration_short_ms == 0) {
        resolved.threshold.duration_short_ms = defaults.threshold.duration_short_ms;
    }
    if (resolved.threshold.speed_slow_px_per_ms <= 0) {
        resolved.threshold.speed_slow_px_per_ms = defaults.threshold.speed_slow_px_per_ms;
    }

    if ((resolved.detect_period_ms < BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MIN_MS) ||
            (resolved.detect_period_ms > BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MAX_MS)) {
        return std::unexpected(
                   (boost::format("Touch gesture detect period must be in range [%1%, %2%] ms") %
                    BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MIN_MS %
                    BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MAX_MS).str()
               );
    }
    if (resolved.release_debounce_ms > BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MAX_MS) {
        return std::unexpected(
                   (boost::format("Touch gesture release debounce must be <= %1% ms") %
                    BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MAX_MS).str()
               );
    }
    if ((resolved.threshold.direction_angle == 0) || (resolved.threshold.direction_angle >= 90)) {
        return std::unexpected("Touch gesture direction angle must be in range [1, 89]");
    }
    if ((resolved.threshold.direction_horizon > output.info.width) ||
            (resolved.threshold.horizontal_edge > output.info.width)) {
        return std::unexpected("Touch gesture horizontal thresholds exceed output width");
    }
    if ((resolved.threshold.direction_vertical > output.info.height) ||
            (resolved.threshold.vertical_edge > output.info.height)) {
        return std::unexpected("Touch gesture vertical thresholds exceed output height");
    }
    return resolved;
}

void Display::reset_touch_gesture_state_locked(OutputContext &output)
{
    output.gesture_info = TouchGestureInfo{};
    output.gesture_info.output_id = output.info.id;
    output.gesture_info.output_name = output.info.name;
    output.gesture_touch_start_time_ms = 0;
    output.gesture_release_start_time_ms = 0;
    output.gesture_active_track_id = 0;
    output.gesture_detection_started = false;
    output.gesture_has_active_track = false;
    output.gesture_direction_locked = false;
    output.gesture_release_pending = false;
}

bool Display::start_touch_gesture_task(uint32_t output_id)
{
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->is_running(), false, "Task scheduler is not running");

    lib_utils::TaskScheduler::TaskId old_task_id = 0;
    uint16_t detect_period_ms = 0;
    bool enabled = false;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(output_id);
        if (output_it == outputs_.end()) {
            return false;
        }
        old_task_id = output_it->second.gesture_task_id;
        output_it->second.gesture_task_id = 0;
        reset_touch_gesture_state_locked(output_it->second);
        enabled = output_it->second.gesture_config.enabled;
        detect_period_ms = output_it->second.gesture_config.detect_period_ms;
    }

    if (old_task_id != 0) {
        scheduler->cancel(old_task_id);
    }
    if (!enabled) {
        return true;
    }

    lib_utils::TaskScheduler::TaskId task_id = 0;
    auto gesture_task = [this, output_id]() -> bool {
        process_touch_gesture(output_id);
        return true;
    };
    const bool scheduled = scheduler->post_periodic(
                               std::move(gesture_task), static_cast<int>(detect_period_ms), &task_id,
                               get_touch_task_group()
                           );
    if (!scheduled) {
        return false;
    }

    bool stored = false;
    {
        std::lock_guard lock(mutex_);
        if (auto output_it = outputs_.find(output_id);
                (output_it != outputs_.end()) && output_it->second.gesture_config.enabled) {
            output_it->second.gesture_task_id = task_id;
            stored = true;
        }
    }
    if (!stored) {
        scheduler->cancel(task_id);
    }
    return stored;
}

void Display::stop_touch_gesture_task(uint32_t output_id)
{
    auto scheduler = get_task_scheduler();
    lib_utils::TaskScheduler::TaskId task_id = 0;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(output_id);
        if (output_it == outputs_.end()) {
            return;
        }
        task_id = output_it->second.gesture_task_id;
        output_it->second.gesture_task_id = 0;
        reset_touch_gesture_state_locked(output_it->second);
    }
    if ((task_id != 0) && scheduler) {
        scheduler->cancel(task_id);
    }
}

void Display::stop_touch_gesture_tasks()
{
    auto scheduler = get_task_scheduler();
    std::vector<lib_utils::TaskScheduler::TaskId> task_ids;
    {
        std::lock_guard lock(mutex_);
        for (auto &[_, output] : outputs_) {
            if (output.gesture_task_id != 0) {
                task_ids.push_back(output.gesture_task_id);
                output.gesture_task_id = 0;
            }
            reset_touch_gesture_state_locked(output);
        }
    }
    if (!scheduler) {
        return;
    }
    for (const auto task_id : task_ids) {
        scheduler->cancel(task_id);
    }
}

void Display::process_touch_gesture(uint32_t output_id)
{
    std::optional<TouchGestureInfo> event_info;
    {
        std::lock_guard lock(mutex_);
        auto output_it = outputs_.find(output_id);
        if ((output_it == outputs_.end()) || !output_it->second.gesture_config.enabled) {
            return;
        }
        auto &output = output_it->second;
        if (output.touch_id == INVALID_TOUCH_ID) {
            reset_touch_gesture_state_locked(output);
            return;
        }
        auto touch_it = touches_.find(output.touch_id);
        if (touch_it == touches_.end()) {
            reset_touch_gesture_state_locked(output);
            return;
        }

        const auto &touch = touch_it->second;
        const auto &snapshot = touch.snapshot;
        const auto selected_point = select_touch_gesture_point(output, snapshot);
        const bool touched = selected_point.has_value();
        const uint64_t current_time_ms = get_current_time_ms();
        bool just_pressed = false;

        if (touched) {
            const auto point = normalize_touch_gesture_point(selected_point.value(), output, touch.info);
            output.gesture_release_pending = false;
            output.gesture_info.stop_x = point.x;
            output.gesture_info.stop_y = point.y;
            output.gesture_info.stop_area = get_touch_gesture_area(output, point.x, point.y);

            if (!output.gesture_detection_started) {
                output.gesture_detection_started = true;
                output.gesture_touch_start_time_ms = current_time_ms;
                output.gesture_has_active_track = selected_point->track_id != 0;
                output.gesture_active_track_id = selected_point->track_id;
                output.gesture_direction_locked = false;
                output.gesture_info = TouchGestureInfo{
                    .output_id = output.info.id,
                    .output_name = output.info.name,
                    .touch_name = touch.info.name,
                    .event_type = TouchGestureEventType::Press,
                    .direction = TouchGestureDirection::None,
                    .start_area = get_touch_gesture_area(output, point.x, point.y),
                    .stop_area = get_touch_gesture_area(output, point.x, point.y),
                    .start_x = point.x,
                    .start_y = point.y,
                    .stop_x = point.x,
                    .stop_y = point.y,
                };
                event_info = output.gesture_info;
                just_pressed = true;
            }

            output.gesture_info.touch_name = touch.info.name;
        } else if (!output.gesture_detection_started) {
            return;
        } else {
            if (!output.gesture_release_pending && (output.gesture_config.release_debounce_ms > 0)) {
                output.gesture_release_pending = true;
                output.gesture_release_start_time_ms = current_time_ms;
                return;
            }
            if (output.gesture_release_pending &&
                    ((current_time_ms - output.gesture_release_start_time_ms) <
                     output.gesture_config.release_debounce_ms)) {
                return;
            }
        }

        if (just_pressed) {
            // The first sample only announces Press; motion metrics start on the next tick.
        } else {
            auto &info = output.gesture_info;
            const auto &config = output.gesture_config;
            info.duration_ms = static_cast<uint32_t>(current_time_ms - output.gesture_touch_start_time_ms);
            info.flags_short_duration = (info.duration_ms < static_cast<uint32_t>(
                                             config.threshold.duration_short_ms
                                         ));

            const int distance_x = info.stop_x - info.start_x;
            const int distance_y = info.stop_y - info.start_y;
            if ((distance_x != 0) || (distance_y != 0)) {
                info.distance_px = std::sqrt(
                                       static_cast<float>((distance_x * distance_x) + (distance_y * distance_y))
                                   );
                info.speed_px_per_ms = (info.duration_ms > 0) ?
                                       (info.distance_px / static_cast<float>(info.duration_ms)) :
                                       std::numeric_limits<float>::infinity();
                info.flags_slow_speed = (info.speed_px_per_ms < config.threshold.speed_slow_px_per_ms);

                TouchGestureDirection direction = TouchGestureDirection::None;
                const float distance_tan = (distance_x == 0) ?
                                           std::numeric_limits<float>::infinity() :
                                           std::abs(static_cast<float>(distance_y) / static_cast<float>(distance_x));
                if (distance_tan > output.gesture_direction_tan_threshold) {
                    if (distance_y > static_cast<int>(config.threshold.direction_vertical)) {
                        direction = TouchGestureDirection::Down;
                    } else if (distance_y < -static_cast<int>(config.threshold.direction_vertical)) {
                        direction = TouchGestureDirection::Up;
                    }
                } else {
                    if (distance_x > static_cast<int>(config.threshold.direction_horizon)) {
                        direction = TouchGestureDirection::Right;
                    } else if (distance_x < -static_cast<int>(config.threshold.direction_horizon)) {
                        direction = TouchGestureDirection::Left;
                    }
                }

                if (direction != TouchGestureDirection::None) {
                    if (!output.gesture_direction_locked || !config.direction_lock_enabled) {
                        info.direction = direction;
                        output.gesture_direction_locked = config.direction_lock_enabled;
                    }
                }
            }

            info.event_type = touched ? TouchGestureEventType::Pressing : TouchGestureEventType::Release;
            event_info = info;
            if (!touched) {
                reset_touch_gesture_state_locked(output);
            }
        }
    }

    if (event_info.has_value()) {
        emit_touch_gesture(event_info.value());
    }
}

uint8_t Display::get_touch_gesture_area(const OutputContext &output, int x, int y) const
{
    uint8_t area = to_area_mask(TouchGestureArea::Center);
    const auto &threshold = output.gesture_config.threshold;
    area |= (y < threshold.vertical_edge) ? to_area_mask(TouchGestureArea::TopEdge) : 0;
    area |= ((static_cast<int>(output.info.height) - y) < threshold.vertical_edge) ?
            to_area_mask(TouchGestureArea::BottomEdge) : 0;
    area |= (x < threshold.horizontal_edge) ? to_area_mask(TouchGestureArea::LeftEdge) : 0;
    area |= ((static_cast<int>(output.info.width) - x) < threshold.horizontal_edge) ?
            to_area_mask(TouchGestureArea::RightEdge) : 0;
    return area;
}

std::optional<Display::TouchPoint> Display::select_touch_gesture_point(
    const OutputContext &output, const TouchSnapshot &snapshot
) const
{
    if (!snapshot.valid || snapshot.points.empty()) {
        return std::nullopt;
    }
    if (output.gesture_detection_started && output.gesture_has_active_track) {
        auto point_it = std::find_if(snapshot.points.begin(), snapshot.points.end(), [&](const auto & point) {
            return point.track_id == output.gesture_active_track_id;
        });
        if (point_it != snapshot.points.end()) {
            return *point_it;
        }
        return std::nullopt;
    }
    return snapshot.points.front();
}

Display::TouchPoint Display::normalize_touch_gesture_point(
    const TouchPoint &point, const OutputContext &output, const TouchInfo &touch
) const
{
    auto normalize_axis = [](int value, uint32_t src_max, uint32_t dst_size) -> int16_t {
        if (dst_size == 0)
        {
            return 0;
        }
        if (src_max == 0)
        {
            return static_cast<int16_t>(std::clamp(value, 0, static_cast<int>(dst_size - 1)));
        }
        const int normalized = static_cast<int>((static_cast<int64_t>(value) * dst_size) / src_max);
        return static_cast<int16_t>(std::clamp(normalized, 0, static_cast<int>(dst_size - 1)));
    };

    return TouchPoint{
        .x = normalize_axis(point.x, touch.x_max, output.info.width),
        .y = normalize_axis(point.y, touch.y_max, output.info.height),
        .pressure = point.pressure,
        .track_id = point.track_id,
    };
}

uint32_t Display::allocate_frame_id_locked()
{
    const uint32_t frame_id = next_frame_id_++;
    if (next_frame_id_ == INVALID_SOURCE_ID) {
        next_frame_id_ = 1;
    }
    return frame_id;
}

bool Display::is_frame_valid_for_output(const FrameInfo &frame, const OutputContext &output, size_t data_size) const
{
    if ((frame.width == 0) || (frame.height == 0)) {
        return false;
    }
    if (frame.pixel_format != output.info.pixel_format) {
        return false;
    }
    if ((frame.x > output.info.width) || (frame.y > output.info.height)) {
        return false;
    }
    if ((frame.width > output.info.width - frame.x) || (frame.height > output.info.height - frame.y)) {
        return false;
    }

    const size_t bpp = bytes_per_pixel(frame.pixel_format);
    if (bpp == 0) {
        return false;
    }
    const size_t expected_size = static_cast<size_t>(frame.width) * frame.height * bpp;
    return data_size == expected_size;
}

bool Display::is_buffer_output_valid(const BufferOutputConfig &config, size_t bpp) const
{
    if ((config.width == 0) || (config.height == 0) || (bpp == 0)) {
        return false;
    }
    auto *buffer_ptr = config.buffer.to_ptr<uint8_t>();
    if (buffer_ptr == nullptr) {
        return false;
    }
    const size_t row_bytes = static_cast<size_t>(config.width) * bpp;
    const size_t stride_bytes = (config.stride_bytes == 0) ? row_bytes : config.stride_bytes;
    if (stride_bytes < row_bytes) {
        return false;
    }
    if (config.height > 1) {
        const size_t max_size = std::numeric_limits<size_t>::max();
        if (stride_bytes > (max_size - row_bytes) / (config.height - 1)) {
            return false;
        }
    }
    const size_t required_size = (config.height > 0) ? (stride_bytes * (config.height - 1) + row_bytes) : 0;
    return config.buffer.data_size >= required_size;
}

Display::PresentResult Display::present_frame_to_output(
    const OutputDrawTarget &target, const FrameInfo &frame, const RawBuffer &data, uint32_t timeout_ms
) const
{
    switch (target.info.slot) {
    case OutputSlot::HalPanel: {
        if (!target.panel) {
            return PresentResult::Error;
        }
        const bool presented = target.panel->draw_bitmap_sync(
                                   frame.x, frame.y, frame.x + frame.width, frame.y + frame.height, data.data_ptr,
                                   timeout_ms
                               );
        return presented ? PresentResult::Presented : PresentResult::Error;
    }
    case OutputSlot::Buffer: {
        auto *target_data = target.buffer.buffer.to_ptr<uint8_t>();
        if ((target_data == nullptr) || (target.buffer.stride_bytes == 0)) {
            return PresentResult::Error;
        }
        const size_t bpp = bytes_per_pixel(frame.pixel_format);
        if (bpp == 0) {
            return PresentResult::Error;
        }
        const size_t row_bytes = static_cast<size_t>(frame.width) * bpp;
        const size_t dst_x_offset = static_cast<size_t>(frame.x) * bpp;
        for (uint32_t row = 0; row < frame.height; ++row) {
            const size_t src_offset = static_cast<size_t>(row) * row_bytes;
            const size_t dst_offset =
                (static_cast<size_t>(frame.y) + row) * target.buffer.stride_bytes + dst_x_offset;
            std::memcpy(target_data + dst_offset, data.data_ptr + src_offset, row_bytes);
        }
        return PresentResult::Presented;
    }
    default:
        return PresentResult::Error;
    }
}

size_t Display::bytes_per_pixel(PixelFormat pixel_format) const
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

bool Display::schedule_render_output_locked(uint32_t output_id)
{
    auto output_it = outputs_.find(output_id);
    if (output_it == outputs_.end()) {
        return false;
    }
    auto &output = output_it->second;
    if (output.render_scheduled) {
        return true;
    }

    auto scheduler = get_task_scheduler();
    if ((scheduler == nullptr) || !scheduler->is_running()) {
        return false;
    }

    const bool scheduled = scheduler->post([this, output_id]() {
        render_output(output_id);
    }, nullptr, get_render_task_group());
    output.render_scheduled = scheduled;
    return scheduled;
}

void Display::render_output(uint32_t output_id)
{
    while (true) {
        AsyncFrame frame;
        OutputDrawTarget target;
        std::shared_ptr<std::mutex> draw_mutex;
        bool has_frame = false;

        {
            std::lock_guard lock(mutex_);
            auto output_it = outputs_.find(output_id);
            if (output_it == outputs_.end()) {
                return;
            }
            auto &output = output_it->second;
            if (!output.pending_frame.has_value()) {
                output.render_scheduled = false;
                return;
            }

            frame = std::move(output.pending_frame.value());
            output.pending_frame.reset();
            output.inflight_frame_id = frame.frame_id;
            draw_mutex = output.draw_mutex;
            has_frame = true;
        }

        if (!has_frame) {
            continue;
        }

        PresentResult result = PresentResult::Error;
        if (draw_mutex == nullptr) {
            result = PresentResult::Error;
        } else {
            std::lock_guard draw_lock(*draw_mutex);
            bool is_active = false;
            bool output_exists = false;
            {
                std::lock_guard lock(mutex_);
                auto output_it = outputs_.find(output_id);
                output_exists = output_it != outputs_.end();
                is_active = output_exists && (output_it->second.active_source_id == frame.source_id);
                if (is_active) {
                    const auto &output = output_it->second;
                    target = OutputDrawTarget{
                        .info = output.info,
                        .panel = output.panel.get(),
                        .buffer = output.buffer,
                    };
                }
            }

            if (!output_exists) {
                result = PresentResult::Error;
            } else if (!is_active) {
                result = PresentResult::DroppedNotActive;
            } else {
                result = present_frame_to_output(target, frame.frame, frame.data, frame.timeout_ms);
            }
        }

        {
            std::lock_guard lock(mutex_);
            auto output_it = outputs_.find(output_id);
            if ((output_it != outputs_.end()) && (output_it->second.inflight_frame_id == frame.frame_id)) {
                output_it->second.inflight_frame_id = 0;
            }
        }
        if (result == PresentResult::Presented) {
            emit_frame_presented(frame.output_name, frame.frame);
        }
        complete_async_frame(std::move(frame), result);
    }
}

void Display::drop_pending_frames_locked(std::vector<AsyncFrame> &dropped_frames)
{
    for (auto &[_, output] : outputs_) {
        if (!output.pending_frame.has_value()) {
            continue;
        }
        dropped_frames.push_back(std::move(output.pending_frame.value()));
        output.pending_frame.reset();
        if (output.inflight_frame_id == 0) {
            output.render_scheduled = false;
        }
    }
}

void Display::complete_async_frame(AsyncFrame frame, PresentResult result)
{
    if (frame.on_complete != nullptr) {
        frame.on_complete(frame.frame_id, result);
    }
}

void Display::bind_default_touches_locked()
{
    for (auto &[_, output] : outputs_) {
        output.touch_id = INVALID_TOUCH_ID;
    }

    if (touches_.empty()) {
        refresh_touch_bound_outputs_locked();
        return;
    }

    std::set<uint32_t> bound_touch_ids;
    for (auto &[output_id, output] : outputs_) {
        if (output.info.group_id.empty()) {
            continue;
        }

        std::vector<uint32_t> matched_touch_ids;
        for (const auto &[touch_id, touch] : touches_) {
            if (touch.info.group_id == output.info.group_id) {
                matched_touch_ids.push_back(touch_id);
            }
        }
        if (matched_touch_ids.empty()) {
            continue;
        }
        if (matched_touch_ids.size() > 1) {
            BROOKESIA_LOGW(
                "Display output %1% group_id(%2%) matches %3% touch devices; leave unbound",
                output.info.name, output.info.group_id, matched_touch_ids.size()
            );
            continue;
        }
        output.touch_id = matched_touch_ids.front();
        bound_touch_ids.insert(output.touch_id);
        BROOKESIA_LOGI(
            "Bound display output %1% to touch %2% by group_id(%3%)", output.info.name,
            touches_.at(output.touch_id).info.name, output.info.group_id
        );
    }

    std::vector<uint32_t> ungrouped_touch_ids;
    for (const auto &[touch_id, touch] : touches_) {
        if (!touch.info.group_id.empty() || (bound_touch_ids.find(touch_id) != bound_touch_ids.end())) {
            continue;
        }
        ungrouped_touch_ids.push_back(touch_id);
    }

    const bool share_single_touch = ungrouped_touch_ids.size() == 1;
    const uint32_t single_touch_id = share_single_touch ? ungrouped_touch_ids.front() : INVALID_TOUCH_ID;
    for (auto &[output_id, output] : outputs_) {
        if (output.touch_id != INVALID_TOUCH_ID) {
            continue;
        }
        if (!output.info.group_id.empty()) {
            continue;
        }
        if (share_single_touch) {
            output.touch_id = single_touch_id;
            BROOKESIA_LOGI(
                "Bound display output %1% to touch %2% by single-touch fallback", output.info.name,
                touches_.at(output.touch_id).info.name
            );
            continue;
        }
        auto touch_it = touches_.find(output_id);
        if ((touch_it != touches_.end()) && touch_it->second.info.group_id.empty()) {
            output.touch_id = output_id;
            BROOKESIA_LOGI(
                "Bound display output %1% to touch %2% by legacy id fallback", output.info.name,
                touch_it->second.info.name
            );
        }
    }
    refresh_touch_bound_outputs_locked();
}

Display::OutputTouchCapability Display::make_output_touch_capability_locked(const TouchContext &touch) const
{
    return OutputTouchCapability{
        .id = touch.info.id,
        .name = touch.info.name,
        .instance = touch.info.touch_instance,
        .max_points = touch.info.max_points,
        .operation_mode = touch.info.operation_mode,
    };
}

Display::OutputBacklightCapability Display::make_output_backlight_capability(const OutputContext &output) const
{
    return OutputBacklightCapability{
        .instance = output.backlight ? output.backlight.instance_name() : std::string(),
        .on_off_supported = output.backlight ? output.backlight->is_light_on_off_supported() : false,
    };
}

void Display::refresh_output_capability_locked(OutputContext &output)
{
    output.info.touch.reset();
    if (output.touch_id != INVALID_TOUCH_ID) {
        auto touch_it = touches_.find(output.touch_id);
        if (touch_it != touches_.end()) {
            output.info.touch = make_output_touch_capability_locked(touch_it->second);
        }
    }

    output.info.backlight.reset();
    if (output.backlight) {
        output.info.backlight = make_output_backlight_capability(output);
    }
}

void Display::refresh_touch_bound_outputs_locked()
{
    for (auto &[_, touch] : touches_) {
        touch.info.bound_outputs.clear();
    }
    for (const auto &[_, output] : outputs_) {
        if (output.touch_id == INVALID_TOUCH_ID) {
            continue;
        }
        auto touch_it = touches_.find(output.touch_id);
        if (touch_it != touches_.end()) {
            touch_it->second.info.bound_outputs.push_back(output.info.name);
        }
    }
    for (auto &[_, output] : outputs_) {
        refresh_output_capability_locked(output);
    }
}

bool Display::start_touch_tasks()
{
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    std::vector<uint32_t> touch_ids;
    {
        std::lock_guard lock(mutex_);
        touch_ids.reserve(touches_.size());
        for (const auto &[touch_id, _] : touches_) {
            touch_ids.push_back(touch_id);
        }
    }

    bool all_started = true;
    for (const auto touch_id : touch_ids) {
        all_started = start_touch_task(touch_id) && all_started;
    }

    return all_started;
}

bool Display::start_touch_task(uint32_t touch_id)
{
    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is not available");

    std::shared_ptr<hal::display::TouchIface> touch_handle;
    TouchInfo touch_info;
    std::shared_ptr<TouchInterruptBridge> old_interrupt_bridge;
    lib_utils::TaskScheduler::TaskId old_poll_task_id = 0;
    {
        std::lock_guard lock(mutex_);
        auto touch_it = touches_.find(touch_id);
        if (touch_it == touches_.end()) {
            return false;
        }
        touch_handle = touch_it->second.touch.get();
        touch_info = touch_it->second.info;
        old_interrupt_bridge = std::move(touch_it->second.interrupt_bridge);
        old_poll_task_id = touch_it->second.poll_task_id;
        touch_it->second.poll_task_id = 0;
        touch_it->second.read_scheduled = false;
    }
    if (!touch_handle) {
        return false;
    }

    if (old_poll_task_id != 0) {
        scheduler->cancel(old_poll_task_id);
    }
    if (old_interrupt_bridge) {
        (void)touch_handle->register_interrupt_handler(nullptr, nullptr);
        old_interrupt_bridge->stop();
    }

    if (touch_info.operation_mode == hal::display::TouchIface::OperationMode::Interrupt) {
        auto interrupt_bridge = std::make_shared<TouchInterruptBridge>();
        if (!interrupt_bridge->start(this, touch_id, touch_handle)) {
            BROOKESIA_LOGW("Failed to register Display touch interrupt handler for %1%", touch_info.name);
            return false;
        }
        bool stored = false;
        {
            std::lock_guard lock(mutex_);
            if (auto touch_it = touches_.find(touch_id); touch_it != touches_.end()) {
                touch_it->second.interrupt_bridge = interrupt_bridge;
                stored = true;
            }
        }
        if (!stored) {
            (void)touch_handle->register_interrupt_handler(nullptr, nullptr);
            interrupt_bridge->stop();
            return false;
        }
        (void)schedule_touch_read(touch_id);
        return true;
    }

    lib_utils::TaskScheduler::TaskId poll_task_id = 0;
    auto poll_task = [this, touch_id]() -> bool {
        read_touch(touch_id);
        return true;
    };
    const bool scheduled = scheduler->post_periodic(
                               std::move(poll_task),
                               static_cast<int>(BROOKESIA_SERVICE_DISPLAY_TOUCH_POLL_INTERVAL_MS), &poll_task_id,
                               get_touch_task_group()
                           );
    if (!scheduled) {
        BROOKESIA_LOGW("Failed to schedule Display touch polling for %1%", touch_info.name);
        return false;
    }
    {
        std::lock_guard lock(mutex_);
        if (auto touch_it = touches_.find(touch_id); touch_it != touches_.end()) {
            touch_it->second.poll_task_id = poll_task_id;
        }
    }
    (void)schedule_touch_read(touch_id);
    return true;
}

void Display::stop_touch_tasks()
{
    auto scheduler = get_task_scheduler();
    std::vector<std::pair<std::shared_ptr<hal::display::TouchIface>, std::shared_ptr<TouchInterruptBridge>>>
    interrupt_bridges;
    std::vector<lib_utils::TaskScheduler::TaskId> poll_task_ids;

    {
        std::lock_guard lock(mutex_);
        for (auto &[_, touch] : touches_) {
            touch.read_scheduled = false;
            if (touch.info.operation_mode == hal::display::TouchIface::OperationMode::Interrupt) {
                interrupt_bridges.emplace_back(touch.touch.get(), std::move(touch.interrupt_bridge));
            }
            if (touch.poll_task_id != 0) {
                poll_task_ids.push_back(touch.poll_task_id);
                touch.poll_task_id = 0;
            }
        }
    }

    for (auto &[touch, bridge] : interrupt_bridges) {
        if (touch) {
            (void)touch->register_interrupt_handler(nullptr, nullptr);
        }
        if (bridge) {
            bridge->stop();
        }
    }
    if (scheduler) {
        for (const auto task_id : poll_task_ids) {
            scheduler->cancel(task_id);
        }
        if (scheduler->is_running()) {
            scheduler->cancel_group(get_touch_task_group());
            (void)scheduler->wait_group(get_touch_task_group(), 200);
        }
    }
}

bool Display::schedule_touch_read(uint32_t touch_id)
{
    auto scheduler = get_task_scheduler();
    if ((scheduler == nullptr) || !scheduler->is_running()) {
        return false;
    }

    {
        std::lock_guard lock(mutex_);
        auto touch_it = touches_.find(touch_id);
        if (touch_it == touches_.end()) {
            return false;
        }
        if (touch_it->second.read_scheduled) {
            return true;
        }
        touch_it->second.read_scheduled = true;
    }

    const bool scheduled = scheduler->post([this, touch_id]() {
        read_touch(touch_id);
    }, nullptr, get_touch_task_group());
    if (!scheduled) {
        std::lock_guard lock(mutex_);
        if (auto touch_it = touches_.find(touch_id); touch_it != touches_.end()) {
            touch_it->second.read_scheduled = false;
        }
    }
    return scheduled;
}

void Display::read_touch(uint32_t touch_id)
{
    std::shared_ptr<hal::display::TouchIface> touch_handle;
    std::vector<std::string> output_names;
    {
        std::lock_guard lock(mutex_);
        auto touch_it = touches_.find(touch_id);
        if (touch_it == touches_.end()) {
            return;
        }
        touch_handle = touch_it->second.touch.get();
        output_names = touch_it->second.info.bound_outputs;
    }

    std::vector<TouchPoint> points;
    bool read_success = false;
    if (touch_handle) {
        read_success = touch_handle->read_points(points);
    }

    TouchSnapshot snapshot;
    {
        std::lock_guard lock(mutex_);
        auto touch_it = touches_.find(touch_id);
        if (touch_it == touches_.end()) {
            return;
        }
        touch_it->second.read_scheduled = false;
        if (!read_success) {
            BROOKESIA_LOGW("Failed to read Display touch %1%", touch_it->second.info.name);
            return;
        }

        auto &cached_snapshot = touch_it->second.snapshot;
        cached_snapshot.points = std::move(points);
        cached_snapshot.sequence++;
        if (cached_snapshot.sequence == 0) {
            cached_snapshot.sequence = 1;
        }
        cached_snapshot.updated_at_ms = get_current_time_ms();
        cached_snapshot.valid = true;
        snapshot = cached_snapshot;
        output_names = touch_it->second.info.bound_outputs;
    }

    emit_touch_updated(output_names, snapshot);
}

void Display::emit_touch_updated(const std::vector<std::string> &output_names, const TouchSnapshot &snapshot)
{
    for (const auto &output_name : output_names) {
        touch_updated_signal_(output_name, snapshot);
    }
}

void Display::emit_touch_gesture(const TouchGestureInfo &info)
{
    touch_gesture_signal_(info.output_name, info);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::TouchGesture),
        std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_JSON(info).as_object())}
    );
}

void Display::emit_output_registered(const OutputInfo &info)
{
    output_registered_signal_(info);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::OutputRegistered),
        std::vector<EventItem> {EventItem(BROOKESIA_DESCRIBE_TO_JSON(info).as_object())}
    );
}

void Display::emit_output_unregistered(const std::string &output_name)
{
    output_unregistered_signal_(output_name);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::OutputUnregistered),
        std::vector<EventItem> {output_name}
    );
}

void Display::emit_frame_presented(const std::string &output_name, const FrameInfo &frame)
{
    frame_presented_signal_(output_name, frame);
}

void Display::emit_source_state_changed(const std::string &source_name, const std::string &output_name, SourceState state)
{
    source_state_changed_signal_(source_name, output_name, state);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::SourceStateChanged),
    std::vector<EventItem> {
        source_name,
        output_name,
        BROOKESIA_DESCRIBE_ENUM_TO_STR(state),
    }
    );
}

void Display::emit_active_source_changed(const std::string &output_name, const std::string &source_name)
{
    active_source_changed_signal_(output_name, source_name);
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::ActiveSourceChanged),
        std::vector<EventItem> {output_name, source_name}
    );
}

#if BROOKESIA_SERVICE_DISPLAY_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Display, Display::Helper::get_name().data(), Display::get_instance(),
    BROOKESIA_SERVICE_DISPLAY_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
