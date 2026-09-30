/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Include after wifi_probe.ipp. This optional probe runs on app_main, never on the GSP render task.
namespace connect_probe {

constexpr int INPUT_TIMEOUT_MS = 120000;
constexpr int CONNECTION_TIMEOUT_MS = 75000;
constexpr size_t MAX_INPUT_BYTES = 256;
constexpr std::array<std::string_view, 4> LETTER_ROWS = {
    "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm",
};
constexpr std::array<std::string_view, 4> SYMBOL_ROWS = {
    "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>?", "~`/+=_-",
};

struct Credentials {
    std::string ssid;
    std::string password;
};

void clear_password(std::string &password)
{
    std::fill(password.begin(), password.end(), '\0');
    password.clear();
}

bool keyboard_can_type(std::string_view password)
{
    for (const unsigned char value : password) {
        if (value == ' ') {
            continue;
        }
        bool supported = false;
        for (const auto row : LETTER_ROWS) {
            if (row.find(static_cast<char>(std::tolower(value))) != std::string_view::npos) {
                supported = true;
                break;
            }
        }
        for (const auto row : SYMBOL_ROWS) {
            if (row.find(static_cast<char>(value)) != std::string_view::npos) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            return false;
        }
    }
    return true;
}

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
        BROOKESIA_LOGE("Wi-Fi connect probe cannot install temporary serial driver");
        return false;
    }
    lib_utils::FunctionGuard uninstall_driver([]() {
        if (usb_serial_jtag_driver_uninstall() != ESP_OK) {
            BROOKESIA_LOGE("Wi-Fi connect probe cannot uninstall temporary serial driver");
        }
    });
    std::array<char, MAX_INPUT_BYTES> input{};
    lib_utils::FunctionGuard erase_input([&]() {
        std::fill(input.begin(), input.end(), '\0');
    });
    size_t length = 0;
    bool line_complete = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(INPUT_TIMEOUT_MS);
    std::puts("CONNECT_PROBE_READY");
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
                line_complete = true;
                break;
            }
            continue;
        }
        if (length == input.size()) {
            BROOKESIA_LOGE("Wi-Fi connect probe input exceeds %1% bytes", input.size());
            return false;
        }
        input[length++] = character;
    }
    if (!line_complete) {
        BROOKESIA_LOGE("Wi-Fi connect probe input timed out");
        return false;
    }

    boost::system::error_code error;
    auto parsed = boost::json::parse(boost::json::string_view(input.data(), length), error);
    if (error || !parsed.is_object()) {
        BROOKESIA_LOGE("Wi-Fi connect probe input is not a JSON object");
        return false;
    }
    auto &object = parsed.as_object();
    const auto ssid = object.find("ssid");
    const auto password = object.find("password");
    if (ssid == object.end() || !ssid->value().is_string() || password == object.end() ||
            !password->value().is_string()) {
        BROOKESIA_LOGE("Wi-Fi connect probe input requires string ssid and password fields");
        return false;
    }
    credentials.ssid = std::string(ssid->value().as_string());
    credentials.password = std::string(password->value().as_string());
    std::fill(password->value().as_string().begin(), password->value().as_string().end(), '\0');
    if (credentials.ssid.empty() || credentials.ssid.size() > 32 ||
            credentials.password.size() < 8 || credentials.password.size() > 63 ||
            !keyboard_can_type(credentials.password)) {
        BROOKESIA_LOGE("Wi-Fi connect probe needs a 1-32-byte SSID and an 8-63-byte keyboard-supported password");
        return false;
    }
    std::puts("CONNECT_PROBE_INPUT_ACCEPTED");
    accepted = true;
    return true;
}

std::optional<std::pair<int, int>> key_center(char character, bool symbols)
{
    const auto &rows = symbols ? SYMBOL_ROWS : LETTER_ROWS;
    for (size_t row = 0; row < rows.size(); ++row) {
        const auto column = rows[row].find(character);
        if (column == std::string_view::npos) {
            continue;
        }
        const int x = static_cast<int>((row == 2 ? 84 : row == 3 ? 178 : 36) + 96 * column + 44);
        const int y = static_cast<int>(164 + 69 * row + 31);
        return std::pair{x, y};
    }
    return std::nullopt;
}

