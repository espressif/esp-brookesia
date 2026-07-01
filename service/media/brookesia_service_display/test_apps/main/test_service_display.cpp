/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "boost/json/value.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_manager.hpp"
#include "mock_display_device.hpp"

using namespace esp_brookesia;

namespace {

using DisplayHelper = service::helper::Display;
using DisplayService = service::Display;
using OutputSlot = DisplayService::OutputSlot;
using PixelFormat = DisplayService::PixelFormat;
using PresentResult = DisplayService::PresentResult;
using PresentSubmitState = DisplayService::PresentSubmitState;
using SourceState = DisplayService::SourceState;
using TouchGestureConfig = DisplayService::TouchGestureConfig;
using TouchGestureDirection = DisplayService::TouchGestureDirection;
using TouchGestureEventType = DisplayService::TouchGestureEventType;
using TouchGestureInfo = DisplayService::TouchGestureInfo;
using TouchOperationMode = DisplayHelper::TouchOperationMode;
using SourceStateMonitor = DisplayHelper::EventMonitor<DisplayHelper::EventId::SourceStateChanged>;
using ActiveSourceMonitor = DisplayHelper::EventMonitor<DisplayHelper::EventId::ActiveSourceChanged>;

constexpr uint32_t EVENT_TIMEOUT_MS = 1000;
constexpr uint32_t DRAW_TIMEOUT_MS = 1000;

auto &service_manager = service::ServiceManager::get_instance();
service::ServiceBinding display_binding;

bool startup()
{
    esp_brookesia::test_apps::service_display::MockDisplayDevice::reset_draw_records();
    if (!service_manager.start()) {
        return false;
    }
    display_binding = service_manager.bind(DisplayHelper::get_name().data());
    return display_binding.is_valid();
}

void shutdown()
{
    display_binding.release();
    service_manager.deinit();
    esp_brookesia::test_apps::service_display::MockDisplayDevice::reset_draw_records();
}

std::vector<DisplayService::OutputInfo> fetch_outputs()
{
    auto outputs = DisplayService::get_instance().get_outputs();
    TEST_ASSERT_FALSE(outputs.empty());
    return outputs;
}

uint32_t register_source(
    const std::string &name, const std::string &role, std::vector<std::string> preferred_outputs = {}
)
{
    DisplayService::SourceInfo source = {
        .name = name,
        .role = role,
        .preferred_outputs = std::move(preferred_outputs),
        .priority = 0,
    };
    auto result = DisplayService::get_instance().register_source(std::move(source));
    TEST_ASSERT_TRUE(result.has_value());
    TEST_ASSERT_TRUE(result.value() > 0);
    return result.value();
}

std::vector<uint8_t> make_rgb565_frame(uint32_t width, uint32_t height, uint16_t color)
{
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 2);
    for (size_t i = 0; i < data.size(); i += 2) {
        data[i] = static_cast<uint8_t>(color & 0xFF);
        data[i + 1] = static_cast<uint8_t>((color >> 8) & 0xFF);
    }
    return data;
}

uint16_t read_rgb565_pixel(const std::vector<uint8_t> &data, size_t stride_bytes, uint32_t x, uint32_t y)
{
    const size_t offset = static_cast<size_t>(y) * stride_bytes + static_cast<size_t>(x) * 2;
    return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
}

bool wait_for_draw_count(size_t expected_count, uint32_t timeout_ms)
{
    const uint32_t step_ms = 10;
    for (uint32_t waited_ms = 0; waited_ms <= timeout_ms; waited_ms += step_ms) {
        auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
        if (records.size() >= expected_count) {
            return true;
        }
        lib_utils::test_adapter::delay_ms(step_ms);
    }
    return false;
}

bool wait_for_sync_draw_enter_count(size_t expected_count, uint32_t timeout_ms)
{
    return esp_brookesia::test_apps::service_display::MockDisplayDevice::wait_for_sync_draw_enter_count(
               expected_count, timeout_ms
           );
}

std::string source_state_to_string(SourceState state)
{
    return BROOKESIA_DESCRIBE_ENUM_TO_STR(state);
}

template <typename T>
T json_array_to(const boost::json::array &json_array)
{
    T value;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(json_array), value));
    return value;
}

class AsyncCompletionCollector {
public:
    void complete(uint32_t frame_id, PresentResult result)
    {
        {
            std::lock_guard lock(mutex_);
            completions_.push_back({frame_id, result});
        }
        cv_.notify_all();
    }

    bool wait_for(uint32_t frame_id, PresentResult result, uint32_t timeout_ms)
    {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() {
            return std::find(completions_.begin(), completions_.end(), Completion{frame_id, result}) !=
                   completions_.end();
        });
    }

    size_t count() const
    {
        std::lock_guard lock(mutex_);
        return completions_.size();
    }

private:
    struct Completion {
        uint32_t frame_id = 0;
        PresentResult result = PresentResult::Error;

        bool operator==(const Completion &other) const
        {
            return (frame_id == other.frame_id) && (result == other.result);
        }
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Completion> completions_;
};

class TouchUpdateCollector {
public:
    void update(const std::string &output_name, const DisplayService::TouchSnapshot &snapshot)
    {
        {
            std::lock_guard lock(mutex_);
            updates_.push_back({output_name, snapshot});
        }
        cv_.notify_all();
    }

    bool wait_for_points(
        const std::string &output_name, size_t expected_count, uint8_t expected_track_id, uint32_t timeout_ms
    )
    {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() {
            return std::any_of(updates_.begin(), updates_.end(), [&](const auto & update) {
                return (update.output_name == output_name) && update.snapshot.valid &&
                       (update.snapshot.points.size() == expected_count) &&
                       (!update.snapshot.points.empty()) &&
                       (update.snapshot.points[0].track_id == expected_track_id);
            });
        });
    }

private:
    struct Update {
        std::string output_name;
        DisplayService::TouchSnapshot snapshot;
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Update> updates_;
};

class TouchGestureCollector {
public:
    void update(const std::string &output_name, const TouchGestureInfo &info)
    {
        {
            std::lock_guard lock(mutex_);
            updates_.push_back({output_name, info});
        }
        cv_.notify_all();
    }

    bool wait_for(
        const std::string &output_name, TouchGestureEventType event_type, TouchGestureDirection direction,
        uint32_t timeout_ms
    )
    {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() {
            return std::any_of(updates_.begin(), updates_.end(), [&](const auto & update) {
                return (update.output_name == output_name) && (update.info.event_type == event_type) &&
                       (update.info.direction == direction);
            });
        });
    }

    bool wait_for_stop_x(const std::string &output_name, int stop_x, uint32_t timeout_ms)
    {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() {
            return std::any_of(updates_.begin(), updates_.end(), [&](const auto & update) {
                return (update.output_name == output_name) &&
                       (update.info.event_type == TouchGestureEventType::Pressing) &&
                       (update.info.stop_x == stop_x);
            });
        });
    }

    size_t count(TouchGestureEventType event_type) const
    {
        std::lock_guard lock(mutex_);
        return static_cast<size_t>(std::count_if(updates_.begin(), updates_.end(), [&](const auto & update) {
            return update.info.event_type == event_type;
        }));
    }

