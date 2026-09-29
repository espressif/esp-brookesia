/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <atomic>

#include "boost/format.hpp"
#include "private/display_impl.hpp"

namespace esp_brookesia::service {

namespace {

std::atomic<Display::TouchCollectionOwner> &touch_collection_owner_storage()
{
    static std::atomic<Display::TouchCollectionOwner> owner{Display::TouchCollectionOwner::Service};
    return owner;
}

bool service_owns_touch_input()
{
    return (BROOKESIA_SERVICE_DISPLAY_ENABLE_TOUCH_COLLECTION != 0) &&
           (touch_collection_owner_storage().load(std::memory_order_acquire) == Display::TouchCollectionOwner::Service);
}

} // namespace

void Display::set_touch_collection_owner(TouchCollectionOwner owner)
{
    touch_collection_owner_storage().store(owner, std::memory_order_release);
    auto &display = get_instance();
    if (!display.is_running()) {
        BROOKESIA_LOGI(
            "Display touch collection owner is %1%",
            owner == TouchCollectionOwner::Service ? "service" : "external"
        );
        return;
    }
    if (owner == TouchCollectionOwner::External) {
        display.stop_touch_gesture_tasks();
        display.stop_touch_tasks();
        BROOKESIA_LOGI("Display released touch input to an external reader");
        return;
    }
    BROOKESIA_LOGI("Display touch collection owner is service");
}

Display::TouchCollectionOwner Display::touch_collection_owner()
{
    return touch_collection_owner_storage().load(std::memory_order_acquire);
}

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

    std::unique_lock lock(mutex_);
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

    lock.unlock();
    auto provider_registration = register_display_dataflow_provider(*this);
    if (!provider_registration) {
        BROOKESIA_LOGE("Failed to register Display DataFlow provider: %1%", provider_registration.error());
        return false;
    }
    dataflow_provider_registration_ = std::move(provider_registration.value());
    BROOKESIA_LOGI("Registered Display DataFlow provider");

    return true;
}


void Display::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    dataflow_provider_registration_.release();
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
    if (service_owns_touch_input()) {
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
    } else {
        BROOKESIA_LOGI("Display touch collection is disabled; an external reader owns the touch device");
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

    ServiceManager::get_instance().get_dataflow_registry().invalidate_provider_operations("Display");
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
}
