/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Include after lvgl_probe.ipp. The Settings and Super keyboard are driven through Display touch injection.
#include <algorithm>
#include <array>
#include <utility>

#include "boost/json.hpp"
#include "driver/usb_serial_jtag.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix_private.h"

namespace esp_brookesia::gsp_app::lvgl_connect_probe {

namespace base = esp_brookesia::gsp_app::lvgl_probe;

constexpr int INPUT_TIMEOUT_MS = 120000;
constexpr int CONNECTION_TIMEOUT_MS = 75000;
constexpr int CYCLES = 6;
constexpr int WARMUP_CYCLES = 2;
constexpr size_t MAX_INPUT_BYTES = 256;
constexpr std::string_view WIFI_CONNECTED_PATH = "/wifi/page/wifi_content/connected";
constexpr std::string_view SAVED_PARENT = "/wifi/page/wifi_content/saved_networks/list/items";
constexpr std::string_view AVAILABLE_PARENT = "/wifi/page/wifi_content/available_networks/list/items";
constexpr std::string_view SAVED_PREV = "/wifi/page/wifi_content/saved_networks/list/pager/prev";
constexpr std::string_view SAVED_NEXT = "/wifi/page/wifi_content/saved_networks/list/pager/next";
constexpr std::string_view AVAILABLE_PREV = "/wifi/page/wifi_content/available_networks/list/pager/prev";
constexpr std::string_view AVAILABLE_NEXT = "/wifi/page/wifi_content/available_networks/list/pager/next";
constexpr std::string_view PASSWORD_ROW = "/wifi_connect/page/password_card/password_row";
constexpr std::string_view CONNECT_BUTTON = "/wifi_connect/page/actions/connect";
constexpr std::string_view CANCEL_BUTTON = "/wifi_connect/page/actions/cancel";
constexpr std::string_view KEYBOARD_PATH = "/keyboard_input/keyboard_panel/keyboard";
constexpr std::string_view KEYBOARD_TEXT_PATH = "/keyboard_input/input_bar/input";
constexpr std::string_view KEYBOARD_FLOW = "keyboard";
constexpr std::string_view SUPPORTED_PUNCTUATION = "+.,\"<>= $[]~@#!%&*()'/-_:;?";

struct Credentials {
    std::string ssid;
    std::string password;
};

void clear_password(std::string &password)
{
    std::fill(password.begin(), password.end(), '\0');
    password.clear();
}

bool supported_password(std::string_view password)
{
    return std::all_of(password.begin(), password.end(), [](unsigned char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z') ||
               SUPPORTED_PUNCTUATION.find(static_cast<char>(character)) != std::string_view::npos;
    });
}

// Called by main_lvgl before any service starts consuming USB console input.
bool read_credentials(Credentials &credentials)
{
    bool accepted = false;
    lib_utils::FunctionGuard clear_rejected_password([&]() {
        if (!accepted) {
            clear_password(credentials.password);
        }
    });
    auto driver_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&driver_config) != ESP_OK) {
        BROOKESIA_LOGE("LVGL connect probe cannot install temporary serial driver");
        return false;
    }
    lib_utils::FunctionGuard uninstall_driver([]() {
        if (usb_serial_jtag_driver_uninstall() != ESP_OK) {
            BROOKESIA_LOGE("LVGL connect probe cannot uninstall temporary serial driver");
        }
    });
    std::array<char, MAX_INPUT_BYTES> input{};
    lib_utils::FunctionGuard erase_input([&]() {
        std::fill(input.begin(), input.end(), '\0');
    });
    size_t length = 0;
    bool complete = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(INPUT_TIMEOUT_MS);
    std::puts("LVGL_CONNECT_PROBE_READY");
    std::fflush(stdout);
    while (std::chrono::steady_clock::now() < deadline) {
        char character = '\0';
        const auto count = usb_serial_jtag_read_bytes(&character, 1, 0);
        if (count != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (character == '\n' || character == '\r') {
            if (length > 0) {
                complete = true;
                break;
            }
            continue;
        }
        if (length == input.size()) {
            BROOKESIA_LOGE("LVGL connect probe input is too long");
            return false;
        }
        input[length++] = character;
    }
    if (!complete) {
        BROOKESIA_LOGE("LVGL connect probe input timed out");
        return false;
    }
    boost::system::error_code error;
    auto parsed = boost::json::parse(boost::json::string_view(input.data(), length), error);
    if (error || !parsed.is_object()) {
        BROOKESIA_LOGE("LVGL connect probe input must be a JSON object");
        return false;
    }
    auto &object = parsed.as_object();
    auto ssid = object.find("ssid");
    auto password = object.find("password");
    if (ssid == object.end() || !ssid->value().is_string() || password == object.end() ||
            !password->value().is_string()) {
        BROOKESIA_LOGE("LVGL connect probe input requires string ssid and password");
        return false;
    }
    credentials.ssid = std::string(ssid->value().as_string());
    credentials.password = std::string(password->value().as_string());
    std::fill(password->value().as_string().begin(), password->value().as_string().end(), '\0');
    if (credentials.ssid.empty() || credentials.ssid.size() > 32 ||
            credentials.password.size() < 8 || credentials.password.size() > 63 ||
            !supported_password(credentials.password)) {
        BROOKESIA_LOGE("LVGL connect probe requires a 1-32-byte SSID and an 8-63-byte keyboard-supported password");
        return false;
    }
    std::puts("LVGL_CONNECT_PROBE_INPUT_ACCEPTED");
    accepted = true;
    return true;
}