private:
    struct Update {
        std::string output_name;
        TouchGestureInfo info;
    };

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Update> updates_;
};

TouchGestureConfig make_test_gesture_config()
{
    TouchGestureConfig config;
    config.enabled = true;
    config.detect_period_ms = 10;
    config.direction_lock_enabled = true;
    config.release_debounce_ms = 20;
    config.threshold.direction_horizon = 30;
    config.threshold.direction_vertical = 30;
    config.threshold.direction_angle = 45;
    config.threshold.horizontal_edge = 20;
    config.threshold.vertical_edge = 20;
    config.threshold.duration_short_ms = 220;
    config.threshold.speed_slow_px_per_ms = 0.1F;
    return config;
}

} // namespace

BROOKESIA_TEST_CASE(get_outputs, "Test ServiceDisplay - get outputs", "[service][display][outputs]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    TEST_ASSERT_EQUAL_size_t(2, outputs.size());
    TEST_ASSERT_EQUAL_STRING("Output0", outputs[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Output1", outputs[1].name.c_str());
    for (const auto &output : outputs) {
        TEST_ASSERT_EQUAL_UINT32(240, output.width);
        TEST_ASSERT_EQUAL_UINT32(240, output.height);
        TEST_ASSERT_TRUE(output.pixel_format == PixelFormat::RGB565);
        TEST_ASSERT_TRUE(output.slot == OutputSlot::HalPanel);
        TEST_ASSERT_FALSE(output.panel_instance.empty());
        TEST_ASSERT_FALSE(output.group_id.empty());
        TEST_ASSERT_TRUE(output.touch.has_value());
        TEST_ASSERT_TRUE(output.backlight.has_value());
        TEST_ASSERT_FALSE(output.touch->instance.empty());
        TEST_ASSERT_FALSE(output.backlight->instance.empty());
        TEST_ASSERT_EQUAL_UINT32(5, output.touch->max_points);
    }

    auto rpc_outputs_json = DisplayHelper::call_function_sync<boost::json::array>(
                                DisplayHelper::FunctionId::GetOutputs
                            );
    TEST_ASSERT_TRUE(rpc_outputs_json.has_value());
    auto rpc_outputs = json_array_to<std::vector<DisplayHelper::OutputInfo>>(rpc_outputs_json.value());
    TEST_ASSERT_EQUAL_size_t(outputs.size(), rpc_outputs.size());
    TEST_ASSERT_EQUAL_STRING(outputs[0].name.c_str(), rpc_outputs[0].name.c_str());
    TEST_ASSERT_EQUAL_UINT32(outputs[0].width, rpc_outputs[0].width);
    TEST_ASSERT_EQUAL_UINT32(outputs[0].height, rpc_outputs[0].height);
    TEST_ASSERT_TRUE(rpc_outputs[0].slot == DisplayHelper::OutputSlot::HalPanel);
    TEST_ASSERT_TRUE(rpc_outputs[0].touch.has_value());
    TEST_ASSERT_TRUE(rpc_outputs[0].backlight.has_value());
}

BROOKESIA_TEST_CASE(
    output_backlight_control,
    "Test ServiceDisplay - output backlight control",
    "[service][display][backlight]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    TEST_ASSERT_TRUE(outputs.size() >= 2);
    TEST_ASSERT_EQUAL_STRING("Output0", outputs[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("Output1", outputs[1].name.c_str());

    constexpr double output0_brightness = 60.0;
    auto set_result = DisplayHelper::call_function_sync(
                          DisplayHelper::FunctionId::SetBacklightBrightness,
                          static_cast<double>(outputs[0].id), output0_brightness
                      );
    TEST_ASSERT_TRUE_MESSAGE(set_result, "Failed to set Output0 backlight brightness");

    auto get_result = DisplayHelper::call_function_sync<double>(
                          DisplayHelper::FunctionId::GetBacklightBrightness, static_cast<double>(outputs[0].id)
                      );
    TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), "Failed to get Output0 backlight brightness");
    TEST_ASSERT_EQUAL_DOUBLE(output0_brightness, get_result.value());

    auto on_result = DisplayHelper::call_function_sync(
                         DisplayHelper::FunctionId::SetBacklightOnOff, static_cast<double>(outputs[0].id), true
                     );
    TEST_ASSERT_TRUE_MESSAGE(on_result, "Failed to turn on Output0 backlight");
    TEST_ASSERT_TRUE(esp_brookesia::test_apps::service_display::MockDisplayDevice::get_backlight_on(0));
    const auto expected_output0_hw = static_cast<uint8_t>(
                                         BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN +
                                         static_cast<int>(output0_brightness) *
                                         (BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX -
                                          BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN) / 100
                                     );
    TEST_ASSERT_EQUAL_UINT8(
        expected_output0_hw,
        esp_brookesia::test_apps::service_display::MockDisplayDevice::get_backlight_brightness(0)
    );

    constexpr double output1_brightness = 35.0;
    set_result = DisplayHelper::call_function_sync(
                     DisplayHelper::FunctionId::SetBacklightBrightness,
                     static_cast<double>(outputs[1].id), output1_brightness
                 );
    TEST_ASSERT_TRUE_MESSAGE(set_result, "Failed to set Output1 backlight brightness");
    get_result = DisplayHelper::call_function_sync<double>(
                     DisplayHelper::FunctionId::GetBacklightBrightness, static_cast<double>(outputs[1].id)
                 );
    TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), "Failed to get Output1 backlight brightness");
    TEST_ASSERT_EQUAL_DOUBLE(output1_brightness, get_result.value());

    std::vector<uint8_t> buffer_data(8 * 8 * 2);
    auto buffer_result = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "BacklightlessBuffer",
        .width = 8,
        .height = 8,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(buffer_data.data(), buffer_data.size()),
    });
    TEST_ASSERT_TRUE(buffer_result.has_value());
    lib_utils::FunctionGuard unregister_guard([]() {
        (void)DisplayService::get_instance().unregister_output("BacklightlessBuffer");
    });
    auto no_backlight_result = DisplayHelper::call_function_sync<double>(
                                   DisplayHelper::FunctionId::GetBacklightBrightness,
                                   static_cast<double>(buffer_result.value())
                               );
    TEST_ASSERT_FALSE_MESSAGE(no_backlight_result.has_value(), "Buffer output should not have backlight");

    auto reset_result = DisplayHelper::call_function_sync(
                            DisplayHelper::FunctionId::ResetData, static_cast<double>(outputs[0].id)
                        );
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "Failed to reset Output0 backlight data");
    get_result = DisplayHelper::call_function_sync<double>(
                     DisplayHelper::FunctionId::GetBacklightBrightness, static_cast<double>(outputs[0].id)
                 );
    TEST_ASSERT_TRUE_MESSAGE(get_result.has_value(), "Failed to get Output0 backlight brightness after reset");
    TEST_ASSERT_EQUAL_DOUBLE(BROOKESIA_SERVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_DEFAULT, get_result.value());
}

