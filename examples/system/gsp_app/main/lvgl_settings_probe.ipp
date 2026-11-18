/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Include after lvgl_probe.ipp so this optional check reuses its touch,
// named-object lookup, navigation, and framebuffer capture helpers.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string_view>
#include <thread>

#include "brookesia/service_helper/media/audio.hpp"

namespace esp_brookesia::gsp_app::lvgl_settings_probe {

namespace {

namespace base = esp_brookesia::gsp_app::lvgl_probe;
using AudioPlaybackHelper = service::helper::AudioPlayback;
using System = system::super::System;
using AppId = system::core::AppId;

constexpr int SERVICE_TIMEOUT_MS = 1500;
constexpr int AUDIO_TIMEOUT_MS = 8000;
constexpr std::string_view HOME_SOUND_PATH = "/settings_home/page/main_list/sound";
constexpr std::string_view HOME_MORE_PATH = "/settings_home/page/main_list/more";
constexpr std::string_view HOME_DISPLAY_PATH = "/settings_home/page/main_list/display";
constexpr std::string_view SOUND_SLIDER_PATH = "/sound/page/volume_card/media_volume_row/slider";
constexpr std::string_view SOUND_MUTE_PATH = "/sound/page/silent_card/silent_mode/toggle";
constexpr std::string_view MORE_LANGUAGE_PATH = "/more/page/language_card/language";
constexpr std::string_view LANGUAGE_LIST_PATH = "/language/page/language_card/list";
constexpr std::string_view LIGHT_MODE_PATH = "/display/page/theme_modes/light_mode";
constexpr std::string_view DARK_MODE_PATH = "/display/page/theme_modes/dark_mode";

struct AudioState {
    double volume;
    bool muted;
};

std::optional<AudioState> audio_state()
{
    auto volume = AudioPlaybackHelper::call_function_sync<double>(
                      AudioPlaybackHelper::FunctionId::GetVolume,
                      service::helper::Timeout(SERVICE_TIMEOUT_MS)
                  );
    auto mute = AudioPlaybackHelper::call_function_sync<bool>(
                    AudioPlaybackHelper::FunctionId::GetMute,
                    service::helper::Timeout(SERVICE_TIMEOUT_MS)
                );
    if (!volume || !mute) {
        return std::nullopt;
    }
    return AudioState{.volume = *volume, .muted = *mute};
}

bool tap_slider_percent(const base::Context &context, const gui::ViewFrame &frame, int percent)
{
    const int x = frame.x + 1 + (frame.width - 2) * std::clamp(percent, 0, 100) / 100;
    return base::tap(context, x, frame.y + frame.height / 2);
}

bool restore_volume_via_ui(const base::Context &context, const gui::ViewFrame &frame, double original)
{
    int percent = std::clamp(static_cast<int>(std::lround(original)), 0, 100);
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto before = audio_state();
        if (!before || !tap_slider_percent(context, frame, percent)) {
            return false;
        }
        std::optional<AudioState> observed;
        if (!base::wait_for([&]() {
        observed = audio_state();
        return observed && (std::abs(observed->volume - original) <= 2.5 ||
                            std::abs(observed->volume - before->volume) >= 0.5);
    }, AUDIO_TIMEOUT_MS, "audio volume restore")) {
            return false;
        }
        if (std::abs(observed->volume - original) <= 2.5) {
            return true;
        }
        percent = std::clamp(percent + static_cast<int>(std::lround(original - observed->volume)), 0, 100);
    }
    return false;
}

bool read_only_page(const base::Context &context, std::string_view path, std::string_view page,
                    std::string_view expected_control, std::string_view frame_label)
{
    if (!base::tap_path(context, context.settings_id, path, true) || !base::wait_page(context, page)) {
        return false;
    }
    if (!base::wait_for([&]() {
    return base::absolute_frame(context, context.settings_id, expected_control).has_value();
}, base::PAGE_TIMEOUT_MS, expected_control)) {
        return false;
    }
    return base::capture_frame(context, frame_label);
}

} // namespace

