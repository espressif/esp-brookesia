/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Included after wifi_probe.ipp. Touches are injected into GSP; App actions stay on its task.
struct SettingsProbeAudioState {
    double volume = 0;
    bool muted = false;
};

bool settings_probe_read_audio(SettingsProbeAudioState &state)
{
    using AudioPlaybackHelper = service::helper::AudioPlayback;
    if (!AudioPlaybackHelper::is_running()) {
        return false;
    }
    auto volume = AudioPlaybackHelper::call_function_sync<double>(
                      AudioPlaybackHelper::FunctionId::GetVolume,
                      service::helper::Timeout(SERVICE_TIMEOUT_MS)
                  );
    auto muted = AudioPlaybackHelper::call_function_sync<bool>(
                     AudioPlaybackHelper::FunctionId::GetMute,
                     service::helper::Timeout(SERVICE_TIMEOUT_MS)
                 );
    if (!volume || !muted) {
        return false;
    }
    state.volume = *volume;
    state.muted = *muted;
    return true;
}

bool settings_probe_read_brightness(uint32_t output_id, int &brightness)
{
    auto level = DisplayHelper::call_function_sync<double>(
                     DisplayHelper::FunctionId::GetBacklightBrightness,
                     static_cast<double>(output_id), service::helper::Timeout(SERVICE_TIMEOUT_MS)
                 );
    if (!level) {
        BROOKESIA_LOGE("Settings probe could not read brightness: %1%", level.error());
        return false;
    }
    brightness = static_cast<int>(*level);
    return true;
}

bool settings_probe_open_page(int y, std::string_view page_component)
{
    if (!probe_has_component("open_sound") || !probe_click(512, y)) {
        return false;
    }
    return probe_wait([page_component]() { return probe_has_component(page_component); }, 3000);
}

bool settings_probe_back_home()
{
    return probe_click(60, 37) &&
           probe_wait([]() { return probe_has_component("open_sound"); }, 3000);
}

bool settings_probe_visit(int cycle, const char *name, int y, std::string_view page_component,
                          hal::display::PanelIface &panel)
{
    if (!settings_probe_open_page(y, page_component)) {
        BROOKESIA_LOGE("Settings probe could not open %1%", name);
        return false;
    }
    std::printf("SETTINGS_PAGE,%d,%s\n", cycle, name);
    if (cycle == 1) {
        std::printf("SETTINGS_FRAME,%s\n", name);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        probe_frame(panel);
    }
    return settings_probe_back_home();
}

bool settings_probe_load_cost(system::core::System &system, system::core::AppId settings_id)
{
    for (int cycle = 0; cycle < 4; ++cycle) {
        const auto before = sample_heap();
        if (heap_caps_monitor_local_minimum_free_size_start() != ESP_OK) {
            return false;
        }
        lib_utils::FunctionGuard stop_monitor([]() { heap_caps_monitor_local_minimum_free_size_stop(); });
        lib_utils::FunctionGuard close_app([&]() { (void)run_app(system, settings_id, "settings", false); });
        const auto start = std::chrono::steady_clock::now();
        if (!run_app(system, settings_id, "settings", true)) {
            return false;
        }
        const auto started = std::chrono::steady_clock::now();
        if (!probe_wait([]() { return probe_has_component("open_sound"); }, 3000)) {
            return false;
        }
        const auto ready = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto active = sample_heap();
        // IDF sums regional low watermarks; these are conservative peak-use bounds.
        const auto internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        const auto psram_min = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        const auto stopping = std::chrono::steady_clock::now();
        if (!run_app(system, settings_id, "settings", false)) {
            return false;
        }
        close_app.release();
        const auto stopped = std::chrono::steady_clock::now();
        heap_caps_monitor_local_minimum_free_size_stop();
        stop_monitor.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto closed = sample_heap();
        const auto micros = [](auto duration) {
            return static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
        };
        std::printf("SETTINGS_LOAD,%d,%lld,%lld,%lld,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n",
                    cycle, micros(started - start), micros(ready - start), micros(stopped - stopping),
                    before.internal_free, before.internal_largest, before.psram_free, before.psram_largest,
                    active.internal_free, active.internal_largest, active.psram_free, active.psram_largest,
                    internal_min, psram_min,
                    closed.internal_free, closed.internal_largest, closed.psram_free, closed.psram_largest);
    }
    return true;
}