BROOKESIA_TEST_CASE(get_sources, "Test ServiceDisplay - get sources", "[service][display][sources]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("diagnostic", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().request_output(source_id, output0).has_value());

    auto rpc_sources_json = DisplayHelper::call_function_sync<boost::json::array>(
                                DisplayHelper::FunctionId::GetSources
                            );
    TEST_ASSERT_TRUE(rpc_sources_json.has_value());
    auto rpc_sources = json_array_to<std::vector<DisplayHelper::SourceInfo>>(rpc_sources_json.value());
    TEST_ASSERT_EQUAL_size_t(1, rpc_sources.size());
    TEST_ASSERT_EQUAL_UINT32(source_id, rpc_sources[0].id);
    TEST_ASSERT_EQUAL_STRING("diagnostic", rpc_sources[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("test", rpc_sources[0].role.c_str());
    TEST_ASSERT_EQUAL_size_t(1, rpc_sources[0].preferred_outputs.size());
    TEST_ASSERT_EQUAL_STRING(output0.c_str(), rpc_sources[0].preferred_outputs[0].c_str());

    TEST_ASSERT_TRUE(DisplayService::get_instance().unregister_source(source_id).has_value());
    rpc_sources_json = DisplayHelper::call_function_sync<boost::json::array>(
                           DisplayHelper::FunctionId::GetSources
                       );
    TEST_ASSERT_TRUE(rpc_sources_json.has_value());
    rpc_sources = json_array_to<std::vector<DisplayHelper::SourceInfo>>(rpc_sources_json.value());
    TEST_ASSERT_TRUE(rpc_sources.empty());
}

BROOKESIA_TEST_CASE(source_control_rpc, "Test ServiceDisplay - source control RPC", "[service][display][sources]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    DisplayHelper::SourceInfo source = {
        .name = "rpc-source",
        .role = "test",
        .preferred_outputs = {output0},
        .priority = 0,
    };

    SourceStateMonitor source_monitor;
    TEST_ASSERT_TRUE(source_monitor.start());

    auto source_id = DisplayHelper::call_function_sync<double>(
                         DisplayHelper::FunctionId::RegisterSource,
                         BROOKESIA_DESCRIBE_TO_JSON(source).as_object()
                     );
    TEST_ASSERT_TRUE(source_id.has_value());
    TEST_ASSERT_TRUE(source_id.value() > 0);
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        std::string(),
        source.name,
        source_state_to_string(SourceState::Registered),
    }, EVENT_TIMEOUT_MS));

    auto request_result = DisplayHelper::call_function_sync(
                              DisplayHelper::FunctionId::RequestOutput,
                              source.name,
                              output0
                          );
    TEST_ASSERT_TRUE(request_result.has_value());
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        output0,
        source.name,
        source_state_to_string(SourceState::Requested),
    }, EVENT_TIMEOUT_MS));

    auto rpc_sources_json = DisplayHelper::call_function_sync<boost::json::array>(
                                DisplayHelper::FunctionId::GetSources
                            );
    TEST_ASSERT_TRUE(rpc_sources_json.has_value());
    auto rpc_sources = json_array_to<std::vector<DisplayHelper::SourceInfo>>(rpc_sources_json.value());
    TEST_ASSERT_EQUAL_size_t(1, rpc_sources.size());
    TEST_ASSERT_EQUAL_STRING(source.name.c_str(), rpc_sources[0].name.c_str());

    auto release_result = DisplayHelper::call_function_sync(
                              DisplayHelper::FunctionId::ReleaseOutput,
                              source.name,
                              output0
                          );
    TEST_ASSERT_TRUE(release_result.has_value());
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        output0,
        source.name,
        source_state_to_string(SourceState::Revoked),
    }, EVENT_TIMEOUT_MS));

    auto unregister_result = DisplayHelper::call_function_sync(
                                 DisplayHelper::FunctionId::UnregisterSource,
                                 source.name
                             );
    TEST_ASSERT_TRUE(unregister_result.has_value());
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        std::string(),
        source.name,
        source_state_to_string(SourceState::Revoked),
    }, EVENT_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(output_capabilities, "Test ServiceDisplay - output capabilities", "[service][display][outputs]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    TEST_ASSERT_TRUE(outputs[0].touch.has_value());
    TEST_ASSERT_EQUAL_STRING("Touch0", outputs[0].touch->name.c_str());
    TEST_ASSERT_TRUE(outputs[0].touch->operation_mode == TouchOperationMode::Interrupt);
    TEST_ASSERT_EQUAL_UINT32(5, outputs[0].touch->max_points);
    TEST_ASSERT_TRUE(outputs[0].backlight.has_value());
    TEST_ASSERT_TRUE(outputs[0].backlight->on_off_supported);

    TEST_ASSERT_TRUE(outputs[1].touch.has_value());
    TEST_ASSERT_EQUAL_STRING("Touch1", outputs[1].touch->name.c_str());
    TEST_ASSERT_TRUE(outputs[1].touch->operation_mode == TouchOperationMode::Polling);
    TEST_ASSERT_TRUE(outputs[1].backlight.has_value());

    auto rpc_outputs_json = DisplayHelper::call_function_sync<boost::json::array>(
                                DisplayHelper::FunctionId::GetOutputs
                            );
    TEST_ASSERT_TRUE(rpc_outputs_json.has_value());
    auto rpc_outputs = json_array_to<std::vector<DisplayHelper::OutputInfo>>(rpc_outputs_json.value());
    TEST_ASSERT_TRUE(rpc_outputs[0].touch.has_value());
    TEST_ASSERT_TRUE(rpc_outputs[0].backlight.has_value());
}