bool type_password(std::string_view password)
{
    enum class Mode { Lower, Upper, Symbols };
    Mode mode = Mode::Lower;
    for (const char character : password) {
        if (character == ' ') {
            if (!probe_click(458, 475)) {
                return false;
            }
            continue;
        }
        const bool upper = character >= 'A' && character <= 'Z';
        const char lower_character = upper ? static_cast<char>(character - 'A' + 'a') : character;
        const bool symbols = !key_center(lower_character, false).has_value();
        if (symbols && mode != Mode::Symbols) {
            if (!probe_click(125, 475)) {
                return false;
            }
            mode = Mode::Symbols;
        } else if (!symbols) {
            if (mode == Mode::Symbols) {
                if (!probe_click(125, 475)) {
                    return false;
                }
                mode = Mode::Lower;
            }
            if (upper != (mode == Mode::Upper)) {
                if (!probe_click(107, 402)) {
                    return false;
                }
                mode = upper ? Mode::Upper : Mode::Lower;
            }
        }
        const auto center = key_center(symbols ? character : lower_character, symbols);
        if (!center || !probe_click(center->first, center->second)) {
            return false;
        }
    }
    return true;
}

bool current_state_is(std::string_view expected)
{
    using WifiHelper = service::helper::Wifi;
    const auto state = WifiHelper::call_function_sync<std::string>(
                           WifiHelper::FunctionId::GetGeneralState,
                           service::helper::Timeout(SERVICE_TIMEOUT_MS)
                       );
    return state && *state == expected;
}

bool connected_to(std::string_view ssid)
{
    using WifiHelper = service::helper::Wifi;
    if (!current_state_is("Connected")) {
        return false;
    }
    const auto connected_ssid = WifiHelper::call_function_sync<std::string>(
                                    WifiHelper::FunctionId::GetConnectedSsid,
                                    service::helper::Timeout(SERVICE_TIMEOUT_MS)
                                );
    return connected_ssid && *connected_ssid == ssid;
}

bool open_wifi(system::core::System &system, system::core::AppId settings_id,
               const std::shared_ptr<std::atomic<unsigned>> &scan_count)
{
    const auto previous = scan_count->load();
    if (!run_app(system, settings_id, "settings", true) || !probe_click(450, 130)) {
        return false;
    }
    if (current_state_is("Idle") || current_state_is("Inited")) {
        if (!probe_click(140, 108)) {
            return false;
        }
    }
    if (probe_wait([&]() {
        return scan_count->load() > previous && probe_enabled("refresh");
    }, 20000)) {
        return true;
    }
    const auto retry_previous = scan_count->load();
    return probe_enabled("refresh") && probe_click(890, 108) && probe_wait([&]() {
        return scan_count->load() > retry_previous && probe_enabled("refresh");
    }, 20000);
}

bool select_target_row()
{
    for (int page = 0; page < 10; ++page) {
        const int row = wifi_probe_hooks::matching_target_row();
        if (row >= 0) {
            const auto key = std::string("network_") + std::to_string(row);
            if (!probe_enabled(key)) {
                return false;
            }
            return probe_click(512, 207 + 61 * row);
        }
        if (!probe_enabled("next")) {
            return false;
        }
        wifi_probe_hooks::reset_target_row();
        if (!probe_click(900, 522)) {
            return false;
        }
    }
    return false;
}

bool open_password_keyboard()
{
    return probe_has_component("edit_password") && probe_enabled("edit_password") &&
           probe_click(843, 329) && probe_wait([]() {
        return probe_has_component("masked_password") && probe_has_component("key_0_0");
    }, 3000);
}

