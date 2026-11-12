# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check optimistic sound controls, stale events and missing completions with fake services."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PREAMBLE = r'''
#include <algorithm>
#include <cassert>
#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace boost {
using mutex = std::mutex;
template<class T> using lock_guard = std::lock_guard<T>;
}
namespace service { struct FunctionResult { bool success = true; std::string error_message; }; }
namespace system_core {
struct AppContext {};
enum class MessageDialogIcon { Warning };
}
using SteadyClock = std::chrono::steady_clock;
constexpr bool SETTINGS_HARDWARE_OPERATIONS_ENABLED = true;
constexpr int DEVICE_SERVICE_TIMEOUT_MS = 500;
constexpr int MESSAGE_DIALOG_FAILED_AUTO_CLOSE_MS = 100;
#define BROOKESIA_LOGW(...) ((void)0)
struct AudioPlaybackHelper {
    enum class FunctionId { SetVolume, SetMute };
    static inline bool submit = true;
    static inline std::vector<std::function<void(bool)>> pending;
    static inline int volume = 20;
    static inline bool muted = false;
    template<class T, class Handler>
    static bool call_function_async(FunctionId id, T value, Handler completion) {
        if (!submit) return false;
        pending.push_back([id, value, completion](bool success) {
            if (success) {
                if (id == FunctionId::SetVolume) volume = value;
                else muted = value;
            }
            completion({success, {}});
        });
        return true;
    }
};
std::string localized_text(const std::string &, const char *key) { return key; }
int clamp_percent(double value) { return std::clamp(static_cast<int>(value), 0, 100); }
/* MAILBOX */
struct SettingsApp {
    struct State {
        bool audio_service_ready = true, audio_player_supported = true, sound_pending = false;
        int volume = 20;
        bool muted = false;
        std::string sound_error;
    } device_ui_state_;
    std::shared_ptr<SoundUiMailbox> sound_ui_mailbox_ = std::make_shared<SoundUiMailbox>();
    uint64_t sound_control_generation_ = 0;
    SteadyClock::time_point sound_control_deadline_{};
    bool settings_tone_active_ = false;
    SteadyClock::time_point settings_tone_deadline_{};
    std::string current_locale_;
    int shown_volume = 20;
    bool shown_mute = false;
    void ensure_message_dialog(system_core::AppContext &, const std::string &, system_core::MessageDialogIcon, int) {}
    std::expected<void, std::string> refresh_sound_state(system_core::AppContext &) {
        shown_volume = device_ui_state_.volume;
        shown_mute = device_ui_state_.muted;
        return {};
    }
    std::expected<void, std::string> load_audio_state() {
        device_ui_state_.volume = AudioPlaybackHelper::volume;
        device_ui_state_.muted = AudioPlaybackHelper::muted;
        return {};
    }
    void set_volume(system_core::AppContext &, int);
    void set_mute(system_core::AppContext &, bool);
    std::expected<void, std::string> drain_sound_events(system_core::AppContext &);
};
'''

TESTS = r'''
int main() {
    system_core::AppContext context;
    SettingsApp app;
    app.set_volume(context, 80);
    assert(app.shown_volume == 80 && app.device_ui_state_.sound_pending);
    app.sound_ui_mailbox_->volume = 20;
    assert(app.drain_sound_events(context));
    assert(app.shown_volume == 80);
    app.set_volume(context, 80);
    assert(AudioPlaybackHelper::pending.size() == 1);
    AudioPlaybackHelper::pending.back()(true);
    assert(app.drain_sound_events(context));
    assert(app.shown_volume == 80 && !app.device_ui_state_.sound_pending);

    app.set_mute(context, true);
    assert(app.shown_mute);
    AudioPlaybackHelper::pending.back()(false);
    assert(app.drain_sound_events(context));
    assert(!app.shown_mute && app.device_ui_state_.sound_error == "sound.update_failed");

    app.set_volume(context, 60);
    app.sound_control_deadline_ = SteadyClock::now() - std::chrono::seconds(1);
    assert(app.drain_sound_events(context));
    assert(!app.device_ui_state_.sound_pending && app.shown_volume == 80);
    // A result from the timed-out generation cannot complete the next request.
    AudioPlaybackHelper::pending.back()(true);
    app.set_volume(context, 40);
    assert(app.drain_sound_events(context));
    assert(app.device_ui_state_.sound_pending && app.shown_volume == 40);
    AudioPlaybackHelper::pending.back()(true);
    assert(app.drain_sound_events(context));
    assert(!app.device_ui_state_.sound_pending && app.shown_volume == 40);

    AudioPlaybackHelper::submit = false;
    app.set_volume(context, 10);
    assert(app.shown_volume == 40 && !app.device_ui_state_.sound_pending);
    assert(!app.device_ui_state_.sound_error.empty());
}
'''


def main():
    header = (ROOT / 'src/private/settings_app_internal.hpp').read_text()
    source = (ROOT / 'src/service/device.ipp').read_text()
    program = PREAMBLE.replace('/* MAILBOX */', extract(header, 'struct SoundUiMailbox {') + ';')
    program += '\n'.join(extract(source, name) for name in (
        'void SettingsApp::set_volume(', 'void SettingsApp::set_mute(',
        'std::expected<void, std::string> SettingsApp::drain_sound_events('))
    program = program.replace('system::core::', 'system_core::') + TESTS
    with tempfile.TemporaryDirectory(prefix='settings-sound-control-') as temporary:
        source_path, binary = Path(temporary) / 'test.cpp', Path(temporary) / 'test'
        source_path.write_text(program)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer', str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Settings sound control tests passed')


if __name__ == '__main__':
    main()