bool run_settings_probe(system::core::System &system, system::core::AppId settings_id,
                        hal::display::PanelIface &panel)
{
    if (!run_app(system, settings_id, "settings", true) ||
            !probe_wait([]() { return probe_has_component("open_sound"); }, 3000)) {
        return false;
    }

    uint32_t output_id = 0;
    int initial_brightness = 0;
    if (!prepare_backlight(output_id, initial_brightness)) {
        return false;
    }
    const auto initial_preference = system.get_environment();
    SettingsProbeAudioState initial_audio;
    const bool audio_available = settings_probe_read_audio(initial_audio);
    std::printf("SETTINGS_BASELINE,brightness=%d,audio=%d,volume=%.2f,mute=%d,theme=%s,language=%s\n",
                initial_brightness, audio_available, initial_audio.volume, initial_audio.muted,
                initial_preference.theme_id.c_str(), initial_preference.language.c_str());
    if (!audio_available) {
        std::puts("SETTINGS_AUDIO_SKIPPED,service_getters_unavailable");
    }
    const auto baseline_heap = sample_heap();
    std::printf("SETTINGS_HEAP_BASELINE,%zu,%zu,%zu,%zu\n", baseline_heap.internal_free,
                baseline_heap.internal_largest, baseline_heap.psram_free, baseline_heap.psram_largest);

    for (int cycle = 1; cycle <= 6; ++cycle) {
        if (cycle == 1) {
            std::puts("SETTINGS_FRAME,home");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            probe_frame(panel);
        }
        if (!settings_probe_visit(cycle, "display", 262, "brightness_slider", panel)) {
            return false;
        }

        if (!settings_probe_open_page(330, "volume_slider")) {
            return false;
        }
        std::printf("SETTINGS_PAGE,%d,sound\n", cycle);
        if (cycle == 1) {
            std::puts("SETTINGS_FRAME,sound");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            probe_frame(panel);
            if (audio_available && !initial_audio.muted && probe_enabled("test_sound")) {
                if (!probe_click(512, 470)) {
                    return false;
                }
                std::puts("SETTINGS_TEST_TONE_SENT,manual_listening_required");
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            } else {
                std::puts("SETTINGS_TEST_TONE_SKIPPED,muted_or_control_unavailable");
            }
            // Changing the slider would round an arbitrary persisted volume. Toggle mute twice instead.
            if (audio_available && probe_enabled("mute_toggle")) {
                bool restore_needed = true;
                lib_utils::FunctionGuard restore_mute([&]() {
                    if (!restore_needed) {
                        return;
                    }
                    SettingsProbeAudioState current;
                    if (settings_probe_read_audio(current) && current.muted != initial_audio.muted &&
                            !probe_click(878, 368)) {
                        BROOKESIA_LOGE("Settings probe emergency mute restoration failed");
                    }
                });
                if (!probe_click(878, 368)) {
                    return false;
                }
                const auto opposite = [&]() {
                    SettingsProbeAudioState state;
                    return settings_probe_read_audio(state) && state.muted != initial_audio.muted;
                };
                if (!probe_wait(opposite, 3000) || !probe_click(878, 368)) {
                    return false;
                }
                const auto restored = [&]() {
                    SettingsProbeAudioState state;
                    return settings_probe_read_audio(state) && state.muted == initial_audio.muted;
                };
                if (!probe_wait(restored, 3000)) {
                    BROOKESIA_LOGE("Settings probe could not restore mute through the UI");
                    return false;
                }
                restore_needed = false;
                std::puts("SETTINGS_MUTE_RESTORED");
            } else {
                std::puts("SETTINGS_MUTE_SKIPPED,control_unavailable");
            }
        }
        if (!settings_probe_back_home() ||
                !settings_probe_visit(cycle, "language", 398, "option_0", panel) ||
                !settings_probe_visit(cycle, "theme", 466, "option_0", panel) ||
                !settings_probe_visit(cycle, "wifi", 194, "wifi_toggle", panel)) {
            return false;
        }

        int brightness = 0;
        if (!settings_probe_read_brightness(output_id, brightness) || brightness != initial_brightness) {
            BROOKESIA_LOGE("Settings probe brightness changed in cycle %1%", cycle);
            return false;
        }
        SettingsProbeAudioState audio;
        if (audio_available && (!settings_probe_read_audio(audio) ||
                                audio.volume != initial_audio.volume || audio.muted != initial_audio.muted)) {
            BROOKESIA_LOGE("Settings probe audio state changed in cycle %1%", cycle);
            return false;
        }
        const auto preference = system.get_environment();
        if (preference.theme_id != initial_preference.theme_id ||
                preference.language != initial_preference.language) {
            BROOKESIA_LOGE("Settings probe active preference changed in cycle %1%", cycle);
            return false;
        }
        const auto heap = sample_heap();
        std::printf("SETTINGS_RECORD,%d,%zu,%zu,%zu,%zu,%d,%.2f,%d\n", cycle,
                    heap.internal_free, heap.internal_largest, heap.psram_free, heap.psram_largest,
                    brightness, audio_available ? audio.volume : initial_audio.volume,
                    audio_available ? audio.muted : initial_audio.muted);
        std::printf("SETTINGS_CYCLE_PASS,%d\n", cycle);
    }

    // Preferences and Wi-Fi selection are deliberately read-only in this phase.
    std::puts("SETTINGS_PREFERENCES_LATER_SKIPPED,no_persistent_selection");
    std::puts("SETTINGS_KEYBOARD_CANCEL_SKIPPED,no_safe_unsaved_secured_ap_target");
    std::puts("SETTINGS_PROBE_PASS,read_only_navigation_brightness_active_preferences");
    return run_app(system, settings_id, "settings", false);
}