bool run(System &system, ::Display &display)
{
    auto panel = hal::acquire_first_interface<hal::display::PanelIface>();
    if (!panel) {
        BROOKESIA_LOGE("LVGL Settings probe panel unavailable");
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
            context.settings_id == system::core::INVALID_APP_ID ||
            !base::wait_active(context, context.shell_id)) {
        BROOKESIA_LOGE("LVGL Settings probe Shell or Settings app missing");
        return false;
    }

    auto audio_binding = service::ServiceManager::get_instance().bind(AudioPlaybackHelper::get_name().data());
    if (!audio_binding.is_valid() || !AudioPlaybackHelper::is_running()) {
        BROOKESIA_LOGE("LVGL Settings probe AudioPlayback service unavailable");
        return false;
    }
    const auto original = audio_state();
    if (!original) {
        BROOKESIA_LOGE("LVGL Settings probe AudioPlayback service unavailable");
        return false;
    }
    bool settings_open = false;
    bool audio_changed = false;
    lib_utils::FunctionGuard cleanup([&]() {
        if (audio_changed) {
            auto volume = AudioPlaybackHelper::call_function_sync(
                              AudioPlaybackHelper::FunctionId::SetVolume, original->volume,
                              service::helper::Timeout(SERVICE_TIMEOUT_MS)
                          );
            auto mute = AudioPlaybackHelper::call_function_sync(
                            AudioPlaybackHelper::FunctionId::SetMute, original->muted,
                            service::helper::Timeout(SERVICE_TIMEOUT_MS)
                        );
            if (!volume || !mute) {
                BROOKESIA_LOGE("LVGL Settings probe audio cleanup failed");
            }
        }
        if (settings_open) {
            (void)system.stop_app(context.settings_id);
        }
    });

    settings_open = true;
    if (!base::open_settings(context)) {
        return false;
    }
    if (!base::tap_path(context, context.settings_id, HOME_SOUND_PATH, true) ||
            !base::wait_page(context, "sound") || !base::capture_frame(context, "sound_initial")) {
        return false;
    }
    auto slider = base::absolute_frame(context, context.settings_id, SOUND_SLIDER_PATH);
    if (!slider || slider->width < 40) {
        BROOKESIA_LOGE("LVGL Settings probe Sound slider unavailable");
        return false;
    }
    const int target = original->volume > 50 ? 25 : 75;
    audio_changed = true;
    if (!tap_slider_percent(context, *slider, target) || !base::wait_for([&]() {
    const auto current = audio_state();
    return current && std::abs(current->volume - original->volume) >= 10.0;
}, AUDIO_TIMEOUT_MS, "audio volume change")) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (!restore_volume_via_ui(context, *slider, original->volume) ||
            !base::tap_path(context, context.settings_id, SOUND_MUTE_PATH, true) ||
            !base::wait_for([&]() {
    const auto current = audio_state();
    return current && current->muted != original->muted;
}, AUDIO_TIMEOUT_MS, "audio mute change")) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (!base::tap_path(context, context.settings_id, SOUND_MUTE_PATH, true) || !base::wait_for([&]() {
    const auto current = audio_state();
    return current && current->muted == original->muted;
}, AUDIO_TIMEOUT_MS, "audio mute restore")) {
        return false;
    }
    const auto ui_restored = audio_state();
    if (!ui_restored || std::abs(ui_restored->volume - original->volume) > 2.5 ||
            ui_restored->muted != original->muted) {
        BROOKESIA_LOGE("LVGL Settings probe Sound UI restore failed");
        return false;
    }
    std::printf("LVGL_SETTINGS_VOLUME_UI,%.1f,%.1f\n", original->volume, ui_restored->volume);
    auto exact_restore = AudioPlaybackHelper::call_function_sync(
                             AudioPlaybackHelper::FunctionId::SetVolume, original->volume,
                             service::helper::Timeout(SERVICE_TIMEOUT_MS)
                         );
    if (!exact_restore || !base::wait_for([&]() {
    const auto current = audio_state();
    return current && std::abs(current->volume - original->volume) <= 0.1 &&
           current->muted == original->muted;
}, AUDIO_TIMEOUT_MS, "exact audio restore")) {
        BROOKESIA_LOGE("LVGL Settings probe exact Sound restore failed");
        return false;
    }
    const auto restored = audio_state();
    if (!restored || !base::capture_frame(context, "sound_restored")) {
        return false;
    }
    audio_changed = false;
    std::printf("LVGL_SETTINGS_SOUND_PASS,%.1f,%d,%.1f,%d\n", original->volume,
                static_cast<int>(original->muted), restored->volume, static_cast<int>(restored->muted));

    if (!base::back_home(context) ||
            !read_only_page(context, HOME_DISPLAY_PATH, "display", LIGHT_MODE_PATH, "display_theme_read_only") ||
            !base::absolute_frame(context, context.settings_id, DARK_MODE_PATH) ||
            !base::back_home(context) ||
            !base::tap_path(context, context.settings_id, HOME_MORE_PATH, true) ||
            !base::wait_page(context, "more") ||
            !read_only_page(context, MORE_LANGUAGE_PATH, "language", LANGUAGE_LIST_PATH, "language_read_only") ||
            !base::tap_path(context, context.settings_id, base::BACK_PATH) ||
            !base::wait_page(context, "more") || !base::back_home(context) ||
            !base::close_settings(context)) {
        return false;
    }
    settings_open = false;
    if (!base::capture_frame(context, "settings_returned_to_super")) {
        return false;
    }
    cleanup.release();
    std::puts("LVGL_SETTINGS_PROBE_PASS");
    return true;
}

} // namespace esp_brookesia::gsp_app::lvgl_settings_probe
