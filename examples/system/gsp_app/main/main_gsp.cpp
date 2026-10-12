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
#include "brookesia/service_helper/media/audio.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#include "brookesia/system_core.hpp"
#include "esp_heap_caps.h"
#include "launcher_app.hpp"

#if CONFIG_BROOKESIA_GSP_DELIVERY_PROBE
#   include "delivery_probe.hpp"
#endif
#if CONFIG_BROOKESIA_GSP_JS_APP_MEMORY_PROBE
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "esp_gsp_debug.h"
extern "C" uint32_t brookesia_gsp_page_delivery_seq();
#endif
#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
#   include "brookesia/gui_gsp/app_host.hpp"
#   include "brookesia/service_helper/network/wifi.hpp"
#   include "esp_gsp_debug.h"
#   include "esp_lcd_mipi_dsi.h"
#   include "wifi_probe_hooks.hpp"
#endif
#if CONFIG_BROOKESIA_GSP_WIFI_CONNECT_PROBE || CONFIG_BROOKESIA_GSP_SETTINGS_PROBE
#   include <algorithm>
#   include <cctype>
#   include <optional>
#   include <utility>
#   include "boost/json.hpp"
#   include "driver/usb_serial_jtag.h"
#endif

namespace esp_brookesia::gsp_app {

namespace {

constexpr uint16_t EXPECTED_H_RES = 1024;
constexpr uint16_t EXPECTED_V_RES = 600;
constexpr int SERVICE_TIMEOUT_MS = 1000;
#if CONFIG_BROOKESIA_GSP_SETTINGS_MEMORY_PROBE
constexpr int WARMUP_CYCLES = 2;
constexpr int MEASURED_CYCLES = 20;
#endif
// Leave this off for a normal start. Turn it on to change the brightness
// once at boot, then put the saved percent back.
constexpr bool RUN_BRIGHTNESS_PROBE = false;

using DisplayHelper = service::helper::Display;
using StorageHelper = service::helper::Storage;
using SntpHelper = service::helper::SNTP;

struct ShellState {
    std::atomic<bool> handoff{false};
    system::core::AppId launcher_id = system::core::INVALID_APP_ID;
    system::core::AppId settings_id = system::core::INVALID_APP_ID;
    system::core::AppId pending_app_id = system::core::INVALID_APP_ID;
};

ShellState shell_state;

class ShellSystem: public system::core::System {
public:
#if CONFIG_BROOKESIA_GSP_SETTINGS_PROBE
    auto probe_appearance()
    {
        return system_gui().get_theme_language();
    }
#endif
    std::expected<void, std::string> on_start() override
    {
        // Use the same stored preferences as Super; selection takes effect at restart.
        begin_gui_preferences_restore();
        const auto saved_theme = get_stored_gui_theme_id().value_or("light");
        const auto saved_language = get_stored_gui_language().value_or("en");
        const auto theme = saved_theme == "dark" ? "dark" : "light";
        const auto language = saved_language == "zh_CN" || saved_language == "zh" ? "zh_CN" : "en";
        auto result = system_gui().set_theme(theme, false);
        if (result) {
            result = system_gui().set_language(language, false);
        }
        mark_gui_preferences_restored();
        return result;
    }

    void set_launcher(std::shared_ptr<LauncherApp> launcher)
    {
        launcher_ = std::move(launcher);
    }

    void set_settings_keyboard_target(
        const std::shared_ptr<app::settings::SettingsApp> &settings, system::core::AppId app_id
    )
    {
        settings_keyboard_target_ = settings;
        settings_keyboard_app_id_ = app_id;
    }

    std::expected<void, std::string> on_show_app_keyboard(
        system::core::AppId app_id,
        system::core::KeyboardRequestId request_id,
        const system::core::KeyboardRequestOptions &options
    ) override
    {
        if (app_id != settings_keyboard_app_id_ || app_id == system::core::INVALID_APP_ID) {
            return std::unexpected("GSP keyboard is only available in Settings");
        }
        auto settings = settings_keyboard_target_.lock();
        const auto app = get_app(app_id);
        if (!settings || !app || app->state != system::core::AppState::Running) {
            return std::unexpected("Settings is not running");
        }
        const auto complete = [this](const system::core::KeyboardResult & result) {
            auto completed = complete_app_keyboard(
                                 result.app_id, result.request_id, result.confirmed, result.text
                             );
            if (!completed) {
                BROOKESIA_LOGW("Failed to complete GSP keyboard request %1%: %2%",
                               result.request_id, completed.error());
            }
        };
        return settings->show_gsp_keyboard(request_id, options, complete);
    }

    void on_hide_app_keyboard(
        system::core::AppId app_id, system::core::KeyboardRequestId request_id
    ) override
    {
        if (app_id != settings_keyboard_app_id_) {
            return;
        }
        if (auto settings = settings_keyboard_target_.lock()) {
            settings->hide_gsp_keyboard(request_id);
        }
    }