BROOKESIA_TEST_CASE(touch_interrupt_cache, "Test ServiceDisplay - touch interrupt cache", "[service][display][touch]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    TouchUpdateCollector collector;
    auto connection = DisplayService::get_instance().connect_touch_updated(
                          outputs[0].name,
    [&collector](const std::string & output_name, const DisplayService::TouchSnapshot & snapshot) {
        collector.update(output_name, snapshot);
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 23, .y = 45, .pressure = 67, .track_id = 7},
        hal::display::TouchIface::Point{.x = 123, .y = 145, .pressure = 167, .track_id = 8},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(collector.wait_for_points(outputs[0].name, 2, 7, EVENT_TIMEOUT_MS));

    auto snapshot = DisplayService::get_instance().get_touch_snapshot(outputs[0].name);
    TEST_ASSERT_TRUE(snapshot.has_value());
    TEST_ASSERT_TRUE(snapshot->valid);
    TEST_ASSERT_EQUAL_size_t(2, snapshot->points.size());
    TEST_ASSERT_EQUAL_UINT8(7, snapshot->points[0].track_id);
    TEST_ASSERT_EQUAL_UINT32(23, snapshot->points[0].x);
    TEST_ASSERT_EQUAL_UINT32(45, snapshot->points[0].y);
    TEST_ASSERT_EQUAL_UINT32(67, snapshot->points[0].pressure);
    TEST_ASSERT_EQUAL_UINT8(8, snapshot->points[1].track_id);
    TEST_ASSERT_EQUAL_UINT32(123, snapshot->points[1].x);
    TEST_ASSERT_EQUAL_UINT32(145, snapshot->points[1].y);
    TEST_ASSERT_EQUAL_UINT32(167, snapshot->points[1].pressure);
}

BROOKESIA_TEST_CASE(touch_polling_cache, "Test ServiceDisplay - touch polling cache", "[service][display][touch]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    1, {
        hal::display::TouchIface::Point{.x = 91, .y = 92, .pressure = 93, .track_id = 3},
        hal::display::TouchIface::Point{.x = 191, .y = 192, .pressure = 193, .track_id = 4},
    }
    );

    TEST_ASSERT_TRUE(
        esp_brookesia::test_apps::service_display::MockDisplayDevice::wait_for_touch_read_count(
            1, 1, EVENT_TIMEOUT_MS
        )
    );

    const uint32_t step_ms = 10;
    DisplayService::TouchSnapshot snapshot;
    bool snapshot_ready = false;
    for (uint32_t waited_ms = 0; waited_ms <= EVENT_TIMEOUT_MS; waited_ms += step_ms) {
        auto snapshot_result = DisplayService::get_instance().get_touch_snapshot(outputs[1].name);
        if (snapshot_result.has_value() && snapshot_result->valid && (snapshot_result->points.size() == 2) &&
                (snapshot_result->points[0].track_id == 3)) {
            snapshot = snapshot_result.value();
            snapshot_ready = true;
            break;
        }
        lib_utils::test_adapter::delay_ms(step_ms);
    }
    TEST_ASSERT_TRUE(snapshot_ready);
    TEST_ASSERT_EQUAL_UINT32(91, snapshot.points[0].x);
    TEST_ASSERT_EQUAL_UINT8(4, snapshot.points[1].track_id);
    TEST_ASSERT_EQUAL_UINT32(191, snapshot.points[1].x);
}

BROOKESIA_TEST_CASE(touch_gesture_default_disabled, "Test ServiceDisplay - touch gesture disabled", "[service][display][gesture]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    TouchGestureCollector collector;
    auto connection = DisplayService::get_instance().connect_touch_gesture(
                          outputs[0].name,
    [&collector](const std::string & output_name, const TouchGestureInfo & info) {
        collector.update(output_name, info);
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 30, .y = 30, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    lib_utils::test_adapter::delay_ms(100);
    TEST_ASSERT_EQUAL_size_t(0, collector.count(TouchGestureEventType::Press));
}

BROOKESIA_TEST_CASE(touch_gesture_basic, "Test ServiceDisplay - touch gesture basic", "[service][display][gesture]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    auto config = make_test_gesture_config();
    auto set_result = DisplayHelper::call_function_sync(
                          DisplayHelper::FunctionId::SetTouchGestureConfig,
                          static_cast<double>(outputs[0].id),
                          BROOKESIA_DESCRIBE_TO_JSON(config).as_object()
                      );
    TEST_ASSERT_TRUE(set_result.has_value());
    auto get_result = DisplayHelper::call_function_sync<boost::json::object>(
                          DisplayHelper::FunctionId::GetTouchGestureConfig,
                          static_cast<double>(outputs[0].id)
                      );
    TEST_ASSERT_TRUE(get_result.has_value());
    TouchGestureConfig resolved_config;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(get_result.value()), resolved_config));
    TEST_ASSERT_TRUE(resolved_config.enabled);
    TEST_ASSERT_EQUAL_UINT16(config.detect_period_ms, resolved_config.detect_period_ms);

    TouchGestureCollector collector;
    auto connection = DisplayHelper::subscribe_event(
                          DisplayHelper::EventId::TouchGesture,
    [&collector](const std::string & event_name, const boost::json::object & info_json) {
        (void)event_name;
        TouchGestureInfo info;
        TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(boost::json::value(info_json), info));
        collector.update(info.output_name, info);
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 10, .y = 10, .pressure = 1, .track_id = 7},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(outputs[0].name, TouchGestureEventType::Press, TouchGestureDirection::None, EVENT_TIMEOUT_MS)
    );

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 90, .y = 10, .pressure = 1, .track_id = 7},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(
            outputs[0].name, TouchGestureEventType::Pressing, TouchGestureDirection::Right, EVENT_TIMEOUT_MS
        )
    );

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(0, {});
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(
            outputs[0].name, TouchGestureEventType::Release, TouchGestureDirection::Right, EVENT_TIMEOUT_MS
        )
    );
}

BROOKESIA_TEST_CASE(touch_gesture_direction_lock, "Test ServiceDisplay - touch gesture direction lock", "[service][display][gesture]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    auto config = make_test_gesture_config();
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_touch_gesture_config(outputs[0].id, config).has_value());

    TouchGestureCollector collector;
    auto connection = DisplayService::get_instance().connect_touch_gesture(
                          outputs[0].name,
    [&collector](const std::string & output_name, const TouchGestureInfo & info) {
        collector.update(output_name, info);
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 20, .y = 20, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(outputs[0].name, TouchGestureEventType::Press, TouchGestureDirection::None, EVENT_TIMEOUT_MS)
    );

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 100, .y = 20, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(
            outputs[0].name, TouchGestureEventType::Pressing, TouchGestureDirection::Right, EVENT_TIMEOUT_MS
        )
    );

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 20, .y = 120, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(
            outputs[0].name, TouchGestureEventType::Pressing, TouchGestureDirection::Right, EVENT_TIMEOUT_MS
        )
    );
    TEST_ASSERT_FALSE(
        collector.wait_for(outputs[0].name, TouchGestureEventType::Pressing, TouchGestureDirection::Down, 80)
    );
}