std::optional<lv_obj_t *> find_object(std::string_view path)
{
    for (auto *display = lv_display_get_next(nullptr); display != nullptr; display = lv_display_get_next(display)) {
        const std::array roots = {
            lv_display_get_screen_active(display), lv_display_get_layer_bottom(display),
            lv_display_get_layer_top(display), lv_display_get_layer_sys(display),
        };
        for (auto *root : roots) {
            if (auto *object = base::find_named_object(root, path); object != nullptr) {
                return object;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> label_text(std::string_view path)
{
    gui::lvgl::lock_thread();
    lib_utils::FunctionGuard unlock([]() { gui::lvgl::unlock_thread(); });
    auto object = find_object(path);
    if (!object || !lv_obj_check_type(*object, &lv_label_class)) {
        return std::nullopt;
    }
    return std::string(lv_label_get_text(*object));
}

bool view_enabled(std::string_view path)
{
    gui::lvgl::lock_thread();
    lib_utils::FunctionGuard unlock([]() { gui::lvgl::unlock_thread(); });
    auto object = find_object(path);
    return object && !lv_obj_has_state(*object, LV_STATE_DISABLED) &&
           !lv_obj_has_flag(*object, LV_OBJ_FLAG_HIDDEN);
}

std::optional<std::pair<int, int>> key_center(std::string_view label)
{
    gui::lvgl::lock_thread();
    lib_utils::FunctionGuard unlock([]() { gui::lvgl::unlock_thread(); });
    auto object = find_object(KEYBOARD_PATH);
    if (!object || !lv_obj_check_type(*object, &lv_keyboard_class)) {
        return std::nullopt;
    }
    lv_obj_update_layout(*object);
    lv_area_t frame;
    lv_obj_get_coords(*object, &frame);
    const auto *matrix = reinterpret_cast<const lv_buttonmatrix_t *>(*object);
    for (uint32_t button = 0; button < matrix->btn_cnt; ++button) {
        const char *text = lv_buttonmatrix_get_button_text(*object, button);
        if (text == nullptr || label != text ||
                lv_buttonmatrix_has_button_ctrl(*object, button, LV_BUTTONMATRIX_CTRL_DISABLED)) {
            continue;
        }
        const auto &area = matrix->button_areas[button];
        return std::pair{frame.x1 + (area.x1 + area.x2) / 2,
                         frame.y1 + (area.y1 + area.y2) / 2};
    }
    return std::nullopt;
}

bool tap_key(const base::Context &context, std::string_view label)
{
    auto center = key_center(label);
    if (!center) {
        BROOKESIA_LOGE("LVGL connect probe keyboard key unavailable");
        return false;
    }
    return base::tap(context, center->first, center->second);
}

bool type_password(const base::Context &context, std::string_view password)
{
    for (const char character : password) {
        const std::string key(1, character);
        if (key_center(key)) {
            if (!tap_key(context, key)) {
                return false;
            }
            continue;
        }
        if (character >= 'A' && character <= 'Z') {
            if (key_center("abc") && !tap_key(context, "abc")) {
                return false;
            }
            if (!tap_key(context, "ABC")) {
                return false;
            }
        } else if (character >= 'a' && character <= 'z') {
            if (!tap_key(context, "abc")) {
                return false;
            }
        } else if (character >= '0' && character <= '9') {
            if (!tap_key(context, "123")) {
                return false;
            }
        } else if (character == ' ') {
            if (!key_center("Space") && !tap_key(context, "abc")) {
                return false;
            }
        } else if (!tap_key(context, ",.?!")) {
            return false;
        }
        if (!tap_key(context, character == ' ' ? std::string_view("Space") : std::string_view(key))) {
            return false;
        }
    }
    gui::lvgl::lock_thread();
    lib_utils::FunctionGuard unlock([]() { gui::lvgl::unlock_thread(); });
    auto input = find_object(KEYBOARD_TEXT_PATH);
    return input && password == lv_textarea_get_text(*input);
}

bool wait_keyboard(const base::Context &context, bool shown)
{
    return base::wait_for([&]() {
        auto state = context.system.gui_get_screen_flow_state(context.shell_id, KEYBOARD_FLOW);
        return state && *state == (shown ? "keyboard_input" : "keyboard_hidden");
    }, base::PAGE_TIMEOUT_MS, shown ? "keyboard input" : "keyboard hidden");
}

bool tap_pager(const base::Context &context, std::string_view button, std::string_view label)
{
    auto previous_page = label_text(label);
    if (!base::tap_path(context, context.settings_id, button, true)) {
        return false;
    }
    return !previous_page || base::wait_for([&]() {
        auto current_page = label_text(label);
        return current_page && *current_page != *previous_page;
    }, base::PAGE_TIMEOUT_MS, "Wi-Fi pager update");
}

std::optional<std::string> find_network_row(const base::Context &context, std::string_view parent,
                                             std::string_view prefix, std::string_view previous,
                                             std::string_view next, std::string_view ssid, bool diagnose = false)
{
    const auto separator = next.rfind('/');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto pager_label = std::string(next.substr(0, separator)) + "/page";
    for (int page = 0; page < 20 && view_enabled(previous); ++page) {
        if (!tap_pager(context, previous, pager_label)) {
            return std::nullopt;
        }
    }
    for (int page = 0; page < 20; ++page) {
        if (diagnose) {
            std::printf("LVGL_CONNECT_PAGE,%.*s,%d,%d\n", static_cast<int>(prefix.size()), prefix.data(),
                        page, view_enabled(next) ? 1 : 0);
        }
        for (int row = 0; row < 5; ++row) {
            auto path = std::string(parent) + "/" + std::string(prefix) + "_" + std::to_string(row);
            auto shown = label_text(path + "/text/ssid");
            if (diagnose) {
                auto runtime_view = context.system.gui_get_view_frame(context.settings_id, path + "/text/ssid");
                std::string safe_text = shown.value_or("<missing>");
                std::replace_if(safe_text.begin(), safe_text.end(), [](char character) {
                    return character == '\n' || character == '\r';
                }, ' ');
                std::printf("LVGL_CONNECT_ROW,%d,%d,%d,%s\n", page, row, runtime_view ? 1 : 0,
                            safe_text.c_str());
            }
            if (shown && *shown == ssid) {
                return path;
            }
        }
        if (!view_enabled(next) || !tap_pager(context, next, pager_label)) {
            break;
        }
    }
    return std::nullopt;
}

bool long_press_path(const base::Context &context, std::string_view path)
{
    auto scrolled = context.system.gui_scroll_to_view(context.settings_id, path, false);
    if (!scrolled) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto frame = base::absolute_frame(context, context.settings_id, path);
    if (!frame) {
        return false;
    }
    auto &service = service::Display::get_instance();
    if (!service.inject_touch(context.display.output_name(), frame->x + frame->width / 2,
                              frame->y + frame->height / 2)) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(950));
    return base::release_touch(context);
}

std::optional<bool> target_saved(std::string_view ssid)
{
    using WifiHelper = service::helper::Wifi;
    auto aps = WifiHelper::call_function_sync<boost::json::array>(
                   WifiHelper::FunctionId::GetConnectedAps, service::helper::Timeout(base::SERVICE_TIMEOUT_MS));
    if (!aps) {
        return std::nullopt;
    }
    for (const auto &entry : *aps) {
        if (!entry.is_object()) {
            continue;
        }
        const auto *name = entry.as_object().if_contains("ssid");
        if (name && name->is_string() && name->as_string() == ssid) {
            return true;
        }
    }
    return false;
}

bool connected_to(std::string_view ssid)
{
    using WifiHelper = service::helper::Wifi;
    auto state = WifiHelper::call_function_sync<std::string>(
                     WifiHelper::FunctionId::GetGeneralState, service::helper::Timeout(base::SERVICE_TIMEOUT_MS));
    if (!state || *state != "Connected") {
        return false;
    }
    auto actual = WifiHelper::call_function_sync<std::string>(
                      WifiHelper::FunctionId::GetConnectedSsid, service::helper::Timeout(base::SERVICE_TIMEOUT_MS));
    return actual && *actual == ssid;
}

std::optional<std::string> wait_wifi_settled(std::string_view reason)
{
    std::optional<std::string> candidate;
    auto stable_since = std::chrono::steady_clock::now();
    if (!base::wait_for([&]() {
    auto state = base::wifi_state();
    if (!state || (*state != "Started" && *state != "Connected" && *state != "Inited" && *state != "Idle")) {
        candidate.reset();
        return false;
    }
    if (candidate != state) {
        candidate = std::move(state);
        stable_since = std::chrono::steady_clock::now();
        return false;
    }
    return std::chrono::steady_clock::now() - stable_since >= std::chrono::milliseconds(1500);
}, CONNECTION_TIMEOUT_MS, reason)) {
        return std::nullopt;
    }
    return candidate;
}

bool open_wifi(const base::Context &context)
{
    if (!base::open_settings(context) ||
            !base::tap_path(context, context.settings_id, base::HOME_WIFI_PATH, true) ||
            !base::wait_page(context, "wifi")) {
        return false;
    }
    auto state = wait_wifi_settled("initial Wi-Fi state");
    if (!state) {
        return false;
    }
    if (*state == "Inited" || *state == "Idle") {
        if (!base::tap_path(context, context.settings_id, base::WIFI_SWITCH_PATH) ||
                !(state = wait_wifi_settled("Wi-Fi start")) ||
                (*state != "Started" && *state != "Connected")) {
            return false;
        }
    }
    return true;
}

bool ensure_scan_ready(const base::Context &context)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto state = wait_wifi_settled("Wi-Fi connection to settle before scan");
        if (!state) {
            return false;
        }
        if (*state == "Started") {
            return true;
        }
        if (*state != "Connected") {
            BROOKESIA_LOGE("LVGL connect probe Wi-Fi stopped before scan");
            return false;
        }
        // Disconnect through Settings after boot auto-reconnect completes, regardless of SSID.
        if (!base::tap_path(context, context.settings_id, WIFI_CONNECTED_PATH, true) ||
                !base::wait_for([&]() {
        auto current = base::wifi_state();
        return current && *current == "Started";
    },
                                CONNECTION_TIMEOUT_MS, "Wi-Fi disconnect before scan")) {
            return false;
        }
    }
    BROOKESIA_LOGE("LVGL connect probe Wi-Fi kept reconnecting before scan");
    return false;
}

bool forget_target_via_ui(const base::Context &context, std::string_view ssid, bool require_saved)
{
    auto saved = target_saved(ssid);
    if (!saved) {
        BROOKESIA_LOGE("LVGL connect probe could not read saved Wi-Fi networks");
        return false;
    }
    if (require_saved && !*saved && !base::wait_for([&]() {
    saved = target_saved(ssid);
    return saved && *saved;
}, base::PAGE_TIMEOUT_MS, "newly connected temporary AP to be saved")) {
        return false;
    }
    if (!*saved) {
        std::puts("LVGL_CONNECT_FORGET_SKIPPED_UNSAVED");
        return true;
    }
    std::optional<std::string> saved_row;
    if (!base::wait_for([&]() {
    saved_row = find_network_row(context, SAVED_PARENT, "saved", SAVED_PREV, SAVED_NEXT, ssid);
    return saved_row.has_value();
}, base::PAGE_TIMEOUT_MS, "temporary AP in saved list") ||
            !long_press_path(context, *saved_row) ||
            !base::wait_for([&]() {
    auto still_saved = target_saved(ssid);
    return still_saved && !*still_saved;
}, base::PAGE_TIMEOUT_MS, "forget temporary AP")) {
        BROOKESIA_LOGE("LVGL connect probe could not forget the temporary AP through Settings");
        return false;
    }
    std::puts("LVGL_CONNECT_FORGET_PASS");
    return true;
}

bool run(system::super::System &system, ::Display &display, Credentials &credentials)
{
    auto panel = hal::acquire_first_interface<hal::display::PanelIface>();
    if (!panel) {
        BROOKESIA_LOGE("LVGL connect probe panel unavailable");
        return false;
    }
    base::Context context{.system = system, .display = display, .panel = *panel};
    for (const auto &app : system.list_apps()) {
        if (app.manifest.id == base::SHELL_ID) {
            context.shell_id = app.app_id;
        } else if (app.manifest.id == base::SETTINGS_ID) {
            context.settings_id = app.app_id;
        }
    }
    if (context.shell_id == system::core::INVALID_APP_ID ||
            context.settings_id == system::core::INVALID_APP_ID) {
        BROOKESIA_LOGE("LVGL connect probe Shell or Settings is missing");
        return false;
    }
    auto &manager = service::ServiceManager::get_instance();
    auto wifi_binding = manager.bind(service::helper::Wifi::get_name().data());
    if (!wifi_binding.is_valid()) {
        BROOKESIA_LOGE("LVGL connect probe Wi-Fi service unavailable");
        return false;
    }
    context.scan_results = std::make_shared<std::atomic<unsigned>>(0);
    context.scan_stops = std::make_shared<std::atomic<unsigned>>(0);
    auto scan_ap_count = std::make_shared<std::atomic<size_t>>(0);
    auto scan_target_seen = std::make_shared<std::atomic<bool>>(false);
    const service::EventRegistry::SignalSlot results_handler =
    [results = context.scan_results, scan_ap_count, scan_target_seen,
     target_ssid = credentials.ssid](const std::string &, const service::EventItemMap & items) {
        const auto found = items.find("ApInfos");
        if (found == items.end()) {
            return;
        }
        const auto *aps = std::get_if<boost::json::array>(&found->second);
        if (!aps) {
            return;
        }
        scan_ap_count->store(aps->size());
        bool target_seen = false;
        for (const auto &ap : *aps) {
            if (!ap.is_object()) {
                continue;
            }
            const auto *ssid = ap.as_object().if_contains("ssid");
            if (ssid && ssid->is_string() &&
                    std::string_view(ssid->as_string().data(), ssid->as_string().size()) == target_ssid) {
                target_seen = true;
                break;
            }
        }
        scan_target_seen->store(target_seen);
        results->fetch_add(1);
    };
    auto results_connection = service::helper::Wifi::subscribe_event(
                                  service::helper::Wifi::EventId::ScanApInfosUpdated, results_handler);
    const service::EventRegistry::SignalSlot stopped_handler =
    [stops = context.scan_stops](const std::string &, const service::EventItemMap & items) {
        const auto found = items.find("IsRunning");
        if (found != items.end()) {
            if (const auto *running = std::get_if<bool>(&found->second); running && !*running) {
                stops->fetch_add(1);
            }
        }
    };
    auto stopped_connection = service::helper::Wifi::subscribe_event(
                                  service::helper::Wifi::EventId::ScanStateChanged, stopped_handler);
    if (!results_connection.connected() || !stopped_connection.connected()) {
        BROOKESIA_LOGE("LVGL connect probe could not observe Wi-Fi scans");
        return false;
    }
    if (!open_wifi(context) || !ensure_scan_ready(context)) {
        return false;
    }
    if (!forget_target_via_ui(context, credentials.ssid, false) ||
            !ensure_scan_ready(context) || !base::refresh_wifi(context)) {
        return false;
    }
    std::printf("LVGL_CONNECT_SCAN_RESULT,%zu,%d\n", scan_ap_count->load(), scan_target_seen->load() ? 1 : 0);
    std::optional<std::string> available_row;
    if (!base::wait_for([&]() {
    available_row = find_network_row(context, AVAILABLE_PARENT, "available", AVAILABLE_PREV,
                                     AVAILABLE_NEXT, credentials.ssid);
    return available_row.has_value();
}, base::PAGE_TIMEOUT_MS, "temporary AP in scan list")) {
        (void)base::capture_frame(context, "wifi_scan_target_missing");
        (void)find_network_row(context, AVAILABLE_PARENT, "available", AVAILABLE_PREV, AVAILABLE_NEXT,
                               credentials.ssid, true);
        BROOKESIA_LOGE("LVGL connect probe target AP is not in the Settings scan list");
        return false;
    }
    const auto baseline = base::sample_heap();
    std::array<base::HeapSample, CYCLES> samples{};
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        if (!base::tap_path(context, context.settings_id, *available_row, true) ||
                !base::wait_page(context, "wifi_connect") ||
                !base::tap_path(context, context.settings_id, PASSWORD_ROW) || !wait_keyboard(context, true) ||
                !type_password(context, "Probe1") || !tap_key(context, "Cancel") ||
                !wait_keyboard(context, false) || !base::wait_page(context, "wifi_connect") ||
                !base::tap_path(context, context.settings_id, CANCEL_BUTTON) ||
                !base::wait_page(context, "wifi")) {
            BROOKESIA_LOGE("LVGL connect probe cancel cycle %1% failed", cycle + 1);
            return false;
        }
        if (connected_to(credentials.ssid)) {
            BROOKESIA_LOGE("LVGL connect probe cancellation unexpectedly connected");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        samples[cycle] = base::sample_heap();
        std::printf("LVGL_CONNECT_HEAP,%d,%zu,%zu,%zu,%zu\n", cycle + 1,
                    samples[cycle].internal_free, samples[cycle].internal_largest,
                    samples[cycle].psram_free, samples[cycle].psram_largest);
    }
    std::puts("LVGL_CONNECT_CANCEL_PASS");
    if (!base::tap_path(context, context.settings_id, *available_row, true) ||
            !base::wait_page(context, "wifi_connect") ||
            !base::tap_path(context, context.settings_id, PASSWORD_ROW) || !wait_keyboard(context, true) ||
            !type_password(context, "Probe1")) {
        BROOKESIA_LOGE("LVGL connect probe could not open the keyboard for external Wi-Fi stop");
        return false;
    }
    using WifiHelper = service::helper::Wifi;
    auto stopped = WifiHelper::call_function_sync(
                       WifiHelper::FunctionId::TriggerGeneralAction,
                       BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Stop),
                       service::helper::Timeout(base::SERVICE_TIMEOUT_MS));
    if (!stopped || !wait_keyboard(context, false) || !base::wait_page(context, "wifi") ||
            !base::wait_for([&]() {
        auto state = base::wifi_state();
        return state && (*state == "Inited" || *state == "Idle");
    }, CONNECTION_TIMEOUT_MS, "external Wi-Fi stop")) {
        BROOKESIA_LOGE("LVGL connect probe external Wi-Fi stop did not close the keyboard and page");
        return false;
    }
    const auto settle_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < settle_until) {
        auto state = base::wifi_state();
        if (!state || (*state != "Inited" && *state != "Idle")) {
            BROOKESIA_LOGE("LVGL connect probe Wi-Fi restarted after external stop");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::puts("LVGL_CONNECT_EXTERNAL_STOP_PASS");
    if (!base::tap_path(context, context.settings_id, base::WIFI_SWITCH_PATH) ||
            !base::wait_for(base::wifi_started, CONNECTION_TIMEOUT_MS, "Wi-Fi start after external stop") ||
            !ensure_scan_ready(context) ||
            !base::wait_for([&]() { return view_enabled(base::WIFI_REFRESH_PATH); },
                            base::SCAN_TIMEOUT_MS, "Wi-Fi refresh enabled after restart") ||
            !base::refresh_wifi(context)) {
        BROOKESIA_LOGE("LVGL connect probe could not restart and rescan Wi-Fi through Settings");
        return false;
    }
    if (!base::wait_for([&]() {
    available_row = find_network_row(context, AVAILABLE_PARENT, "available", AVAILABLE_PREV,
                                     AVAILABLE_NEXT, credentials.ssid);
    return available_row.has_value();
}, base::PAGE_TIMEOUT_MS, "temporary AP after external Wi-Fi restart")) {
        (void)base::capture_frame(context, "wifi_after_external_stop_target_missing");
        BROOKESIA_LOGE("LVGL connect probe target AP missing after external Wi-Fi restart");
        return false;
    }
    if (!base::tap_path(context, context.settings_id, *available_row, true) ||
            !base::wait_page(context, "wifi_connect") ||
            !base::tap_path(context, context.settings_id, PASSWORD_ROW) || !wait_keyboard(context, true) ||
            !base::capture_frame(context, "wifi_password_keyboard") ||
            !type_password(context, credentials.password) || !tap_key(context, "OK") ||
            !wait_keyboard(context, false) || !base::wait_page(context, "wifi_connect") ||
            !base::tap_path(context, context.settings_id, CONNECT_BUTTON) ||
            !base::wait_page(context, "wifi") ||
            !base::wait_for([&]() { return connected_to(credentials.ssid); },
                            CONNECTION_TIMEOUT_MS, "temporary AP connected")) {
        BROOKESIA_LOGE("LVGL connect probe could not connect through Settings");
        return false;
    }
    std::puts("LVGL_CONNECT_REAL_PASS");
    if (!base::capture_frame(context, "wifi_connected") || !base::back_home(context) ||
            !base::close_settings(context) || !open_wifi(context) || !connected_to(credentials.ssid)) {
        BROOKESIA_LOGE("LVGL connect probe did not retain the actual connection after reopening Settings");
        return false;
    }
    for (size_t index = 0; index < samples.size(); ++index) {
        const auto &sample = samples[index];
        std::printf("LVGL_CONNECT_RECORD,%zu,%zu,%zu,%zu,%zu\n", index + 1,
                    sample.internal_free, sample.internal_largest, sample.psram_free, sample.psram_largest);
    }
    const auto &last = samples.back();
    std::printf("LVGL_CONNECT_VERIFIED_PASS,%d,%d,%lld,%lld\n", CYCLES, WARMUP_CYCLES,
                static_cast<long long>(last.internal_free) - static_cast<long long>(baseline.internal_free),
                static_cast<long long>(last.psram_free) - static_cast<long long>(baseline.psram_free));
    // Cleanup happens after the measured and verified result so the next normal GSP run can ask for a password.
    if (!ensure_scan_ready(context) || !forget_target_via_ui(context, credentials.ssid, true) ||
            !base::tap_path(context, context.settings_id, base::WIFI_SWITCH_PATH) ||
            !base::wait_for([&]() {
        auto state = base::wifi_state();
        return state && (*state == "Inited" || *state == "Idle");
    }, CONNECTION_TIMEOUT_MS, "Wi-Fi stop after temporary AP cleanup") ||
            !base::back_home(context) || !base::close_settings(context)) {
        BROOKESIA_LOGE("LVGL connect probe could not clean up the temporary AP through Settings");
        return false;
    }
    std::puts("LVGL_CONNECT_TEST_AP_REMOVED_FOR_MANUAL_INPUT");
    std::printf("LVGL_CONNECT_PROBE_PASS,%d,%d,%lld,%lld\n", CYCLES, WARMUP_CYCLES,
                static_cast<long long>(last.internal_free) - static_cast<long long>(baseline.internal_free),
                static_cast<long long>(last.psram_free) - static_cast<long long>(baseline.psram_free));
    return true;
}

} // namespace esp_brookesia::gsp_app::lvgl_connect_probe