    std::expected<void, std::string> on_app_installed(const system::core::AppInfo &app) override
    {
        if (app.manifest.kind == system::core::AppKind::Runtime) {
            refresh_launcher_slots();
        }
        return {};
    }

    std::expected<void, std::string> on_app_uninstalled(const system::core::AppInfo &app) override
    {
        if (app.manifest.kind == system::core::AppKind::Runtime) {
            refresh_launcher_slots();
        }
        return {};
    }

    void on_app_stopped(const system::core::AppInfo &app) override
    {
        if (!shell_state.handoff.load()) {
            return;
        }
        auto open_app = [this](system::core::AppId app_id, const char *label) {
            const auto started_at = std::chrono::steady_clock::now();
            auto started = start_app(app_id);
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - started_at
                                    ).count();
            if (started) {
                BROOKESIA_LOGI("App open elapsed_ms(%1%) target(%2%) id(%3%)", elapsed_ms, label, app_id);
                return started;
            }
            BROOKESIA_LOGE(
                "App open failed elapsed_ms(%1%) target(%2%) id(%3%): %4%",
                elapsed_ms, label, app_id, started.error()
            );
            return started;
        };
        if (app.app_id == shell_state.launcher_id) {
            const auto next = shell_state.pending_app_id;
            shell_state.pending_app_id = system::core::INVALID_APP_ID;
            const bool open_settings = next == system::core::INVALID_APP_ID;
            const auto target = open_settings ? shell_state.settings_id : next;
            const char *label = open_settings ? "settings" : "app";
            BROOKESIA_LOGI("Launcher stopped; starting %1%", label);
            auto started = open_app(target, label);
            if (!started) {
                BROOKESIA_LOGE("Failed to open %1% after the launcher stopped: %2%", label, started.error());
                auto restored = open_app(shell_state.launcher_id, "launcher");
                if (!restored) {
                    BROOKESIA_LOGE("Failed to restore the launcher: %1%", restored.error());
                }
            }
            return;
        }
        BROOKESIA_LOGI("App stopped; returning to the launcher");
        auto returned = open_app(shell_state.launcher_id, "launcher");
        if (!returned) {
            BROOKESIA_LOGE("Failed to return to the launcher: %1%", returned.error());
        }
    }