std::optional<bool> target_is_saved(std::string_view target)
{
    using WifiHelper = service::helper::Wifi;
    const auto saved = WifiHelper::call_function_sync<boost::json::array>(
                           WifiHelper::FunctionId::GetConnectedAps,
                           service::helper::Timeout(SERVICE_TIMEOUT_MS)
                       );
    std::vector<WifiHelper::ConnectApInfo> aps;
    if (!saved || !BROOKESIA_DESCRIBE_FROM_JSON(*saved, aps)) {
        return std::nullopt;
    }
    bool found = false;
    for (auto &ap : aps) {
        found = found || ap.ssid == target;
        clear_password(ap.password);
    }
    return found;
}

bool prepare_saved_row(std::string_view ssid, const std::shared_ptr<std::atomic<unsigned>> &scan_count)
{
    using WifiHelper = service::helper::Wifi;
    const auto setup_failed = [](const char *stage) {
        BROOKESIA_LOGE("Saved-row probe setup failed at %1%", stage);
        return false;
    };
    const auto wait_stable_state = []() -> std::optional<std::string> {
        constexpr auto SETTLE_TIME = std::chrono::seconds(3);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(CONNECTION_TIMEOUT_MS);
        std::string candidate;
        auto candidate_since = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() < deadline) {
            const auto state = WifiHelper::call_function_sync<std::string>(
                                   WifiHelper::FunctionId::GetGeneralState,
                                   service::helper::Timeout(SERVICE_TIMEOUT_MS));
            const auto now = std::chrono::steady_clock::now();
            if (state && (*state == "Started" || *state == "Connected")) {
                if (*state != candidate) {
                    candidate = *state;
                    candidate_since = now;
                } else if (now - candidate_since >= SETTLE_TIME) {
                    return candidate;
                }
            } else {
                candidate.clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        return std::nullopt;
    };
    const auto before_refresh = wifi_probe_hooks::wifi_display_snapshot();
    if (current_state_is("Idle") || current_state_is("Inited")) {
        if (!probe_enabled("wifi_toggle") || !probe_click(140, 108)) {
            return setup_failed("ui_start");
        }
    }
    const auto settled_state = wait_stable_state();
    if (!settled_state) {
        return setup_failed("wait_stable_start_or_connect");
    }
    if (*settled_state == "Connected") {
        if (!probe_wait([]() { return probe_enabled("disconnect"); }, 10000)) {
            return setup_failed("ui_disconnect_enabled");
        }
        if (!probe_click(890, 154)) {
            return setup_failed("ui_disconnect_click");
        }
        const auto disconnected_state = wait_stable_state();
        if (!disconnected_state || *disconnected_state != "Started") {
            return setup_failed("wait_stable_after_disconnect");
        }
    }
    if (!probe_wait([]() { return probe_enabled("refresh"); }, 20000)) {
        return setup_failed("ui_refresh_enabled");
    }
    const auto previous_scans = scan_count->load();
    wifi_probe_hooks::reset_target_row();
    if (!probe_click(890, 108)) {
        return setup_failed("ui_refresh_click");
    }
    if (!probe_wait([&]() {
        return scan_count->load() > previous_scans && probe_enabled("refresh");
    }, 20000)) {
        return setup_failed("fresh_scan_result");
    }
    const auto display = wifi_probe_hooks::wifi_display_snapshot();
    if (display.status_updates <= before_refresh.status_updates ||
            display.connection_updates <= before_refresh.connection_updates ||
            display.wifi_off || display.status_connecting || display.connection_connecting) {
        return setup_failed("settled_list_render");
    }
    if (!target_is_saved(ssid).value_or(false)) {
        return setup_failed("saved_target_lookup");
    }
    return true;
}

bool run_saved_external_stop_probe(std::string_view ssid,
                                   const std::shared_ptr<std::atomic<unsigned>> &scan_count)
{
    using WifiHelper = service::helper::Wifi;
    constexpr int STOP_CYCLES = 3;
    for (int cycle = 1; cycle <= STOP_CYCLES; ++cycle) {
        // A rapid successful connection can beat the probe. Retry only that untested attempt.
        bool stopped_pending_connection = false;
        for (int attempt = 1; attempt <= 3 && !stopped_pending_connection; ++attempt) {
            if (!prepare_saved_row(ssid, scan_count)) {
                return false;
            }
            std::printf("CONNECT_PROBE_DIRECT_STOP_RESTART_READY,%d,%u\n", cycle, scan_count->load());
            const auto before_row = wifi_probe_hooks::wifi_display_snapshot();
            if (!select_target_row() || !probe_wait([&]() {
                const auto display = wifi_probe_hooks::wifi_display_snapshot();
                return display.target_connecting_updates > before_row.target_connecting_updates &&
                       display.status_connecting && display.connection_connecting;
            }, 5000)) {
                return false;
            }
            const auto before_stop = wifi_probe_hooks::wifi_display_snapshot();
            if (current_state_is("Connected")) {
                std::printf("CONNECT_PROBE_DIRECT_STOP_RETRY,%d,%d,connected_before_stop\n", cycle, attempt);
                continue;
            }
            // Stop is queued by the service; the connection may still finish before Stop executes.
            if (!before_stop.status_connecting || !before_stop.connection_connecting) {
                BROOKESIA_LOGE("Saved-row probe lost the rendered Connecting state before Stop");
                return false;
            }
            std::printf("CONNECT_PROBE_DIRECT_STOP_PENDING,%d,%d\n", cycle, attempt);
            if (!WifiHelper::call_function_sync(WifiHelper::FunctionId::TriggerGeneralAction, "Stop",
                                                service::helper::Timeout(SERVICE_TIMEOUT_MS))) {
                BROOKESIA_LOGE("Saved-row probe could not externally stop Wi-Fi");
                return false;
            }
            if (!probe_wait([&]() {
                const auto display = wifi_probe_hooks::wifi_display_snapshot();
                return (current_state_is("Inited") || current_state_is("Idle")) &&
                       display.status_updates > before_stop.status_updates &&
                       display.connection_updates > before_stop.connection_updates &&
                       display.wifi_off && !display.status_connecting &&
                       !display.connection_connecting && !probe_enabled("refresh");
            }, 15000)) {
                BROOKESIA_LOGE("Saved-row probe kept Connecting or failed to render Off after external Stop");
                return false;
            }
            stopped_pending_connection = true;
        }
        if (!stopped_pending_connection) {
            BROOKESIA_LOGE("Saved-row probe never caught a pending connection in cycle %1%", cycle);
            return false;
        }
        std::printf("CONNECT_PROBE_DIRECT_STOP_CYCLE_PASS,%d\n", cycle);
    }
    if (!prepare_saved_row(ssid, scan_count)) {
        return false;
    }
    const auto previous_connected_text = wifi_probe_hooks::connected_target_count();
    if (!select_target_row() || !probe_wait([&]() {
        return connected_to(ssid) &&
               wifi_probe_hooks::connected_target_count() > previous_connected_text;
    }, CONNECTION_TIMEOUT_MS)) {
        BROOKESIA_LOGE("Saved-row probe did not reconnect through the refreshed list");
        return false;
    }
    std::puts("CONNECT_PROBE_DIRECT_STOP_PASS");
    return true;
}

} // namespace connect_probe