BROOKESIA_TEST_CASE(touch_gesture_track_id, "Test ServiceDisplay - touch gesture track id", "[service][display][gesture]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    auto config = make_test_gesture_config();
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_touch_gesture_config(outputs[0].id, config).has_value());

    TouchGestureCollector collector;
    auto connection = DisplayService::get_instance().connect_touch_gesture(
                          outputs[0].name,
    [&collector](const std::string & output_name, const TouchGestureInfo & info) {
        collector.update(output_name, info);
    }
                      );
    TEST_ASSERT_TRUE(connection.connected());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 10, .y = 10, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(
        collector.wait_for(outputs[0].name, TouchGestureEventType::Press, TouchGestureDirection::None, EVENT_TIMEOUT_MS)
    );

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_touch_points(
    0, {
        hal::display::TouchIface::Point{.x = 150, .y = 10, .pressure = 1, .track_id = 2},
        hal::display::TouchIface::Point{.x = 80, .y = 10, .pressure = 1, .track_id = 1},
    }
    );
    esp_brookesia::test_apps::service_display::MockDisplayDevice::trigger_touch_interrupt(0);
    TEST_ASSERT_TRUE(collector.wait_for_stop_x(outputs[0].name, 80, EVENT_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(source_arbitration, "Test ServiceDisplay - source arbitration", "[service][display][source]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_a = register_source("video", "video", {output0});
    const uint32_t source_b = register_source("gui", "gui", {output0});

    TEST_ASSERT_TRUE(DisplayService::get_instance().request_output(source_a, output0).has_value());
    TEST_ASSERT_TRUE(DisplayService::get_instance().request_output(source_b, output0).has_value());
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "video").has_value());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 32,
        .height = 32,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0xF800);
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_a, output0, frame, service::RawBuffer(data.data(), data.size())
                  );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);
    TEST_ASSERT_TRUE(wait_for_draw_count(1, DRAW_TIMEOUT_MS));

    auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(1, records.size());

    result = DisplayService::get_instance().present_frame_sync(
                 source_b, output0, frame, service::RawBuffer(data.data(), data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedNotActive);

    records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(1, records.size());
}