bool settings_probe_navigation_failure()
{
    using namespace wifi_probe_hooks;
    lib_utils::FunctionGuard reset_fault([]() { disarm(); });
    const auto failures = text_failure_count();
    const auto sound_after_failure = [failures]() {
        return probe_has_component("volume_slider") && text_failure_count() == failures + 1;
    };
    const auto home_after_failure = [failures]() {
        return probe_has_component("open_sound") && text_failure_count() == failures + 2;
    };
    if (!probe_click(198, 211) ||
            !probe_wait([]() { return probe_has_component("open_sound"); }, 3000) ||
            !arm_text_failure(probe_key("header_back")) || !probe_click(512, 330) ||
            !probe_wait(sound_after_failure, 3000)) {
        return false;
    }
    // Fail the next page's first label as well: Back must commit Home, then Exit must still dispatch.
    if (!arm_text_failure(probe_key("header_title")) || !probe_click(60, 37) ||
            !probe_wait(home_after_failure, 3000) || !probe_click(920, 37) ||
            !probe_wait([]() { return probe_has_component("open_settings"); }, 3000)) {
        return false;
    }
    if (!probe_click(198, 211) ||
            !probe_wait([]() { return probe_has_component("open_sound"); }, 3000) ||
            !probe_click(920, 37) ||
            !probe_wait([]() { return probe_has_component("open_settings"); }, 3000)) {
        return false;
    }
    std::puts("SETTINGS_NAV_FAILURE_PASS,two_label_failures_back_exit_reopen");
    return true;
}

// Optional acceptance console. It dispatches touch coordinates on app_main and
// never runs service waits in the GSP callback or exposes credential contents.
void settings_probe_console(system::core::System &system, hal::display::PanelIface &panel)
{
    // The acceptance console owns RX after the measured run. Normal firmware
    // keeps the USB host bridge; a reset restores it in probe firmware as well.
    auto &manager = service::ServiceManager::get_instance();
    if (!manager.remove_service("Usb")) {
        BROOKESIA_LOGE("Settings probe could not release the USB host bridge");
        return;
    }
    auto config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
        BROOKESIA_LOGE("Settings probe serial console is unavailable");
        return;
    }
    lib_utils::FunctionGuard uninstall([]() { usb_serial_jtag_driver_uninstall(); });
    std::array<char, 128> command{};
    size_t length = 0;
    std::puts("SETTINGS_CONSOLE_READY");
    while (true) {
        char c = '\0';
        if (usb_serial_jtag_read_bytes(&c, 1, 0) != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (c != '\n' && c != '\r') {
            if (length + 1 < command.size()) {
                command[length++] = c;
            }
            continue;
        }
        command[length] = '\0';
        int x = 0;
        int y = 0;
        std::array<char, 64> label{};
        if (std::sscanf(command.data(), "touch %d %d", &x, &y) == 2) {
            std::printf("SETTINGS_TOUCH,%d\n", probe_click(x, y));
        } else if (std::string_view(command.data()) == "frame") {
            probe_frame(panel);
        } else if (std::sscanf(command.data(), "fail_label %63s", label.data()) == 1) {
            const bool armed = wifi_probe_hooks::arm_text_failure(probe_key(label.data()));
            std::printf("SETTINGS_TEXT_FAILURE_ARMED,%d\n", armed);
        } else if (std::string_view(command.data()) == "busy_tone") {
            const service::helper::Audio::PlayUrlConfig playback{
                .interrupt = false,
                .loop_count = 5,
                .only_if_idle = true,
            };
            const auto result = service::helper::AudioPlayback::call_function_sync(
                                    service::helper::AudioPlayback::FunctionId::Play,
                                    std::string(CONFIG_BROOKESIA_APP_SETTINGS_SOUND_TEST_URL),
                                    BROOKESIA_DESCRIBE_TO_JSON(playback).as_object(),
                                    service::helper::Timeout(SERVICE_TIMEOUT_MS)
                                );
            std::printf("SETTINGS_BUSY_TONE,%d\n", static_cast<bool>(result));
        } else if (std::string_view(command.data()) == "state") {
            SettingsProbeAudioState audio;
            const auto active = system.get_environment();
            const auto heap = sample_heap();
            const bool available = settings_probe_read_audio(audio);
            const auto wifi = service::helper::Wifi::call_function_sync<std::string>(
                                  service::helper::Wifi::FunctionId::GetGeneralState,
                                  service::helper::Timeout(SERVICE_TIMEOUT_MS));
            std::printf("SETTINGS_STATE,theme=%s,language=%s,audio=%d,volume=%.2f,mute=%d,wifi=%s\n",
                        active.theme_id.c_str(), active.language.c_str(), available, audio.volume,
                        audio.muted, wifi ? wifi->c_str() : "unavailable");
            std::printf("SETTINGS_HEAP,%zu,%zu,%zu,%zu\n", heap.internal_free, heap.internal_largest,
                        heap.psram_free, heap.psram_largest);
        }
        std::fill(command.begin(), command.end(), '\0');
        length = 0;
        std::puts("SETTINGS_COMMAND_DONE");
        std::fflush(stdout);
    }
}
