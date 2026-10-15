/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// The probe injects pointer input through GSP, so actions still run on the App task.
uint32_t probe_key(std::string_view name)
{
    uint32_t key = 2166136261U;
    for (const auto character : name) {
        key = (key ^ static_cast<uint8_t>(character)) * 16777619U;
    }
    return key;
}

esp_gsp_handle_t probe_handle()
{
    auto *host = gui::gsp::AppHost::current();
    return host ? host->handle() : nullptr;
}

bool probe_enabled(std::string_view name)
{
    bool enabled = false;
    return esp_gsp_component_get_enabled(probe_handle(), probe_key(name), &enabled) == ESP_GSP_OK && enabled;
}

bool probe_click(int x, int y)
{
    auto handle = probe_handle();
    if (!handle || esp_gsp_inject_touch(handle, x, y, true) != ESP_GSP_OK) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    if (esp_gsp_inject_touch(handle, x, y, false) != ESP_GSP_OK) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return true;
}

bool probe_wait(const std::function<bool()> &ready, int timeout_ms = 15000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    BROOKESIA_LOGE("Wi-Fi probe timed out");
    return false;
}

void probe_frame(hal::display::PanelIface &panel)
{
    (void)esp_gsp_flush(probe_handle(), 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    std::array<void *, 3> buffers{};
    hal::display::PanelIface::DriverSpecific driver;
    if (!panel.get_driver_specific(driver) || driver.frame_buffer_count != buffers.size() ||
            esp_lcd_dpi_panel_get_frame_buffer(static_cast<esp_lcd_panel_handle_t>(driver.panel_handle),
                    buffers.size(), &buffers[0], &buffers[1], &buffers[2]) != ESP_OK) {
        BROOKESIA_LOGW("Wi-Fi probe framebuffer unavailable");
        return;
    }
    // Include all presentation buffers; inactive buffers can contain an earlier frame.
    for (size_t frame = 0; frame < buffers.size(); ++frame) {
        const auto *pixels = static_cast<const uint16_t *>(buffers[frame]);
        std::printf("WIFI_FRAME_BEGIN %zu 512 300\n", frame);
        for (int y = 0; y < 600; y += 2) {
            std::string line = "WIFI_PIX ";
            line.reserve(2058);
            for (int x = 0; x < 1024; x += 2) {
                char hex[5];
                std::snprintf(hex, sizeof(hex), "%04x", pixels[y * 1024 + x]);
                line += hex;
            }
            std::puts(line.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        std::puts("WIFI_FRAME_END");
    }
}

bool probe_refresh_real_scan(const std::shared_ptr<std::atomic<unsigned>> &results)
{
    using namespace wifi_probe_hooks;
    const auto previous_results = results->load();
    const auto previous_nearby = status_count(Status::Nearby);
    const auto previous_empty = status_count(Status::Empty);
    if (!probe_click(890, 108)) {
        return false;
    }
    const auto recovered = [&]() {
        return results->load() == previous_results + 1 && probe_enabled("refresh") &&
               (status_count(Status::Nearby) > previous_nearby || status_count(Status::Empty) > previous_empty);
    };
    if (!probe_wait(recovered)) {
        BROOKESIA_LOGE("Wi-Fi probe real-scan recovery failed");
        return false;
    }
    std::puts("WIFI_RECOVERY_PASS");
    return true;
}

bool probe_fault_case(wifi_probe_hooks::Fault fault, const char *name, wifi_probe_hooks::Status expected_status,
                      const std::shared_ptr<std::atomic<unsigned>> &results,
                      const std::shared_ptr<std::atomic<unsigned>> &empty_results,
                      hal::display::PanelIface &panel)
{
    using namespace wifi_probe_hooks;
    const auto previous_injections = injection_count(fault);
    const auto previous_status = status_count(expected_status);
    const auto previous_results = results->load();
    const auto previous_empty = empty_results->load();
    if (!arm(fault)) {
        BROOKESIA_LOGE("Wi-Fi probe could not arm %1%", name);
        return false;
    }
    lib_utils::FunctionGuard disarm_guard([]() {
        disarm();
    });
    if (!probe_click(890, 108)) {
        return false;
    }
    const auto settled = [&]() {
        return injection_count(fault) == previous_injections + 1 &&
               status_count(expected_status) > previous_status && probe_enabled("refresh");
    };
    if (!probe_wait(settled)) {
        BROOKESIA_LOGE("Wi-Fi probe %1% did not settle", name);
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const bool has_expected_result = fault == Fault::Empty ?
                                     (results->load() == previous_results + 1 &&
                                      empty_results->load() == previous_empty + 1) :
                                     (results->load() == previous_results &&
                                      empty_results->load() == previous_empty);
    if (fault == Fault::ScanFailure && !clear_driver_scan_list()) {
        BROOKESIA_LOGE("Wi-Fi probe %1% could not clear the scan list", name);
        return false;
    }
    if (!has_expected_result || !cleanup_succeeded()) {
        BROOKESIA_LOGE("Wi-Fi probe %1% result or driver cleanup mismatch", name);
        return false;
    }
    disarm();
    std::printf("WIFI_CASE,%s\n", name);
    probe_frame(panel);
    std::printf("WIFI_%s_PASS\n", name);
    return probe_refresh_real_scan(results);
}

bool probe_unicode_case(const std::shared_ptr<std::atomic<unsigned>> &results,
                        hal::display::PanelIface &panel)
{
    using namespace wifi_probe_hooks;
    const auto previous_injections = injection_count(Fault::UnicodeAps);
    const auto previous_results = results->load();
    const auto previous_nearby = status_count(Status::Nearby);
    std::array<unsigned, 3> previous_rows{};
    for (size_t row = 0; row < previous_rows.size(); ++row) {
        previous_rows[row] = unicode_row_count(row);
    }
    if (!arm(Fault::UnicodeAps)) {
        return false;
    }
    lib_utils::FunctionGuard disarm_guard([]() {
        disarm();
    });
    if (!probe_click(890, 108)) {
        return false;
    }
    const auto settled = [&]() {
        if (injection_count(Fault::UnicodeAps) != previous_injections + 1 ||
                results->load() != previous_results + 1 || status_count(Status::Nearby) <= previous_nearby ||
                !probe_enabled("refresh")) {
            return false;
        }
        for (size_t row = 0; row < previous_rows.size(); ++row) {
            if (unicode_row_count(row) <= previous_rows[row]) {
                return false;
            }
        }
        return true;
    };
    if (!probe_wait(settled) || !cleanup_succeeded()) {
        BROOKESIA_LOGE("Wi-Fi probe Unicode AP result did not reach Settings rows");
        return false;
    }
    disarm();
    std::puts("WIFI_CASE,unicode_ssid");
    // This line exercises glyph rendering only; the service has not connected to this AP.
    std::puts("WIFI_SYNTHETIC_CONNECTION_TEXT");
    if (esp_gsp_component_set_text(probe_handle(), probe_key("connection_status"),
                                   "Connected: 中文热点_嗨嗯") != ESP_GSP_OK) {
        return false;
    }
    probe_frame(panel);
    std::puts("WIFI_UNICODE_SSID_PASS");
    return probe_refresh_real_scan(results);
}

bool run_wifi_probe(system::core::System &system, system::core::AppId settings_id, hal::display::PanelIface &panel)
{
    using WifiHelper = service::helper::Wifi;
    auto results = std::make_shared<std::atomic<unsigned>>(0);
    auto empty_results = std::make_shared<std::atomic<unsigned>>(0);
    const service::EventRegistry::SignalSlot scan_callback =
    [results, empty_results](const std::string &, const service::EventItemMap & items) {
        const auto found = items.find("ApInfos");
        if (found == items.end()) {
            return;
        }
        const auto *aps = std::get_if<boost::json::array>(&found->second);
        if (aps) {
            results->fetch_add(1);
            if (aps->empty()) {
                empty_results->fetch_add(1);
            }
            BROOKESIA_LOGI("Wi-Fi probe service result: %1% APs", aps->size());
        }
    };
    auto connection = WifiHelper::subscribe_event(WifiHelper::EventId::ScanApInfosUpdated, scan_callback);
    if (!connection.connected()) {
        BROOKESIA_LOGE("Wi-Fi probe cannot subscribe to service");
        return false;
    }
    const auto initial = WifiHelper::call_function_sync<std::string>(
                             WifiHelper::FunctionId::GetGeneralState, service::helper::Timeout(SERVICE_TIMEOUT_MS)
                         );
    if (!initial) {
        return false;
    }
    const bool initially_enabled = *initial == "Started" || *initial == "Connected" || *initial == "Connecting";
    if (!run_app(system, settings_id, "settings", true) || !probe_click(512, 194)) {
        return false;
    }
    if (!initially_enabled && !probe_click(140, 108)) {
        return false;
    }
    const auto first_scan_ready = [&]() {
        return results->load() > 0 && probe_enabled("refresh");
    };
    if (!probe_wait(first_scan_ready)) {
        return false;
    }
    std::puts("WIFI_CASE,nearby");
    probe_frame(panel);
    if (probe_enabled("next")) {
        if (!probe_click(900, 522) || !probe_enabled("previous")) {
            return false;
        }
        std::puts("WIFI_CASE,next_page");
        probe_frame(panel);
        if (!probe_click(110, 522)) {
            return false;
        }
        std::puts("WIFI_PAGINATION_PASS");
    } else {
        std::puts("WIFI_PAGINATION_SKIPPED,not_enough_networks");
    }
    if (!probe_fault_case(wifi_probe_hooks::Fault::Empty, "empty", wifi_probe_hooks::Status::Empty,
                          results, empty_results, panel) ||
            !probe_fault_case(wifi_probe_hooks::Fault::CountFailure, "count_failure",
                              wifi_probe_hooks::Status::Failed, results, empty_results, panel) ||
            !probe_fault_case(wifi_probe_hooks::Fault::ScanFailure, "scan_failure",
                              wifi_probe_hooks::Status::Failed, results, empty_results, panel) ||
            !probe_unicode_case(results, panel)) {
        return false;
    }
    HeapSample baseline;
    HeapSample last;
    std::array < HeapSample, WARMUP_CYCLES + MEASURED_CYCLES > samples{};
    for (int cycle = 1; cycle <= WARMUP_CYCLES + MEASURED_CYCLES; ++cycle) {
        auto previous = results->load();
        if (!probe_click(890, 108)) {
            return false;
        }
        const auto scan_ready = [&]() {
            return results->load() > previous && probe_enabled("refresh");
        };
        if (!probe_wait(scan_ready)) {
            return false;
        }
        // Leave while a new scan is active, then return and require a fresh result.
        if (!probe_click(890, 108) || !probe_click(60, 37) || !probe_click(512, 194)) {
            return false;
        }
        previous = results->load();
        if (!probe_wait(scan_ready)) {
            return false;
        }
        if (cycle <= WARMUP_CYCLES || cycle % 5 == 0) {
            if (!probe_click(140, 108)) {
                return false;
            }
            const auto off = [&]() {
                return probe_enabled("wifi_toggle") && !probe_enabled("refresh");
            };
            if (!probe_wait(off) || !probe_click(140, 108)) {
                return false;
            }
            previous = results->load();
            if (!probe_wait(scan_ready)) {
                return false;
            }
        }
        if (!probe_click(60, 37) || !run_app(system, settings_id, "settings", false)) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        last = sample_heap();
        samples[cycle - 1] = last;
        char label[48];
        std::snprintf(label, sizeof(label), "wifi cycle %d", cycle);
        log_heap(label, last);
        std::printf("WIFI_HEAP,%d,%zu,%zu,%zu,%zu\n", cycle, last.internal_free, last.internal_largest,
                    last.psram_free, last.psram_largest);
        if (cycle == WARMUP_CYCLES) {
            baseline = last;
        }
        previous = results->load();
        if (!run_app(system, settings_id, "settings", true) || !probe_click(512, 194) || !probe_wait(scan_ready)) {
            return false;
        }
    }
    // Repeat the complete table after the burst of lifecycle logs has settled.
    for (size_t index = 0; index < samples.size(); ++index) {
        const auto &sample = samples[index];
        std::printf("WIFI_RECORD,%zu,%zu,%zu,%zu,%zu\n", index + 1, sample.internal_free, sample.internal_largest,
                    sample.psram_free, sample.psram_largest);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::printf("WIFI_PROBE_PASS,%d,%lld,%lld\n", MEASURED_CYCLES,
                static_cast<long long>(last.internal_free) - static_cast<long long>(baseline.internal_free),
                static_cast<long long>(last.psram_free) - static_cast<long long>(baseline.psram_free));
    if (!initially_enabled) {
        if (!probe_click(140, 108)) {
            return false;
        }
        const auto off = [&]() {
            return probe_enabled("wifi_toggle") && !probe_enabled("refresh");
        };
        if (!probe_wait(off)) {
            return false;
        }
    }
    if (!probe_click(60, 37) || !probe_click(512, 262)) {
        return false;
    }
    int32_t brightness = -1;
    if (esp_gsp_component_get_value(probe_handle(), probe_key("brightness_slider"), &brightness) != ESP_GSP_OK) {
        return false;
    }
    BROOKESIA_LOGI("Wi-Fi probe brightness regression: %1%", brightness);
    uint32_t output_id = 0;
    int saved_brightness = 0;
    if (!prepare_backlight(output_id, saved_brightness)) {
        return false;
    }
    lib_utils::FunctionGuard restore_brightness([&]() {
        (void)set_backlight_brightness(output_id, saved_brightness);
    });
    if (!probe_click(saved_brightness < 50 ? 558 : 238, 393)) {
        return false;
    }
    const auto brightness_changed = [&]() {
        int32_t value = -1;
        const auto level = DisplayHelper::call_function_sync<double>(
                               DisplayHelper::FunctionId::GetBacklightBrightness,
                               static_cast<double>(output_id), service::helper::Timeout(SERVICE_TIMEOUT_MS)
                           );
        return level && esp_gsp_component_get_value(probe_handle(), probe_key("brightness_slider"), &value) ==
               ESP_GSP_OK && value != saved_brightness && value == static_cast<int>(*level);
    };
    if (!probe_wait(brightness_changed)) {
        return false;
    }
    std::puts("WIFI_CASE,brightness");
    probe_frame(panel);
    if (!set_backlight_brightness(output_id, saved_brightness)) {
        return false;
    }
    std::printf("WIFI_BRIGHTNESS_PASS,restored=%d\n", saved_brightness);
    if (!probe_click(60, 37)) {
        return false;
    }
    return run_app(system, settings_id, "settings", false);
}


bool probe_has_component(std::string_view name)
{
    esp_gsp_component_info_t info{};
    auto handle = probe_handle();
    return handle && esp_gsp_component_get_info(handle, probe_key(name), &info) == ESP_GSP_OK;
}

bool probe_launcher_round_trip(hal::display::PanelIface &panel)
{
    std::puts("WIFI_CASE,launcher");
    probe_frame(panel);
    for (int cycle = 0; cycle < 2; ++cycle) {
        if (!probe_has_component("open_settings") || !probe_click(198, 211) ||
                !probe_wait([]() { return probe_has_component("open_wifi"); }, 3000)) {
            return false;
        }
        if (cycle == 0) {
            std::puts("WIFI_CASE,settings_home");
            probe_frame(panel);
        }
        if (!probe_click(920, 37) ||
                !probe_wait([]() { return probe_has_component("open_settings"); }, 3000)) {
            return false;
        }
    }
    std::puts("WIFI_LAUNCHER_ROUND_TRIP_PASS");
    return true;
}