bool run_wifi_connect_probe(system::core::System &system, system::core::AppId settings_id,
                            hal::display::PanelIface &panel, connect_probe::Credentials &credentials)
{
    using namespace connect_probe;
    using WifiHelper = service::helper::Wifi;
    lib_utils::FunctionGuard clear_credentials([&]() {
        clear_password(credentials.password);
        wifi_probe_hooks::clear_target_ssid();
    });
    wifi_probe_hooks::track_target_ssid(credentials.ssid);
    auto scan_count = std::make_shared<std::atomic<unsigned>>(0);
    auto connect_actions = std::make_shared<std::atomic<unsigned>>(0);
    auto target_seen = std::make_shared<std::atomic<bool>>(false);
    auto target_locked = std::make_shared<std::atomic<bool>>(false);
    auto target_enterprise = std::make_shared<std::atomic<bool>>(false);
    const service::EventRegistry::SignalSlot scan_callback =
    [scan_count, target_seen, target_locked, target_enterprise, ssid = credentials.ssid](
        const std::string &, const service::EventItemMap & items
    ) {
        const auto found = items.find("ApInfos");
        if (found == items.end()) {
            return;
        }
        const auto *array = std::get_if<boost::json::array>(&found->second);
        if (!array) {
            return;
        }
        std::vector<WifiHelper::ScanApInfo> aps;
        if (BROOKESIA_DESCRIBE_FROM_JSON(*array, aps)) {
            for (const auto &ap : aps) {
                if (ap.ssid == ssid) {
                    target_seen->store(true);
                    target_locked->store(ap.is_locked);
                    target_enterprise->store(ap.is_enterprise);
                }
            }
        }
        scan_count->fetch_add(1);
    };
    auto connection = WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated, scan_callback);
    const service::EventRegistry::SignalSlot action_callback = [connect_actions](
        const std::string &, const service::EventItemMap & items
    ) {
        const auto action = items.find("Action");
        if (action == items.end()) {
            return;
        }
        const auto *value = std::get_if<std::string>(&action->second);
        if (value && *value == BROOKESIA_DESCRIBE_TO_STR(WifiHelper::GeneralAction::Connect)) {
            connect_actions->fetch_add(1);
        }
    };
    auto action_connection = WifiHelper::subscribe_event(
                                 WifiHelper::EventId::GeneralActionTriggered, action_callback);
    if (!connection.connected() || !action_connection.connected() ||
            !open_wifi(system, settings_id, scan_count)) {
        return false;
    }
    if (!target_seen->load() || !target_locked->load() || target_enterprise->load()) {
        std::puts("CONNECT_PROBE_SKIP,target_not_supported_or_not_visible");
        return run_app(system, settings_id, "settings", false);
    }
    const auto initially_saved = target_is_saved(credentials.ssid);
    if (!initially_saved) {
        BROOKESIA_LOGE("Wi-Fi connect probe could not inspect saved APs");
        return false;
    }
    if (*initially_saved) {
        std::puts("CONNECT_PROBE_DIRECT_STOP_PROBE,saved_fast_path");
        return run_saved_external_stop_probe(credentials.ssid, scan_count) &&
               run_app(system, settings_id, "settings", false);
    }
    if (connected_to(credentials.ssid)) {
        std::puts("CONNECT_PROBE_SKIP,already_connected_unsaved");
        return run_app(system, settings_id, "settings", false);
    }

    // Exercise keyboard cancellation and app teardown before a successful connection saves the AP.
    const auto connects_before_cancellation = connect_actions->load();
    for (int cycle = 1; cycle <= 8; ++cycle) {
        if (!select_target_row() || !open_password_keyboard() || !probe_click(80, 195) ||
                !probe_click(107, 402) || !probe_click(80, 264) ||
                !probe_click(125, 475) || !probe_click(80, 195)) {
            return false;
        }
        if (cycle == 1) {
            std::puts("WIFI_CASE masked_keyboard");
            probe_frame(panel);
        }
        if (cycle % 2 == 0) {
            if (!probe_click(778, 475) || !probe_wait([]() {
                return probe_has_component("edit_password");
            }, 3000) || !probe_click(60, 37)) {
                return false;
            }
        }
        if (!run_app(system, settings_id, "settings", false)) {
            return false;
        }
        if (cycle <= 2) {
            std::printf("CONNECT_PROBE_WARMUP,%d,%u\n", cycle, scan_count->load());
        } else {
            const auto sample = sample_heap();
            std::printf("CONNECT_PROBE_HEAP,%d,%zu,%zu,%zu,%zu,%u\n", cycle, sample.internal_free,
                        sample.internal_largest, sample.psram_free, sample.psram_largest, scan_count->load());
        }
        if (!open_wifi(system, settings_id, scan_count)) {
            return false;
        }
        if (connect_actions->load() != connects_before_cancellation) {
            BROOKESIA_LOGE("Wi-Fi connect probe cancellation submitted a Connect action");
            return false;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (connect_actions->load() != connects_before_cancellation) {
        BROOKESIA_LOGE("Wi-Fi connect probe cancellation submitted a delayed Connect action");
        return false;
    }
    std::puts("CONNECT_PROBE_CANCEL_NO_CONNECT");

    if (!select_target_row() || !open_password_keyboard()) {
        return false;
    }
    std::string wrong_password = credentials.password;
    wrong_password.front() = wrong_password.front() == 'a' ? 'b' : 'a';
    const bool typed_wrong = type_password(wrong_password);
    clear_password(wrong_password);
    if (!typed_wrong || !probe_click(928, 475) || !probe_enabled("connect") || !probe_click(837, 515)) {
        return false;
    }
    if (!probe_wait([]() { return !probe_enabled("edit_password"); }, 5000) ||
            !probe_wait([&]() {
        return probe_enabled("edit_password") || connected_to(credentials.ssid);
    }, CONNECTION_TIMEOUT_MS) || connected_to(credentials.ssid)) {
        BROOKESIA_LOGE("Wi-Fi connect probe wrong-password attempt did not fail");
        return false;
    }
    std::puts("CONNECT_PROBE_WRONG_PASSWORD_FAILED");
    std::puts("WIFI_CASE wrong_password");
    probe_frame(panel);

    if (!open_password_keyboard() || !type_password(credentials.password) ||
            !probe_click(928, 475) || !probe_enabled("connect")) {
        return false;
    }
    if (!probe_click(837, 515) || !probe_wait([&]() {
        return current_state_is("Connecting") || connected_to(credentials.ssid);
    }, 10000)) {
        return false;
    }
    const bool interrupted_while_connecting = current_state_is("Connecting");
    if (!interrupted_while_connecting && !connected_to(credentials.ssid)) {
        return false;
    }
    if (!probe_click(60, 37) || !probe_click(60, 37) ||
            !run_app(system, settings_id, "settings", false) ||
            !open_wifi(system, settings_id, scan_count)) {
        return false;
    }
    if (!probe_wait([&]() {
        return connected_to(credentials.ssid) && wifi_probe_hooks::connected_target_count() > 0;
    }, CONNECTION_TIMEOUT_MS)) {
        return false;
    }
    std::puts(interrupted_while_connecting ? "CONNECT_PROBE_BACK_EXIT_CONNECTED" :
              "CONNECT_PROBE_BACK_EXIT_FAST_CONNECTED");
    std::puts("WIFI_CASE connected");
    probe_frame(panel);

    if (!probe_wait([&]() { return target_is_saved(credentials.ssid).value_or(false); }, 10000) ||
            !probe_enabled("disconnect") || !probe_click(890, 154) ||
            !probe_wait([]() { return current_state_is("Started"); }, 15000)) {
        return false;
    }
    const auto previous_scans = scan_count->load();
    if (!probe_enabled("refresh") || !probe_click(890, 108) || !probe_wait([&]() {
        return scan_count->load() > previous_scans && probe_enabled("refresh");
    }, 20000) || !target_is_saved(credentials.ssid).value_or(false)) {
        return false;
    }
    if (connected_to(credentials.ssid)) {
        std::puts("CONNECT_PROBE_AUTO_RECONNECTED");
    } else {
        const auto previous_connected_text = wifi_probe_hooks::connected_target_count();
        if (!select_target_row() || !probe_wait([&]() {
            return connected_to(credentials.ssid) &&
                   wifi_probe_hooks::connected_target_count() > previous_connected_text;
        }, CONNECTION_TIMEOUT_MS)) {
            return false;
        }
    }
    const auto final_sample = sample_heap();
    std::printf("CONNECT_PROBE_FINAL_HEAP,%zu,%zu,%zu,%zu,%u\n", final_sample.internal_free,
                final_sample.internal_largest, final_sample.psram_free, final_sample.psram_largest,
                scan_count->load());
    std::puts("CONNECT_PROBE_DIRECT_STOP_PROBE,after_full_connect");
    if (!run_saved_external_stop_probe(credentials.ssid, scan_count) ||
            !run_app(system, settings_id, "settings", false)) {
        return false;
    }
    std::puts("CONNECT_PROBE_PASS");
    return true;
}