BROOKESIA_TEST_CASE(sync_present_no_wait, "Test ServiceDisplay - sync present no wait", "[service][display][sync]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("no-wait", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "no-wait").has_value());

    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_sync_draw_blocked(true);
    lib_utils::FunctionGuard unblock_guard([]() {
        esp_brookesia::test_apps::service_display::MockDisplayDevice::set_sync_draw_blocked(false);
    });

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 32,
        .height = 32,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0x1357);
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_id, output0, frame, service::RawBuffer(data.data(), data.size()), 0
                  );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);
    TEST_ASSERT_TRUE(wait_for_draw_count(1, DRAW_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(partial_update, "Test ServiceDisplay - partial update", "[service][display][partial]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("eyes", "expression", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "eyes").has_value());

    const DisplayService::FrameInfo partial_frame = {
        .x = 0,
        .y = 16,
        .width = 32,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto partial_data = make_rgb565_frame(partial_frame.width, partial_frame.height, 0x07E0);
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_id, output0, partial_frame, service::RawBuffer(partial_data.data(), partial_data.size())
                  );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);
    TEST_ASSERT_TRUE(wait_for_draw_count(1, DRAW_TIMEOUT_MS));

    auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(1, records.size());
    TEST_ASSERT_EQUAL_UINT32(0, records[0].x1);
    TEST_ASSERT_EQUAL_UINT32(16, records[0].y1);
    TEST_ASSERT_EQUAL_UINT32(32, records[0].x2);
    TEST_ASSERT_EQUAL_UINT32(32, records[0].y2);
    TEST_ASSERT_EQUAL_size_t(partial_data.size(), records[0].data.size());

    const DisplayService::FrameInfo invalid_frame = {
        .x = 0,
        .y = 232,
        .width = 32,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto invalid_data = make_rgb565_frame(invalid_frame.width, invalid_frame.height, 0x001F);
    result = DisplayService::get_instance().present_frame_sync(
                 source_id, output0, invalid_frame, service::RawBuffer(invalid_data.data(), invalid_data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedInvalidFrame);

    records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(1, records.size());
}

BROOKESIA_TEST_CASE(buffer_output_present, "Test ServiceDisplay - buffer output present", "[service][display][buffer]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    constexpr uint32_t width = 32;
    constexpr uint32_t height = 24;
    constexpr size_t stride_bytes = 40 * 2;
    std::vector<uint8_t> output_buffer(stride_bytes * height, 0);
    auto output_id = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "Buffer0",
        .width = width,
        .height = height,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(output_buffer.data(), output_buffer.size()),
        .stride_bytes = stride_bytes,
    });
    TEST_ASSERT_TRUE(output_id.has_value());

    auto outputs = DisplayService::get_instance().get_outputs();
    auto output_it = std::find_if(outputs.begin(), outputs.end(), [](const auto & output) {
        return output.name == "Buffer0";
    });
    TEST_ASSERT_TRUE(output_it != outputs.end());
    TEST_ASSERT_TRUE(output_it->slot == OutputSlot::Buffer);
    TEST_ASSERT_TRUE(output_it->panel_instance.empty());

    const uint32_t source_id = register_source("buffer-source", "test", {"Buffer0"});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source("Buffer0", "buffer-source").has_value());

    constexpr uint16_t color = 0xA55A;
    const DisplayService::FrameInfo partial_frame = {
        .x = 3,
        .y = 4,
        .width = 8,
        .height = 6,
        .pixel_format = PixelFormat::RGB565,
    };
    auto frame_data = make_rgb565_frame(partial_frame.width, partial_frame.height, color);
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_id, "Buffer0", partial_frame, service::RawBuffer(frame_data.data(), frame_data.size())
                  );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);

    TEST_ASSERT_EQUAL_UINT16(0, read_rgb565_pixel(output_buffer, stride_bytes, 2, 4));
    TEST_ASSERT_EQUAL_UINT16(color, read_rgb565_pixel(output_buffer, stride_bytes, 3, 4));
    TEST_ASSERT_EQUAL_UINT16(color, read_rgb565_pixel(output_buffer, stride_bytes, 10, 9));
    TEST_ASSERT_EQUAL_UINT16(0, read_rgb565_pixel(output_buffer, stride_bytes, 11, 9));
    auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_TRUE(records.empty());

    bool writer_called = false;
    constexpr uint16_t direct_color = 0x0F0F;
    auto buffer_writer = [&](DisplayService::BufferOutputView & view) {
        writer_called = true;
        auto *data = view.buffer.to_ptr<uint8_t>();
        TEST_ASSERT_NOT_NULL(data);
        for (uint32_t row = 0; row < partial_frame.height; row++) {
            auto *dst = reinterpret_cast<uint16_t *>(
                            data + (partial_frame.y + row) * view.stride_bytes +
                            partial_frame.x * sizeof(uint16_t)
                        );
            std::fill(dst, dst + partial_frame.width, direct_color);
        }
        return true;
    };
    result = DisplayService::get_instance().present_buffer_frame_sync(
                 source_id, "Buffer0", partial_frame, buffer_writer
             );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);
    TEST_ASSERT_TRUE(writer_called);
    TEST_ASSERT_EQUAL_UINT16(direct_color, read_rgb565_pixel(output_buffer, stride_bytes, 3, 4));

    const uint32_t inactive_source = register_source("inactive-buffer-source", "test", {"Buffer0"});
    writer_called = false;
    auto inactive_buffer_writer = [&](DisplayService::BufferOutputView &) {
        writer_called = true;
        return true;
    };
    result = DisplayService::get_instance().present_buffer_frame_sync(
                 inactive_source, "Buffer0", partial_frame, inactive_buffer_writer
             );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedNotActive);
    TEST_ASSERT_FALSE(writer_called);
    TEST_ASSERT_EQUAL_UINT16(direct_color, read_rgb565_pixel(output_buffer, stride_bytes, 3, 4));

    auto inactive_data = make_rgb565_frame(partial_frame.width, partial_frame.height, 0xFFFF);
    result = DisplayService::get_instance().present_frame_sync(
                 inactive_source, "Buffer0", partial_frame, service::RawBuffer(inactive_data.data(), inactive_data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedNotActive);
    TEST_ASSERT_EQUAL_UINT16(direct_color, read_rgb565_pixel(output_buffer, stride_bytes, 3, 4));

    auto invalid_data = frame_data;
    invalid_data.pop_back();
    result = DisplayService::get_instance().present_frame_sync(
                 source_id, "Buffer0", partial_frame, service::RawBuffer(invalid_data.data(), invalid_data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedInvalidFrame);
    TEST_ASSERT_EQUAL_UINT16(direct_color, read_rgb565_pixel(output_buffer, stride_bytes, 3, 4));
}

BROOKESIA_TEST_CASE(buffer_output_async, "Test ServiceDisplay - buffer output async", "[service][display][buffer]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    constexpr uint32_t width = 16;
    constexpr uint32_t height = 16;
    constexpr size_t stride_bytes = width * 2;
    std::vector<uint8_t> output_buffer(stride_bytes * height, 0);
    auto output_id = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "BufferAsync",
        .width = width,
        .height = height,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(output_buffer.data(), output_buffer.size()),
        .stride_bytes = stride_bytes,
    });
    TEST_ASSERT_TRUE(output_id.has_value());

    const uint32_t source_id = register_source("buffer-async-source", "test", {"BufferAsync"});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source("BufferAsync", "buffer-async-source").has_value());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = width,
        .height = height,
        .pixel_format = PixelFormat::RGB565,
    };
    auto frame_data = make_rgb565_frame(frame.width, frame.height, 0x0F0F);
    AsyncCompletionCollector collector;
    auto result = DisplayService::get_instance().present_frame_async(
                      source_id, "BufferAsync", frame, service::RawBuffer(frame_data.data(), frame_data.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                  );
    TEST_ASSERT_TRUE(result.state == PresentSubmitState::Queued);
    TEST_ASSERT_TRUE(result.frame_id > 0);
    TEST_ASSERT_TRUE(collector.wait_for(result.frame_id, PresentResult::Presented, DRAW_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT16(0x0F0F, read_rgb565_pixel(output_buffer, stride_bytes, 0, 0));
}

BROOKESIA_TEST_CASE(buffer_output_invalid_inputs, "Test ServiceDisplay - buffer output invalid inputs", "[service][display][buffer]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    std::vector<uint8_t> output_buffer(8 * 8 * 2, 0);
    auto duplicate_output = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = outputs[0].name,
        .width = 8,
        .height = 8,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(output_buffer.data(), output_buffer.size()),
    });
    TEST_ASSERT_FALSE(duplicate_output.has_value());

    auto const_buffer = service::RawBuffer(static_cast<const uint8_t *>(output_buffer.data()), output_buffer.size());
    auto const_output = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "ConstBuffer",
        .width = 8,
        .height = 8,
        .pixel_format = PixelFormat::RGB565,
        .buffer = const_buffer,
    });
    TEST_ASSERT_FALSE(const_output.has_value());

    auto bad_stride_output = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "BadStride",
        .width = 8,
        .height = 8,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(output_buffer.data(), output_buffer.size()),
        .stride_bytes = 8,
    });
    TEST_ASSERT_FALSE(bad_stride_output.has_value());

    TEST_ASSERT_FALSE(DisplayService::get_instance().unregister_output(outputs[0].name).has_value());

    auto valid_output = DisplayService::get_instance().register_output(DisplayService::BufferOutputConfig{
        .name = "TempBuffer",
        .width = 8,
        .height = 8,
        .pixel_format = PixelFormat::RGB565,
        .buffer = service::RawBuffer(output_buffer.data(), output_buffer.size()),
    });
    TEST_ASSERT_TRUE(valid_output.has_value());
    TEST_ASSERT_TRUE(DisplayService::get_instance().unregister_output("TempBuffer").has_value());
    auto current_outputs = DisplayService::get_instance().get_outputs();
    TEST_ASSERT_TRUE(std::none_of(current_outputs.begin(), current_outputs.end(), [](const auto & output) {
        return output.name == "TempBuffer";
    }));
}

BROOKESIA_TEST_CASE(multiple_outputs, "Test ServiceDisplay - multiple outputs", "[service][display][multi]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const std::string output1 = outputs[1].name;
    const uint32_t source_a = register_source("left-eye", "expression", {output0});
    const uint32_t source_b = register_source("right-eye", "expression", {output1});

    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "left-eye").has_value());
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output1, "right-eye").has_value());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 32,
        .height = 32,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0xFFFF);
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_a, output0, frame, service::RawBuffer(data.data(), data.size())
                  );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);
    result = DisplayService::get_instance().present_frame_sync(
                 source_b, output1, frame, service::RawBuffer(data.data(), data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::Presented);

    TEST_ASSERT_TRUE(wait_for_draw_count(2, DRAW_TIMEOUT_MS));
    auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(2, records.size());
    TEST_ASSERT_EQUAL_UINT32(0, records[0].panel_index);
    TEST_ASSERT_EQUAL_UINT32(1, records[1].panel_index);
}