    void refresh_launcher_slots()
    {
        if (shell_state.launcher_id == system::core::INVALID_APP_ID) {
            return;
        }
        auto active = get_active_app();
        if (!active || (active->app_id != shell_state.launcher_id) || !launcher_) {
            return;
        }
        auto refreshed = launcher_->refresh_slots();
        if (!refreshed) {
            BROOKESIA_LOGW("Failed to refresh launcher apps: %1%", refreshed.error());
        }
    }

private:
    std::weak_ptr<app::settings::SettingsApp> settings_keyboard_target_;
    std::shared_ptr<LauncherApp> launcher_;
    system::core::AppId settings_keyboard_app_id_ = system::core::INVALID_APP_ID;
};

struct HeapSample {
    size_t internal_free = 0;
    size_t internal_largest = 0;
    size_t psram_free = 0;
    size_t psram_largest = 0;
};

[[maybe_unused]] HeapSample sample_heap()
{
    return {
        .internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        .internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        .psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        .psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
    };
}

[[maybe_unused]] void log_heap(const char *label, const HeapSample &sample)
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
#   if CONFIG_BROOKESIA_GSP_SETTINGS_PROBE
#       include "settings_probe.ipp"
#   endif
#   if CONFIG_BROOKESIA_GSP_WIFI_CONNECT_PROBE
#       include "wifi_connect_probe.ipp"
#   endif
#endif

void run_main()
{
#if CONFIG_BROOKESIA_GSP_WIFI_CONNECT_PROBE
    connect_probe::Credentials test_credentials;
    if (!connect_probe::read_credentials(test_credentials)) {
        return;
    }
    lib_utils::FunctionGuard clear_test_credentials([&]() {
        connect_probe::clear_password(test_credentials.password);
    });
#endif
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
    config.install_package_apps = true;
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

    static service::ServiceBinding playback_binding = service::ServiceManager::get_instance().bind(
                service::helper::AudioPlayback::get_name().data()
            );
    if (!playback_binding.is_valid()) {
        BROOKESIA_LOGW("Audio playback is unavailable; Settings sound controls will be disabled");
    }

    // Super keeps SNTP bound for as long as the shell runs, so the wall clock is set whichever app opens first.
    // The GSP host does the same; without it Clock shows 1970 and alarms never match the real time.
    static service::ServiceBinding sntp_binding;
    if (!SntpHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not available; the wall clock stays unset");
    } else {
        sntp_binding = service::ServiceManager::get_instance().bind(SntpHelper::get_name().data());
        if (!sntp_binding.is_valid()) {
            BROOKESIA_LOGW("Failed to bind SNTP; the wall clock stays unset");
        } else if (auto state = SntpHelper::call_function_sync<std::string>(SntpHelper::FunctionId::GetState);
                   state && (*state == BROOKESIA_DESCRIBE_TO_STR(SntpHelper::State::Stopped))) {
            if (auto started = SntpHelper::call_function_sync<void>(SntpHelper::FunctionId::Start); !started) {
                BROOKESIA_LOGW("Failed to start SNTP: %1%", started.error());
            }
        }
        if (sntp_binding.is_valid()) {
            BROOKESIA_LOGI("SNTP held by the GSP host");
        }
    }

    static auto launcher = std::make_shared<LauncherApp>();
    system_instance.set_launcher(launcher);
    launcher->set_launch_request([](system::core::AppId app_id) {
        shell_state.pending_app_id = app_id;
    });
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
    system_instance.set_settings_keyboard_target(settings, shell_state.settings_id);

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
#   if CONFIG_BROOKESIA_GSP_SETTINGS_PROBE
    if (!settings_probe_load_cost(system_instance, shell_state.settings_id)) {
        BROOKESIA_LOGE("Settings load cost probe FAILED");
        return;
    }
    if (!run_settings_probe(system_instance, shell_state.settings_id, *panel)) {
        BROOKESIA_LOGE("Settings sound and appearance probe FAILED");
        (void)run_app(system_instance, shell_state.settings_id, "settings", false);
    }
#   elif CONFIG_BROOKESIA_GSP_WIFI_CONNECT_PROBE
    if (!run_wifi_connect_probe(system_instance, shell_state.settings_id, *panel, test_credentials)) {
        BROOKESIA_LOGE("Wi-Fi connection probe FAILED");
        return;
    }
#   else
    if (!run_wifi_probe(system_instance, shell_state.settings_id, *panel)) {
        BROOKESIA_LOGE("Wi-Fi probe FAILED");
        return;
    }
#   endif
#endif

#if CONFIG_BROOKESIA_GSP_SETTINGS_MEMORY_PROBE
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
#endif

#if CONFIG_BROOKESIA_GSP_JS_APP_MEMORY_PROBE
    system::core::AppId js_app_id = system::core::INVALID_APP_ID;
    for (const auto &app : system_instance.list_apps()) {
        if (app.manifest.id == "brookesia.gsp.device_control") {
            js_app_id = app.app_id;
            break;
        }
    }
    if (js_app_id == system::core::INVALID_APP_ID) {
        BROOKESIA_LOGE("JS app memory probe did not find brookesia.gsp.device_control");
        return;
    }
    if (!run_app(system_instance, js_app_id, "device_control", true) ||
            !run_app(system_instance, js_app_id, "device_control", false)) {
        return;
    }
    const auto js_heap_after_warmup = sample_heap();
    log_heap("js app after warmup", js_heap_after_warmup);
    HeapSample js_heap_after_cycles = js_heap_after_warmup;
    constexpr int JS_PROBE_CYCLES = 5;
    for (int cycle = 1; cycle <= JS_PROBE_CYCLES; ++cycle) {
        if (!run_app(system_instance, js_app_id, "device_control", true) ||
                !run_app(system_instance, js_app_id, "device_control", false)) {
            return;
        }
        js_heap_after_cycles = sample_heap();
        char label[40];
        std::snprintf(label, sizeof(label), "js app cycle %d", cycle);
        log_heap(label, js_heap_after_cycles);
    }
    BROOKESIA_LOGI(
        "JS app start/exit trend: cycles(%1%), internal_free_delta(%2%), psram_free_delta(%3%)",
        JS_PROBE_CYCLES,
        static_cast<int64_t>(js_heap_after_cycles.internal_free) -
        static_cast<int64_t>(js_heap_after_warmup.internal_free),
        static_cast<int64_t>(js_heap_after_cycles.psram_free) -
        static_cast<int64_t>(js_heap_after_warmup.psram_free)
    );
#endif

    shell_state.handoff.store(true);
    if (!run_app(system_instance, shell_state.launcher_id, "launcher", true)) {
        return;
    }
#if CONFIG_BROOKESIA_GSP_JS_APP_MEMORY_PROBE
    auto elapsed_since = [](std::chrono::steady_clock::time_point started_at) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - started_at
               ).count();
    };
    auto tap_current = [](int x, int y) {
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            BROOKESIA_LOGE("JS app probe has no GSP session");
            return false;
        }
        if (esp_gsp_inject_touch(
                    host->handle(), static_cast<int16_t>(x), static_cast<int16_t>(y), true
                ) != ESP_GSP_OK) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        return esp_gsp_inject_touch(
                   host->handle(), static_cast<int16_t>(x), static_cast<int16_t>(y), false
               ) == ESP_GSP_OK;
    };
    auto slide_current = [](int x, int y, int dx) {
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            BROOKESIA_LOGE("JS app probe has no GSP session");
            return false;
        }
        const auto handle = host->handle();
        if (esp_gsp_inject_touch(handle, static_cast<int16_t>(x), static_cast<int16_t>(y), true) != ESP_GSP_OK) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        if (esp_gsp_inject_touch(
                    handle, static_cast<int16_t>(x + dx), static_cast<int16_t>(y), true
                ) != ESP_GSP_OK) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return esp_gsp_inject_touch(
                   handle, static_cast<int16_t>(x + dx), static_cast<int16_t>(y), false
               ) == ESP_GSP_OK;
    };
    auto wait_until = [&](const char *label, auto ready) {
        const auto started_at = std::chrono::steady_clock::now();
        constexpr int timeout_ms = 3000;
        while (elapsed_since(started_at) < timeout_ms) {
            if (ready()) {
                BROOKESIA_LOGI("Probe observed %1% elapsed_ms(%2%)", label, elapsed_since(started_at));
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        BROOKESIA_LOGE("Probe timed out waiting for %1%", label);
        return false;
    };
    auto active_is = [&](system::core::AppId app_id) {
        auto active = system_instance.get_active_app();
        return active && (active->app_id == app_id);
    };
    auto name_key = [](const char *name) {
        uint32_t key = 2166136261U;
        for (const char *character = name; *character != '\0'; ++character) {
            key = (key ^ static_cast<uint8_t>(*character)) * 16777619U;
        }
        return key;
    };
    const auto slider_key = name_key("brightness_slider");
    BROOKESIA_LOGI("JS app probe tapping the first installed app");
    const auto opened_at = std::chrono::steady_clock::now();
    const bool opened = (js_app_id != system::core::INVALID_APP_ID) && slide_current(164, 500, 20) &&
    wait_until("app open", [&]() {
        return active_is(js_app_id);
    });
    if (opened) {
        BROOKESIA_LOGI("JS app open from tap elapsed_ms(%1%)", elapsed_since(opened_at));
    }
    const auto page_seq_before_brightness = brookesia_gsp_page_delivery_seq();
    const bool brightness_page = opened && tap_current(264, 382) &&
    wait_until("brightness page", [&]() {
        return brookesia_gsp_page_delivery_seq() != page_seq_before_brightness;
    });
    int32_t slider_before = 0;
    if (brightness_page) {
        auto *host = gui::gsp::AppHost::current();
        if ((host != nullptr) && (host->handle() != nullptr)) {
            (void)esp_gsp_component_get_value(host->handle(), slider_key, &slider_before);
        }
    }
    const bool slider_moved = brightness_page && tap_current(800, 284) &&
    wait_until("slider value", [&]() {
        auto *host = gui::gsp::AppHost::current();
        if ((host == nullptr) || (host->handle() == nullptr)) {
            return false;
        }
        int32_t value = slider_before;
        if (esp_gsp_component_get_value(host->handle(), slider_key, &value) != ESP_GSP_OK) {
            return false;
        }
        return value != slider_before;
    });
    const auto page_seq_before_status = brookesia_gsp_page_delivery_seq();
    const bool status_page = slider_moved && tap_current(106, 60) &&
    wait_until("status page", [&]() {
        return brookesia_gsp_page_delivery_seq() != page_seq_before_status;
    });
    const bool exited = status_page && tap_current(638, 382) &&
    wait_until("app exit", [&]() {
        return active_is(shell_state.launcher_id);
    });
    if (exited) {
        BROOKESIA_LOGI("JS app probe finished the status, brightness, back, and exit taps");
    } else {
        BROOKESIA_LOGE("JS app probe tap sequence failed");
    }
#endif
#if CONFIG_BROOKESIA_GSP_WIFI_PROBE
    if (!probe_launcher_round_trip(*panel)) {
        BROOKESIA_LOGE("Wi-Fi probe launcher round trip FAILED");
        return;
    }
#endif
#if CONFIG_BROOKESIA_GSP_DELIVERY_PROBE
    (void)start_delivery_probe(*panel);
#endif
    BROOKESIA_LOGI("Interactive launcher is running. Confirm the panel visually; serial logs do not prove the picture");
#if CONFIG_BROOKESIA_GSP_SETTINGS_PROBE
    if (!settings_probe_navigation_failure()) {
        BROOKESIA_LOGE("Settings navigation failure probe FAILED");
        return;
    }
    settings_probe_console(system_instance, *panel);
#endif
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
