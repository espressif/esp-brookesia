/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define BROOKESIA_LOG_TAG "GspApp"
#include "brookesia/lib_utils/log.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "boost/json/array.hpp"
#include "brookesia/app_settings/settings_app.hpp"
#include "brookesia/hal_adaptor/display/device.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/service_display/service_display.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/system_core.hpp"
#include "esp_heap_caps.h"
#include "launcher_app.hpp"

#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "brookesia/service_helper/network/wifi.hpp"
#   include "esp_gsp_debug.h"
#   include "esp_lcd_mipi_dsi.h"
#   include "wifi_probe_hooks.hpp"
#endif

namespace esp_brookesia::gsp_app {

namespace {

constexpr uint16_t EXPECTED_H_RES = 1024;
constexpr uint16_t EXPECTED_V_RES = 600;
constexpr int SERVICE_TIMEOUT_MS = 1000;
constexpr int WARMUP_CYCLES = 2;
constexpr int MEASURED_CYCLES = 20;
// Leave this off for a normal start. Turn it on to change the brightness
// once at boot, then put the saved percent back.
constexpr bool RUN_BRIGHTNESS_PROBE = false;

using DisplayHelper = service::helper::Display;
using StorageHelper = service::helper::Storage;

struct ShellState {
    std::atomic<bool> handoff{false};
    system::core::AppId launcher_id = system::core::INVALID_APP_ID;
    system::core::AppId settings_id = system::core::INVALID_APP_ID;
};

ShellState shell_state;

class ShellSystem: public system::core::System {
public:
    void on_app_stopped(const system::core::AppInfo &app) override
    {
        if (!shell_state.handoff.load()) {
            return;
        }
        std::expected<void, std::string> started = std::unexpected("No GSP app to start");
        if (app.manifest.id == "gsp_launcher") {
            BROOKESIA_LOGI("Launcher stopped; starting Settings");
            started = start_app(shell_state.settings_id);
        } else if (app.manifest.id == "brookesia.general.settings") {
            BROOKESIA_LOGI("Settings stopped; returning to the launcher");
            started = start_app(shell_state.launcher_id);
        } else {
            return;
        }
        if (!started) {
            BROOKESIA_LOGE("Failed to switch the active GSP app: %1%", started.error());
        }
    }
};

struct HeapSample {
    size_t internal_free = 0;
    size_t internal_largest = 0;
    size_t psram_free = 0;
    size_t psram_largest = 0;
};

HeapSample sample_heap()
{
    return {
        .internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
    };
}

void log_heap(const char *label, const HeapSample &sample)
{
    BROOKESIA_LOGI(
        "Heap %1%: internal_free(%2%) internal_largest(%3%) psram_free(%4%) psram_largest(%5%)",
        label, sample.internal_free, sample.internal_largest, sample.psram_free, sample.psram_largest
    );
}

bool run_app(system::core::System &system, system::core::AppId app_id, const char *name, bool start)
{
    auto result = start ? system.start_app(app_id) : system.stop_app(app_id);
    if (!result) {
        BROOKESIA_LOGE("Failed to %1% %2%: %3%", start ? "start" : "stop", name, result.error());
        return false;
    }
    return true;
}

bool prepare_backlight(uint32_t &output_id, int &brightness)
{
    auto outputs_json = DisplayHelper::call_function_sync<boost::json::array>(
                            DisplayHelper::FunctionId::GetOutputs,
                            service::helper::Timeout(SERVICE_TIMEOUT_MS)
                        );
    if (!outputs_json) {
        BROOKESIA_LOGE("GetOutputs failed: %1%", outputs_json.error());
        return false;
    }
    std::vector<DisplayHelper::OutputInfo> outputs;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(outputs_json.value(), outputs)) {
        BROOKESIA_LOGE("Failed to parse Display outputs");
        return false;
    }
    for (const auto &output : outputs) {
        if (!output.backlight.has_value()) {
            continue;
        }
        output_id = output.id;
        auto turned_on = DisplayHelper::call_function_sync(
                             DisplayHelper::FunctionId::SetBacklightOnOff,
                             static_cast<double>(output.id),
                             true,
                             service::helper::Timeout(SERVICE_TIMEOUT_MS)
                         );
        if (!turned_on) {
            BROOKESIA_LOGE("SetBacklightOnOff failed: %1%", turned_on.error());
            return false;
        }
        auto level = DisplayHelper::call_function_sync<double>(
                         DisplayHelper::FunctionId::GetBacklightBrightness,
                         static_cast<double>(output.id),
                         service::helper::Timeout(SERVICE_TIMEOUT_MS)
                     );
        if (!level) {
            BROOKESIA_LOGE("GetBacklightBrightness failed: %1%", level.error());
            return false;
        }
        brightness = static_cast<int>(*level);
        BROOKESIA_LOGI(
            "Display output %1% backlight is on at persisted brightness %2%", output.id, brightness
        );
        return true;
    }
    BROOKESIA_LOGE("Display has no backlight output");
    return false;
}

bool set_backlight_brightness(uint32_t output_id, int brightness)
{
    auto changed = DisplayHelper::call_function_sync(
                       DisplayHelper::FunctionId::SetBacklightBrightness,
                       static_cast<double>(output_id),
                       static_cast<double>(brightness),
                       service::helper::Timeout(SERVICE_TIMEOUT_MS)
                   );
    if (!changed) {
        BROOKESIA_LOGE("SetBacklightBrightness(%1%) failed: %2%", brightness, changed.error());
        return false;
    }
    return true;
}

bool run_brightness_probe(uint32_t output_id, int saved_brightness)
{
    bool written = false;
    lib_utils::FunctionGuard restore([&]() {
        if (!written) {
            return;
        }
        if (!set_backlight_brightness(output_id, saved_brightness)) {
            BROOKESIA_LOGE("Failed to restore brightness to %1%", saved_brightness);
            return;
        }
        BROOKESIA_LOGI("Restored brightness to the saved value %1%", saved_brightness);
    });

    const int next_brightness = saved_brightness == 60 ? 45 : 60;
    if (!set_backlight_brightness(output_id, next_brightness)) {
        return false;
    }
    written = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    BROOKESIA_LOGI(
        "Service-side brightness set to %1%. The Settings UI log above should show the same value",
        next_brightness
    );

    auto rejected = DisplayHelper::call_function_sync(
                        DisplayHelper::FunctionId::SetBacklightBrightness,
                        999.0,
                        40.0,
                        service::helper::Timeout(SERVICE_TIMEOUT_MS)
                    );
    if (rejected) {
        BROOKESIA_LOGE("Expected an unknown output id to be rejected");
        return false;
    }
    BROOKESIA_LOGI("Unknown output brightness call failed as expected: %1%", rejected.error());
    return true;
}

#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
#   include "wifi_probe.ipp"
#endif

void run_main()
{
    static auto panel = hal::acquire_interface<hal::display::PanelIface>(hal::DisplayDevice::LCD_PANEL_IMPL_NAME);
    static auto touch = hal::acquire_interface<hal::display::TouchIface>(hal::DisplayDevice::LCD_TOUCH_IMPL_NAME);
    static auto backlight = hal::acquire_interface<hal::display::BacklightIface>(
                                hal::DisplayDevice::LEDC_BACKLIGHT_IMPL_NAME
                            );
    if (!panel || !touch || !backlight) {
        BROOKESIA_LOGE("Failed to acquire panel, touch, or backlight");
        return;
    }
    const auto &info = panel->get_info();
    if ((info.h_res != EXPECTED_H_RES) || (info.v_res != EXPECTED_V_RES)) {
        BROOKESIA_LOGE("Panel is %1%x%2%, Settings is 1024x600", info.h_res, info.v_res);
        return;
    }

    // GSP reads the touchscreen in this example. Set that before Display starts.
    service::Display::set_touch_collection_owner(service::Display::TouchCollectionOwner::External);

    static ShellSystem system_instance;
    system::core::System::Config config;
    config.gsp_panel = & *panel;
    config.gsp_touch = & *touch;
    config.environment.width_px = EXPECTED_H_RES;
    config.environment.height_px = EXPECTED_V_RES;
    config.start_service_manager = true;
    config.install_registered_apps = false;
    config.install_package_apps = false;
    auto init_result = system_instance.init(std::move(config));
    if (!init_result) {
        BROOKESIA_LOGE("System init failed: %1%", init_result.error());
        return;
    }
    auto start_result = system_instance.start();
    if (!start_result) {
        BROOKESIA_LOGE("System start failed: %1%", start_result.error());
        return;
    }

    static service::ServiceBinding storage_binding = service::ServiceManager::get_instance().bind(
                StorageHelper::get_name().data()
            );
    if (!storage_binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Storage; Display cannot keep backlight state");
        return;
    }

    static auto launcher = std::make_shared<LauncherApp>();
    static auto settings = std::make_shared<app::settings::SettingsApp>();
    auto launcher_id = system_instance.install_app(launcher);
    auto settings_id = system_instance.install_app(settings);
    if (!launcher_id || !settings_id) {
        BROOKESIA_LOGE(
            "Failed to install apps: launcher(%1%), settings(%2%)",
            launcher_id ? "ok" : launcher_id.error(),
            settings_id ? "ok" : settings_id.error()
        );
        return;
    }
    shell_state.launcher_id = launcher_id.value();
    shell_state.settings_id = settings_id.value();

    // Keep Display running for the whole example. Closing Settings also
    // releases its own use of Display, and that must not stop the service
    // or reset the brightness.
    static service::ServiceBinding display_binding = service::ServiceManager::get_instance().bind(
                DisplayHelper::get_name().data()
            );
    if (!display_binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind Display");
        return;
    }
    uint32_t output_id = 0;
    int brightness = 0;
    if (!prepare_backlight(output_id, brightness)) {
        return;
    }

    if (RUN_BRIGHTNESS_PROBE) {
        if (!run_app(system_instance, shell_state.settings_id, "settings", true)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        // Open the display page, where the brightness slider lives.
        system::core::AppContext settings_context(system_instance, shell_state.settings_id);
        auto opened = settings->on_action(settings_context, "0:1:0");
        if (!opened) {
            BROOKESIA_LOGE("Failed to open the display page: %1%", opened.error());
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!run_brightness_probe(output_id, brightness)) {
            return;
        }
        if (!run_app(system_instance, shell_state.settings_id, "settings", false)) {
            return;
        }
    } else {
        BROOKESIA_LOGI("Brightness probe is off; persisted brightness stays %1%", brightness);
    }

#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
    if (!run_wifi_probe(system_instance, shell_state.settings_id, *panel)) {
        BROOKESIA_LOGE("Wi-Fi probe FAILED");
        return;
    }
#endif

    for (int cycle = 1; cycle <= WARMUP_CYCLES; ++cycle) {
        if (!run_app(system_instance, shell_state.settings_id, "settings", true) ||
                !run_app(system_instance, shell_state.settings_id, "settings", false)) {
            return;
        }
        BROOKESIA_LOGI("Settings warmup cycle %1% finished", cycle);
    }
    const auto heap_after_warmup = sample_heap();
    log_heap("after warmup", heap_after_warmup);

    HeapSample heap_after_measured = heap_after_warmup;
    for (int cycle = 1; cycle <= MEASURED_CYCLES; ++cycle) {
        if (!run_app(system_instance, shell_state.settings_id, "settings", true)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!run_app(system_instance, shell_state.settings_id, "settings", false)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        heap_after_measured = sample_heap();
        char label[32];
        std::snprintf(label, sizeof(label), "cycle %d", cycle);
        log_heap(label, heap_after_measured);
    }
    const auto internal_delta = static_cast<int64_t>(heap_after_measured.internal_free) -
                                static_cast<int64_t>(heap_after_warmup.internal_free);
    const auto psram_delta = static_cast<int64_t>(heap_after_measured.psram_free) -
                             static_cast<int64_t>(heap_after_warmup.psram_free);
    BROOKESIA_LOGI(
        "Settings open/close trend after warmup: cycles(%1%), internal_free_delta(%2%), psram_free_delta(%3%)",
        MEASURED_CYCLES, internal_delta, psram_delta
    );

    shell_state.handoff.store(true);
    if (!run_app(system_instance, shell_state.launcher_id, "launcher", true)) {
        return;
    }
#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
    if (!probe_launcher_round_trip()) {
        BROOKESIA_LOGE("Wi-Fi probe launcher round trip FAILED");
        return;
    }
#endif
    BROOKESIA_LOGI("Interactive launcher is running. Confirm the panel visually; serial logs do not prove the picture");
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

} // namespace

} // namespace esp_brookesia::gsp_app

extern "C" void app_main(void)
{
    try {
        esp_brookesia::gsp_app::run_main();
    } catch (const std::exception &exception) {
        BROOKESIA_LOGE("Unhandled exception: %1%", exception.what());
    } catch (...) {
        BROOKESIA_LOGE("Unhandled unknown exception");
    }
}