BROOKESIA_TEST_CASE(invalid_inputs, "Test ServiceDisplay - invalid inputs", "[service][display][invalid]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("bad-source", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "bad-source").has_value());

    const DisplayService::FrameInfo valid_frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto short_data = make_rgb565_frame(valid_frame.width, valid_frame.height, 0x1234);
    short_data.pop_back();
    auto result = DisplayService::get_instance().present_frame_sync(
                      source_id, output0, valid_frame, service::RawBuffer(short_data.data(), short_data.size())
                  );
    TEST_ASSERT_TRUE(result == PresentResult::DroppedInvalidFrame);

    result = DisplayService::get_instance().present_frame_sync(
                 999, output0, valid_frame, service::RawBuffer(short_data.data(), short_data.size())
             );
    TEST_ASSERT_TRUE(result == PresentResult::Error);

    auto duplicate = DisplayService::get_instance().register_source(DisplayService::SourceInfo{
        .name = "bad-source",
        .role = "duplicate",
        .preferred_outputs = {},
        .priority = 0,
    });
    TEST_ASSERT_FALSE(duplicate.has_value());
    TEST_ASSERT_FALSE(DisplayService::get_instance().unregister_output("").has_value());
    TEST_ASSERT_FALSE(DisplayService::get_instance().set_active_source("UnknownOutput", "bad-source").has_value());
    TEST_ASSERT_FALSE(DisplayService::get_instance().set_active_source(output0, "unknown-source").has_value());

    auto records = esp_brookesia::test_apps::service_display::MockDisplayDevice::get_draw_records();
    TEST_ASSERT_EQUAL_size_t(0, records.size());
}

