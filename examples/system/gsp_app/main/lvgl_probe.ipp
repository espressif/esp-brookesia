/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include "boost/json/array.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/gui_lvgl/backend.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_display/service_display.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/network/wifi.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/system_super.hpp"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "lvgl.h"
#include "lvgl/display.hpp"

#if !LV_USE_OBJ_NAME
#   error "The LVGL hardware probe requires CONFIG_LV_USE_OBJ_NAME=y"
#endif

namespace esp_brookesia::gsp_app::lvgl_probe {

namespace {

constexpr int MEASURED_CYCLES = 20;
constexpr int SCAN_TIMEOUT_MS = 20000;
constexpr int PAGE_TIMEOUT_MS = 8000;
constexpr int SERVICE_TIMEOUT_MS = 1000;
constexpr int PANEL_WIDTH = 1024;
constexpr int PANEL_HEIGHT = 600;
constexpr std::string_view SHELL_ID = "brookesia.system.super.shell";
constexpr std::string_view SETTINGS_ID = "brookesia.general.settings";
constexpr std::string_view CONTENT_FLOW = "settings_content";
constexpr std::string_view HOME_DISPLAY_PATH = "/settings_home/page/main_list/display";
constexpr std::string_view HOME_WIFI_PATH = "/settings_home/page/main_list/wifi";
constexpr std::string_view BRIGHTNESS_PATH = "/display/page/brightness_card/brightness_row/slider";
constexpr std::string_view WIFI_SWITCH_PATH = "/wifi/page/wifi_status_card/wifi_switch";
constexpr std::string_view WIFI_REFRESH_PATH = "/wifi/page/wifi_content/available_header/available_refresh";
constexpr std::string_view BACK_PATH = "/settings_header/bar/back";

using DisplayHelper = service::helper::Display;
using DeviceHelper = service::helper::Device;
using WifiHelper = service::helper::Wifi;
using StorageHelper = service::helper::Storage;
using System = system::super::System;
using AppId = system::core::AppId;

struct HeapSample {
    size_t internal_free = 0;
    size_t internal_largest = 0;
    size_t psram_free = 0;
    size_t psram_largest = 0;
};

struct Context {
    System &system;
    ::Display &display;
    hal::display::PanelIface &panel;
    AppId shell_id = esp_brookesia::system::core::INVALID_APP_ID;
    AppId settings_id = esp_brookesia::system::core::INVALID_APP_ID;
    uint32_t backlight_output_id = 0;
    int original_brightness = 0;
    bool brightness_changed = false;
    bool wifi_enabled_by_probe = false;
    std::shared_ptr<std::atomic<unsigned>> scan_results{};
    std::shared_ptr<std::atomic<unsigned>> scan_stops{};
};

template <typename Predicate>
bool wait_for(Predicate &&ready, int timeout_ms, std::string_view reason)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    BROOKESIA_LOGE("LVGL probe timed out: %1%", reason);
    return false;
}

lv_obj_t *find_named_object(lv_obj_t *root, std::string_view path)
{
    if (root == nullptr) {
        return nullptr;
    }
    if (const char *name = lv_obj_get_name(root); name != nullptr && path == name) {
        return root;
    }
    for (uint32_t index = 0; index < lv_obj_get_child_count(root); ++index) {
        if (auto *found = find_named_object(lv_obj_get_child(root, index), path); found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

std::optional<gui::ViewFrame> absolute_frame(const Context &context, AppId app_id, std::string_view path)
{
    if (path.empty() || path.front() != '/' || !context.system.gui_get_view_frame(app_id, path)) {
        return std::nullopt;
    }
    gui::lvgl::lock_thread();
    lib_utils::FunctionGuard unlock_guard([]() {
        gui::lvgl::unlock_thread();
    });
    for (auto *display = lv_display_get_next(nullptr); display != nullptr;
            display = lv_display_get_next(display)) {
        const std::array roots = {
            lv_display_get_screen_active(display),
            lv_display_get_layer_bottom(display),
            lv_display_get_layer_top(display),
            lv_display_get_layer_sys(display),
        };
        for (auto *root : roots) {
            auto *object = find_named_object(root, path);
            if (object == nullptr) {
                continue;
            }
            lv_obj_update_layout(object);
            lv_area_t coords;
            lv_obj_get_coords(object, &coords);
            gui::ViewFrame frame{
                .x = coords.x1,
                .y = coords.y1,
                .width = lv_area_get_width(&coords),
                .height = lv_area_get_height(&coords),
            };
            return frame.width > 0 && frame.height > 0 ? std::optional(frame) : std::nullopt;
        }
    }
    return std::nullopt;
}

bool release_touch(const Context &context)
{
    auto &display_service = service::Display::get_instance();
    const auto released = display_service.inject_touch(context.display.output_name(),
                          std::vector<service::Display::TouchPoint>{});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto cleared = display_service.clear_injected_touch(context.display.output_name());
    return released.has_value() && cleared.has_value();
}

bool tap(const Context &context, int x, int y)
{
    auto &display_service = service::Display::get_instance();
    if (!display_service.inject_touch(context.display.output_name(), x, y)) {
        return false;
    }
    lib_utils::FunctionGuard release_guard([&]() {
        (void)release_touch(context);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!release_touch(context)) {
        return false;
    }
    release_guard.release();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

bool tap_path(const Context &context, AppId app_id, std::string_view path, bool scroll = false)
{
    if (scroll) {
        auto scrolled = context.system.gui_scroll_to_view(app_id, path, false);
        if (!scrolled) {
            BROOKESIA_LOGE("LVGL probe cannot scroll to %1%: %2%", path, scrolled.error());
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    std::optional<gui::ViewFrame> frame;
    if (!wait_for([&]() {
    frame = absolute_frame(context, app_id, path);
    return frame.has_value();
}, PAGE_TIMEOUT_MS, path)) {
        return false;
    }
    const int x = frame->x + frame->width / 2;
    const int y = frame->y + frame->height / 2;
    if (x < 0 || x >= PANEL_WIDTH || y < 0 || y >= PANEL_HEIGHT) {
        BROOKESIA_LOGE("LVGL probe target outside panel: %1% at (%2%, %3%)", path, x, y);
        return false;
    }
    return tap(context, x, y);
}

bool wait_page(const Context &context, std::string_view page)
{
    return wait_for([&]() {
        const auto state = context.system.gui_get_screen_flow_state(context.settings_id, CONTENT_FLOW);
        return state && *state == page;
    }, PAGE_TIMEOUT_MS, page);
}

bool wait_active(const Context &context, AppId app_id)
{
    return wait_for([&]() {
        const auto active = context.system.get_active_app();
        if (active) {
            return active->app_id == app_id;
        }
        // Super keeps its Shell running when the foreground application stops.
        const auto app = context.system.get_app(app_id);
        return app_id == context.shell_id && app &&
               (app->state == system::core::AppState::Running || app->state == system::core::AppState::Paused);
    }, PAGE_TIMEOUT_MS, "active app");
}

bool open_settings(const Context &context)
{
    const auto path = std::string("/launcher/content/grid_stage/item_layer/app_") +
                      std::to_string(context.settings_id);
    if (!wait_active(context, context.shell_id) || !tap_path(context, context.shell_id, path) ||
            !wait_active(context, context.settings_id) || !wait_page(context, "settings_home")) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

bool close_settings(const Context &context)
{
    // Display injection feeds LVGL input, but the Shell gesture detector reads
    // hardware snapshots. Exercise app shutdown through the public lifecycle API.
    auto result = context.system.stop_app(context.settings_id);
    if (!result) {
        BROOKESIA_LOGE("LVGL probe could not return to the desktop: %1%", result.error());
        return false;
    }
    if (!wait_active(context, context.shell_id)) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return true;
}

bool capture_frame(const Context &context, std::string_view label)
{
    const auto &info = context.panel.get_info();
    hal::display::PanelIface::DriverSpecific driver;
    std::array<void *, 3> buffers{};
    if (info.h_res != PANEL_WIDTH || info.v_res != PANEL_HEIGHT ||
            info.pixel_format != hal::display::PanelIface::PixelFormat::RGB565 ||
            !context.panel.get_driver_specific(driver) ||
            driver.bus_type != hal::display::PanelIface::BusType::MIPI ||
            driver.frame_buffer_count != buffers.size() ||
            esp_lcd_dpi_panel_get_frame_buffer(static_cast<esp_lcd_panel_handle_t>(driver.panel_handle),
                    buffers.size(), &buffers[0], &buffers[1], &buffers[2]) != ESP_OK) {
        BROOKESIA_LOGE("LVGL probe cannot capture the 1024x600 RGB565 DPI buffers");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::printf("LVGL_CASE,%.*s\n", static_cast<int>(label.size()), label.data());
    for (size_t frame = 0; frame < buffers.size(); ++frame) {
        const auto *pixels = static_cast<const uint16_t *>(buffers[frame]);
        std::printf("LVGL_FRAME_BEGIN %zu 512 300\n", frame);
        for (int y = 0; y < PANEL_HEIGHT; y += 2) {
            std::string line = "LVGL_PIX ";
            line.reserve(2058);
            for (int x = 0; x < PANEL_WIDTH; x += 2) {
                char hex[5];
                std::snprintf(hex, sizeof(hex), "%04x", pixels[y * PANEL_WIDTH + x]);
                line += hex;
            }
            std::puts(line.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::puts("LVGL_FRAME_END");
    }
    return true;
}

HeapSample sample_heap()
{
    return {
        .internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
    };
}

bool check_device_service()
{
    auto capabilities_json = DeviceHelper::call_function_sync<boost::json::array>(
                                 DeviceHelper::FunctionId::GetCapabilities,
                                 service::helper::Timeout(SERVICE_TIMEOUT_MS));
    if (!capabilities_json) {
        BROOKESIA_LOGE("LVGL probe Device capabilities query failed: %1%", capabilities_json.error());
        return false;
    }
    DeviceHelper::Capabilities capabilities;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*capabilities_json, capabilities)) {
        BROOKESIA_LOGE("LVGL probe cannot decode Device capabilities");
        return false;
    }
    size_t interface_count = 0;
    size_t player_count = 0;
    size_t recorder_count = 0;
    size_t camera_count = 0;
    for (const auto &device : capabilities) {
        interface_count += device.interfaces.size();
        for (const auto &interface : device.interfaces) {
            player_count += interface.type_name == hal::audio::CodecPlayerIface::NAME;
            recorder_count += interface.type_name == hal::audio::CodecRecorderIface::NAME;
            camera_count += interface.type_name == hal::video::CameraIface::NAME;
        }
    }
    auto camera_json = DeviceHelper::call_function_sync<boost::json::array>(
                           DeviceHelper::FunctionId::GetCameraDeviceInfos,
                           service::helper::Timeout(SERVICE_TIMEOUT_MS));
    if (!camera_json) {
        BROOKESIA_LOGE("LVGL probe Device camera info query failed: %1%", camera_json.error());
        return false;
    }
    DeviceHelper::CameraDeviceInfos camera_infos;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(*camera_json, camera_infos)) {
        BROOKESIA_LOGE("LVGL probe cannot decode Device camera infos");
        return false;
    }
    std::printf("LVGL_DEVICE,%zu,%zu,%zu,%zu,%zu,%zu\n", capabilities.size(), interface_count,
                player_count, recorder_count, camera_count, camera_infos.size());
    if (player_count == 0 || recorder_count == 0 || camera_count == 0) {
        BROOKESIA_LOGE("LVGL probe retained codec or camera HAL interface is missing");
        return false;
    }
    std::puts("LVGL_DEVICE_PASS");
    return true;
}

std::optional<std::string> wifi_state()
{
    auto state = WifiHelper::call_function_sync<std::string>(
                     WifiHelper::FunctionId::GetGeneralState, service::helper::Timeout(SERVICE_TIMEOUT_MS));
    return state ? std::optional(*state) : std::nullopt;
}

bool wifi_state_is_started(std::string_view state)
{
    return state == "Started" || state == "Connecting" ||
           state == "Connected" || state == "Disconnecting";
}

bool wifi_started()
{
    auto state = wifi_state();
    return state && wifi_state_is_started(*state);
}

bool wait_scan_result(const Context &context, unsigned previous_results, unsigned previous_stops)
{
    if (!wait_for([&]() {
    return context.scan_results->load() > previous_results && context.scan_stops->load() > previous_stops;
}, SCAN_TIMEOUT_MS, "Wi-Fi result and stop")) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    return true;
}

bool open_wifi_and_scan(const Context &context)
{
    const auto previous_results = context.scan_results->load();
    const auto previous_stops = context.scan_stops->load();
    if (!tap_path(context, context.settings_id, HOME_WIFI_PATH, true) || !wait_page(context, "wifi")) {
        return false;
    }
    return wait_scan_result(context, previous_results, previous_stops);
}

bool refresh_wifi(const Context &context)
{
    const auto previous_results = context.scan_results->load();
    const auto previous_stops = context.scan_stops->load();
    return tap_path(context, context.settings_id, WIFI_REFRESH_PATH, true) &&
           wait_scan_result(context, previous_results, previous_stops);
}

bool back_home(const Context &context)
{
    return tap_path(context, context.settings_id, BACK_PATH) && wait_page(context, "settings_home");
}

bool check_brightness(Context &context)
{
    if (!tap_path(context, context.settings_id, HOME_DISPLAY_PATH, true) || !wait_page(context, "display")) {
        return false;
    }
    auto frame = absolute_frame(context, context.settings_id, BRIGHTNESS_PATH);
    if (!frame || frame->width < 40) {
        BROOKESIA_LOGE("LVGL probe brightness slider frame unavailable");
        return false;
    }
    const int target_x = frame->x + frame->width * (context.original_brightness > 50 ? 25 : 75) / 100;
    context.brightness_changed = true;
    if (!tap(context, target_x, frame->y + frame->height / 2)) {
        return false;
    }
    const auto changed = [&]() {
        auto value = DisplayHelper::call_function_sync<double>(
                         DisplayHelper::FunctionId::GetBacklightBrightness,
                         static_cast<double>(context.backlight_output_id),
                         service::helper::Timeout(SERVICE_TIMEOUT_MS));
        return value && std::abs(*value - context.original_brightness) >= 10.0;
    };
    if (!wait_for(changed, PAGE_TIMEOUT_MS, "brightness change")) {
        return false;
    }
    if (!capture_frame(context, "display_brightness")) {
        return false;
    }
    auto restored = DisplayHelper::call_function_sync(
                        DisplayHelper::FunctionId::SetBacklightBrightness,
                        static_cast<double>(context.backlight_output_id),
                        static_cast<double>(context.original_brightness),
                        service::helper::Timeout(SERVICE_TIMEOUT_MS));
    if (!restored) {
        BROOKESIA_LOGE("LVGL probe could not restore brightness: %1%", restored.error());
        return false;
    }
    context.brightness_changed = false;
    return back_home(context);
}

bool restore_wifi_via_ui(Context &context)
{
    if (!context.wifi_enabled_by_probe) {
        return true;
    }
    if (!open_settings(context) || !tap_path(context, context.settings_id, HOME_WIFI_PATH, true) ||
            !wait_page(context, "wifi") || !tap_path(context, context.settings_id, WIFI_SWITCH_PATH)) {
        return false;
    }
    if (!wait_for([]() {
    const auto state = wifi_state();
    return state && (*state == "Inited" || *state == "Idle");
}, PAGE_TIMEOUT_MS, "Wi-Fi restore")) {
        return false;
    }
    if (!back_home(context) || !close_settings(context)) {
        return false;
    }
    auto saved = StorageHelper::save_key_value("app.settings", "WifiEnabled", false, SERVICE_TIMEOUT_MS);
    if (!saved) {
        BROOKESIA_LOGE("LVGL probe could not persist restored Wi-Fi preference: %1%", saved.error());
        return false;
    }
    context.wifi_enabled_by_probe = false;
    return true;
}

} // namespace

bool run(System &system, ::Display &display)
{
    auto panel = hal::acquire_first_interface<hal::display::PanelIface>();
    if (!panel) {
        BROOKESIA_LOGE("LVGL probe panel unavailable");
        return false;
    }
    Context context{.system = system, .display = display, .panel = *panel};
    for (const auto &app : system.list_apps()) {
        if (app.manifest.id == SHELL_ID) {
            context.shell_id = app.app_id;
        } else if (app.manifest.id == SETTINGS_ID) {
            context.settings_id = app.app_id;
        }
    }
    if (context.shell_id == esp_brookesia::system::core::INVALID_APP_ID ||
            context.settings_id == esp_brookesia::system::core::INVALID_APP_ID ||
            !wait_active(context, context.shell_id)) {
        BROOKESIA_LOGE("LVGL probe Shell or Settings app missing");
        return false;
    }

    auto &manager = service::ServiceManager::get_instance();
    auto display_binding = manager.bind(DisplayHelper::get_name().data());
    auto device_binding = manager.bind(DeviceHelper::get_name().data());
    auto wifi_binding = manager.bind(WifiHelper::get_name().data());
    auto storage_binding = manager.bind(StorageHelper::get_name().data());
    if (!display_binding.is_valid() || !device_binding.is_valid() || !wifi_binding.is_valid() ||
            !storage_binding.is_valid()) {
        BROOKESIA_LOGE("LVGL probe Display, Device, Wi-Fi, or Storage service unavailable");
        return false;
    }
    if (!check_device_service()) {
        return false;
    }
    const auto outputs = service::Display::get_instance().get_outputs();
    bool backlight_found = false;
    for (const auto &output : outputs) {
        if (output.name == display.output_name() && output.backlight) {
            context.backlight_output_id = output.id;
            backlight_found = true;
            break;
        }
    }
    if (!backlight_found) {
        BROOKESIA_LOGE("LVGL probe backlight output unavailable");
        return false;
    }
    auto brightness = DisplayHelper::call_function_sync<double>(
                          DisplayHelper::FunctionId::GetBacklightBrightness,
                          static_cast<double>(context.backlight_output_id),
                          service::helper::Timeout(SERVICE_TIMEOUT_MS));
    if (!brightness) {
        return false;
    }
    context.original_brightness = static_cast<int>(*brightness);
    context.scan_results = std::make_shared<std::atomic<unsigned>>(0);
    context.scan_stops = std::make_shared<std::atomic<unsigned>>(0);
    const service::EventRegistry::SignalSlot scan_callback =
    [results = context.scan_results](const std::string &, const service::EventItemMap & items) {
        const auto found = items.find("ApInfos");
        if (found != items.end() && std::get_if<boost::json::array>(&found->second)) {
            results->fetch_add(1);
        }
    };
    auto scan_connection = WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated, scan_callback);
    const service::EventRegistry::SignalSlot scan_state_callback =
    [stops = context.scan_stops](const std::string &, const service::EventItemMap & items) {
        const auto found = items.find("IsRunning");
        if (found != items.end()) {
            const auto *running = std::get_if<bool>(&found->second);
            if (running && !*running) {
                stops->fetch_add(1);
            }
        }
    };
    auto state_connection = WifiHelper::subscribe_event(WifiHelper::EventId::ScanStateChanged, scan_state_callback);
    if (!scan_connection.connected() || !state_connection.connected()) {
        BROOKESIA_LOGE("LVGL probe cannot subscribe to Wi-Fi scan events");
        return false;
    }
    lib_utils::FunctionGuard restore_guard([&]() {
        if (context.brightness_changed) {
            auto result = DisplayHelper::call_function_sync(
                              DisplayHelper::FunctionId::SetBacklightBrightness,
                              static_cast<double>(context.backlight_output_id),
                              static_cast<double>(context.original_brightness),
                              service::helper::Timeout(SERVICE_TIMEOUT_MS));
            if (!result) {
                BROOKESIA_LOGE("LVGL probe brightness cleanup failed: %1%", result.error());
            }
        }
        if (context.wifi_enabled_by_probe) {
            auto saved = StorageHelper::save_key_value("app.settings", "WifiEnabled", false, SERVICE_TIMEOUT_MS);
            auto stopped = WifiHelper::call_function_sync(
                               WifiHelper::FunctionId::TriggerGeneralAction, "Stop",
                               service::helper::Timeout(SERVICE_TIMEOUT_MS));
            if (!saved || !stopped) {
                BROOKESIA_LOGE("LVGL probe Wi-Fi preference cleanup failed");
            }
        }
    });

    if (!capture_frame(context, "desktop_initial") || !open_settings(context) ||
            !capture_frame(context, "settings_home") || !check_brightness(context)) {
        return false;
    }
    const auto initial_results = context.scan_results->load();
    const auto initial_stops = context.scan_stops->load();
    if (!tap_path(context, context.settings_id, HOME_WIFI_PATH, true) || !wait_page(context, "wifi")) {
        return false;
    }
    std::optional<std::string> settled_wifi_state;
    const auto wifi_settled = [&]() {
        settled_wifi_state = wifi_state();
        return settled_wifi_state && (*settled_wifi_state == "Started" || *settled_wifi_state == "Connected" ||
                                      *settled_wifi_state == "Inited" || *settled_wifi_state == "Idle");
    };
    if (!wait_for(wifi_settled, SCAN_TIMEOUT_MS, "initial Wi-Fi state")) {
        return false;
    }
    if (*settled_wifi_state == "Inited" || *settled_wifi_state == "Idle") {
        context.wifi_enabled_by_probe = true;
        if (!tap_path(context, context.settings_id, WIFI_SWITCH_PATH) ||
                !wait_for(wifi_started, SCAN_TIMEOUT_MS, "Wi-Fi start") || !refresh_wifi(context)) {
            return false;
        }
    }
    if (!wait_scan_result(context, initial_results, initial_stops) || !capture_frame(context, "wifi_scan") ||
            !refresh_wifi(context) || !back_home(context) || !close_settings(context) ||
            !capture_frame(context, "desktop_returned")) {
        return false;
    }
    const auto baseline = sample_heap();
    HeapSample last = baseline;
    std::array<HeapSample, MEASURED_CYCLES> samples{};
    for (int cycle = 1; cycle <= MEASURED_CYCLES; ++cycle) {
        if (!open_settings(context) || !open_wifi_and_scan(context) || !refresh_wifi(context) ||
                !back_home(context) || !close_settings(context)) {
            BROOKESIA_LOGE("LVGL probe failed in cycle %1%", cycle);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        last = sample_heap();
        samples[cycle - 1] = last;
        std::printf("LVGL_HEAP,%d,%zu,%zu,%zu,%zu\n", cycle,
                    last.internal_free, last.internal_largest, last.psram_free, last.psram_largest);
    }
    if (!restore_wifi_via_ui(context)) {
        BROOKESIA_LOGE("LVGL probe could not restore Wi-Fi through Settings");
        return false;
    }
    for (size_t index = 0; index < samples.size(); ++index) {
        const auto &sample = samples[index];
        std::printf("LVGL_RECORD,%zu,%zu,%zu,%zu,%zu\n", index + 1,
                    sample.internal_free, sample.internal_largest, sample.psram_free, sample.psram_largest);
    }
    std::printf("LVGL_PROBE_PASS,%d,%lld,%lld\n", MEASURED_CYCLES,
                static_cast<long long>(last.internal_free) - static_cast<long long>(baseline.internal_free),
                static_cast<long long>(last.psram_free) - static_cast<long long>(baseline.psram_free));
    return true;
}

} // namespace esp_brookesia::gsp_app::lvgl_probe