BROOKESIA_TEST_CASE(async_present, "Test ServiceDisplay - async present", "[service][display][async]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("async-source", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "async-source").has_value());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0x1357);
    AsyncCompletionCollector collector;
    auto result = DisplayService::get_instance().present_frame_async(
                      source_id, output0, frame, service::RawBuffer(data.data(), data.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                  );
    TEST_ASSERT_TRUE(result.state == PresentSubmitState::Queued);
    TEST_ASSERT_TRUE(result.frame_id > 0);
    TEST_ASSERT_TRUE(collector.wait_for(result.frame_id, PresentResult::Presented, DRAW_TIMEOUT_MS));
    TEST_ASSERT_TRUE(wait_for_draw_count(1, DRAW_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(async_submit_drop, "Test ServiceDisplay - async submit drop", "[service][display][async]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_a = register_source("active", "test", {output0});
    const uint32_t source_b = register_source("inactive", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "active").has_value());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0x2468);
    AsyncCompletionCollector collector;
    auto result = DisplayService::get_instance().present_frame_async(
                      source_b, output0, frame, service::RawBuffer(data.data(), data.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                  );
    TEST_ASSERT_TRUE(result.state == PresentSubmitState::DroppedNotActive);
    TEST_ASSERT_EQUAL_UINT32(0, result.frame_id);
    TEST_ASSERT_EQUAL_size_t(0, collector.count());

    auto invalid_data = data;
    invalid_data.pop_back();
    result = DisplayService::get_instance().present_frame_async(
                 source_a, output0, frame, service::RawBuffer(invalid_data.data(), invalid_data.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    }
             );
    TEST_ASSERT_TRUE(result.state == PresentSubmitState::DroppedInvalidFrame);
    TEST_ASSERT_EQUAL_UINT32(0, result.frame_id);
    TEST_ASSERT_EQUAL_size_t(0, collector.count());
}

BROOKESIA_TEST_CASE(async_latest_wins, "Test ServiceDisplay - async latest wins", "[service][display][async]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_id = register_source("latest", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "latest").has_value());
    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_sync_draw_blocked(true);

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data_a = make_rgb565_frame(frame.width, frame.height, 0x1111);
    auto data_b = make_rgb565_frame(frame.width, frame.height, 0x2222);
    auto data_c = make_rgb565_frame(frame.width, frame.height, 0x3333);
    AsyncCompletionCollector collector;
    auto result_a = DisplayService::get_instance().present_frame_async(
                        source_id, output0, frame, service::RawBuffer(data_a.data(), data_a.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                    );
    TEST_ASSERT_TRUE(result_a.state == PresentSubmitState::Queued);
    TEST_ASSERT_TRUE(wait_for_sync_draw_enter_count(1, DRAW_TIMEOUT_MS));
    auto result_b = DisplayService::get_instance().present_frame_async(
                        source_id, output0, frame, service::RawBuffer(data_b.data(), data_b.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                    );
    TEST_ASSERT_TRUE(result_b.state == PresentSubmitState::Queued);
    auto result_c = DisplayService::get_instance().present_frame_async(
                        source_id, output0, frame, service::RawBuffer(data_c.data(), data_c.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                    );
    TEST_ASSERT_TRUE(result_c.state == PresentSubmitState::Queued);

    TEST_ASSERT_TRUE(collector.wait_for(result_b.frame_id, PresentResult::DroppedQueueFull, EVENT_TIMEOUT_MS));
    esp_brookesia::test_apps::service_display::MockDisplayDevice::release_sync_draws(1);
    TEST_ASSERT_TRUE(collector.wait_for(result_a.frame_id, PresentResult::Presented, DRAW_TIMEOUT_MS));
    TEST_ASSERT_TRUE(wait_for_sync_draw_enter_count(2, DRAW_TIMEOUT_MS));
    esp_brookesia::test_apps::service_display::MockDisplayDevice::release_sync_draws(1);
    TEST_ASSERT_TRUE(collector.wait_for(result_c.frame_id, PresentResult::Presented, DRAW_TIMEOUT_MS));
    TEST_ASSERT_TRUE(wait_for_draw_count(2, DRAW_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(async_active_switch_drop, "Test ServiceDisplay - async active switch drop", "[service][display][async]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t source_a = register_source("source-a", "test", {output0});
    (void)register_source("source-b", "test", {output0});
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "source-a").has_value());
    esp_brookesia::test_apps::service_display::MockDisplayDevice::set_sync_draw_blocked(true);

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data_a = make_rgb565_frame(frame.width, frame.height, 0x3333);
    auto data_b = make_rgb565_frame(frame.width, frame.height, 0x4444);
    AsyncCompletionCollector collector;
    auto result_a = DisplayService::get_instance().present_frame_async(
                        source_a, output0, frame, service::RawBuffer(data_a.data(), data_a.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                    );
    TEST_ASSERT_TRUE(result_a.state == PresentSubmitState::Queued);
    TEST_ASSERT_TRUE(wait_for_sync_draw_enter_count(1, DRAW_TIMEOUT_MS));
    auto result_b = DisplayService::get_instance().present_frame_async(
                        source_a, output0, frame, service::RawBuffer(data_b.data(), data_b.size()),
    [&collector](uint32_t frame_id, PresentResult present_result) {
        collector.complete(frame_id, present_result);
    },
    DRAW_TIMEOUT_MS
                    );
    TEST_ASSERT_TRUE(result_b.state == PresentSubmitState::Queued);
    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source(output0, "source-b").has_value());
    TEST_ASSERT_TRUE(collector.wait_for(result_b.frame_id, PresentResult::DroppedNotActive, EVENT_TIMEOUT_MS));
    esp_brookesia::test_apps::service_display::MockDisplayDevice::release_sync_draws(1);
    TEST_ASSERT_TRUE(collector.wait_for(result_a.frame_id, PresentResult::Presented, DRAW_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(control_rpc_by_name, "Test ServiceDisplay - control RPC by name", "[service][display][control]")
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    (void)register_source("lvgl", "gui", {output0});
    (void)register_source("emote", "expression", {output0});

    SourceStateMonitor source_monitor;
    ActiveSourceMonitor active_monitor;
    TEST_ASSERT_TRUE(source_monitor.start());
    TEST_ASSERT_TRUE(active_monitor.start());

    auto result = DisplayHelper::call_function_sync(
                      DisplayHelper::FunctionId::SetActiveSource,
                      output0,
                      std::string("lvgl")
                  );
    TEST_ASSERT_TRUE(result.has_value());
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        output0,
        std::string("lvgl"),
        source_state_to_string(SourceState::Granted),
    }, EVENT_TIMEOUT_MS));
    TEST_ASSERT_TRUE(active_monitor.wait_for({output0, std::string("lvgl")}, EVENT_TIMEOUT_MS));

    auto active_source = DisplayHelper::call_function_sync<std::string>(
                             DisplayHelper::FunctionId::GetActiveSource,
                             output0
                         );
    TEST_ASSERT_TRUE(active_source.has_value());
    TEST_ASSERT_EQUAL_STRING("lvgl", active_source.value().c_str());

    source_monitor.clear();
    active_monitor.clear();
    result = DisplayHelper::call_function_sync(
                 DisplayHelper::FunctionId::SetActiveSource,
                 output0,
                 std::string("emote")
             );
    TEST_ASSERT_TRUE(result.has_value());
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        output0,
        std::string("lvgl"),
        source_state_to_string(SourceState::Dummy),
    }, EVENT_TIMEOUT_MS));
    TEST_ASSERT_TRUE(source_monitor.wait_for({
        output0,
        std::string("emote"),
        source_state_to_string(SourceState::Granted),
    }, EVENT_TIMEOUT_MS));
    TEST_ASSERT_TRUE(active_monitor.wait_for({output0, std::string("emote")}, EVENT_TIMEOUT_MS));
}

BROOKESIA_TEST_CASE(
    active_source_role_and_default_output,
    "Test ServiceDisplay - active source role and default output",
    "[service][display][control]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(startup(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto outputs = fetch_outputs();
    const std::string output0 = outputs[0].name;
    const uint32_t gui_first = register_source("gui-first", "gui", {output0});
    (void)register_source("gui-second", "gui", {output0});
    (void)register_source("emote", "expression", {output0});
    (void)register_source("empty-role", "", {output0});

    auto roles_json = DisplayHelper::call_function_sync<boost::json::array>(
                          DisplayHelper::FunctionId::GetSourceRoles
                      );
    TEST_ASSERT_TRUE(roles_json.has_value());
    auto roles = json_array_to<std::vector<std::string>>(roles_json.value());
    TEST_ASSERT_EQUAL_size_t(2, roles.size());
    TEST_ASSERT_EQUAL_STRING("gui", roles[0].c_str());
    TEST_ASSERT_EQUAL_STRING("expression", roles[1].c_str());

    auto result = DisplayHelper::call_function_sync(
                      DisplayHelper::FunctionId::SetActiveSourceRole,
                      std::string(),
                      std::string("gui")
                  );
    TEST_ASSERT_TRUE(result.has_value());

    auto active_source = DisplayHelper::call_function_sync<std::string>(
                             DisplayHelper::FunctionId::GetActiveSource,
                             std::string()
                         );
    TEST_ASSERT_TRUE(active_source.has_value());
    TEST_ASSERT_EQUAL_STRING("gui-first", active_source.value().c_str());

    auto active_role = DisplayHelper::call_function_sync<std::string>(
                           DisplayHelper::FunctionId::GetActiveRole,
                           std::string()
                       );
    TEST_ASSERT_TRUE(active_role.has_value());
    TEST_ASSERT_EQUAL_STRING("gui", active_role.value().c_str());

    const DisplayService::FrameInfo frame = {
        .x = 0,
        .y = 0,
        .width = 16,
        .height = 16,
        .pixel_format = PixelFormat::RGB565,
    };
    auto data = make_rgb565_frame(frame.width, frame.height, 0x07E0);
    auto present_result = DisplayService::get_instance().present_frame_sync(
                              gui_first, "", frame, service::RawBuffer(data.data(), data.size())
                          );
    TEST_ASSERT_TRUE(present_result == PresentResult::Presented);
    TEST_ASSERT_TRUE(wait_for_draw_count(1, DRAW_TIMEOUT_MS));

    TEST_ASSERT_TRUE(DisplayService::get_instance().set_active_source("", "emote").has_value());
    active_source = DisplayHelper::call_function_sync<std::string>(
                        DisplayHelper::FunctionId::GetActiveSource,
                        std::string()
                    );
    TEST_ASSERT_TRUE(active_source.has_value());
    TEST_ASSERT_EQUAL_STRING("emote", active_source.value().c_str());
    active_role = DisplayHelper::call_function_sync<std::string>(
                      DisplayHelper::FunctionId::GetActiveRole,
                      std::string()
                  );
    TEST_ASSERT_TRUE(active_role.has_value());
    TEST_ASSERT_EQUAL_STRING("expression", active_role.value().c_str());

    result = DisplayHelper::call_function_sync(
                 DisplayHelper::FunctionId::SetActiveSourceRole,
                 std::string(),
                 std::string("gui")
             );
    TEST_ASSERT_TRUE(result.has_value());
    TEST_ASSERT_FALSE(DisplayService::get_instance().set_active_source_role("", "").has_value());
    TEST_ASSERT_FALSE(DisplayService::get_instance().set_active_source_role("", "missing-role").has_value());

    active_source = DisplayHelper::call_function_sync<std::string>(
                        DisplayHelper::FunctionId::GetActiveSource,
                        std::string()
                    );
    TEST_ASSERT_TRUE(active_source.has_value());
    TEST_ASSERT_EQUAL_STRING("gui-first", active_source.value().c_str());
}
