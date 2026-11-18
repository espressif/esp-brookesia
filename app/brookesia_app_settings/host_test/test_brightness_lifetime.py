# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise Settings brightness subscription shutdown with real signal lifetimes."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PREAMBLE = r'''
#include <atomic>
#include <cassert>
#include <cmath>
#include <expected>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>
#include "brookesia/lib_utils/signal.hpp"
#define BROOKESIA_LOGE(...) ((void)0)
#define BROOKESIA_LOGW(...) ((void)0)
namespace system_core {
using TimerId = uint64_t;
constexpr TimerId INVALID_TIMER_ID = 0;
struct Timer {
    bool fail = false;
    int next = 0;
    int stopped = 0;
    std::expected<TimerId, std::string> start_periodic(const char *, int) {
        if (fail) return std::unexpected("timer creation failed");
        return ++next;
    }
    bool stop(int) { ++stopped; return true; }
};
struct AppContext { Timer timer_; Timer &timer() { return timer_; } };
}
namespace service {
using EventItem = std::variant<double, std::string>;
using EventItemMap = std::map<std::string, EventItem>;
}
using esp_brookesia::lib_utils::scoped_connection;
using esp_brookesia::lib_utils::signal;
constexpr const char *BRIGHTNESS_UI_TIMER_NAME = "brightness";
constexpr int BRIGHTNESS_UI_POLL_INTERVAL_MS = 100;
const service::EventItem *find_event_item(const service::EventItemMap &items, const char *name) {
    auto it = items.find(name); return it == items.end() ? nullptr : &it->second;
}
int clamp_percent(double value) { return static_cast<int>(std::lround(value)); }

struct BrightnessCallbackState {
    std::mutex mutex;
    bool open = false;
    unsigned output_id = 0;
    bool output_valid = false;
    bool sync_queued = false;
    int pending_brightness = 0;
    system_core::TimerId timer_id = system_core::INVALID_TIMER_ID;
};
struct DisplayHelper {
    enum class EventId { BacklightBrightnessChanged };
    using Slot = std::function<void(const std::string &, const service::EventItemMap &)>;
    static inline signal<void(const std::string &, const service::EventItemMap &)> events;
    static inline std::atomic_bool block = false;
    static inline std::promise<void> *entered = nullptr;
    static inline std::shared_future<void> release;
    static scoped_connection subscribe_event(EventId, Slot callback) {
        return events.connect([callback = std::move(callback)](const std::string &name,
                                                              const service::EventItemMap &items) {
            if (block) { entered->set_value(); release.wait(); }
            callback(name, items);
        });
    }
};
struct SoundMailbox { void close() {} };
class SettingsApp {
public:
    struct DeviceState {
        bool display_backlight_supported = true;
        std::optional<unsigned> backlight_output_id = 7;
        int brightness = 0;
    } device_ui_state_;
    system_core::AppContext *context_;
    std::shared_ptr<BrightnessCallbackState> brightness_callbacks_ = std::make_shared<BrightnessCallbackState>();
    std::vector<scoped_connection> device_event_connections_;
    std::shared_ptr<SoundMailbox> sound_ui_mailbox_;
    system_core::TimerId sound_ui_timer_id_ = system_core::INVALID_TIMER_ID;
    bool settings_tone_active_ = false;
    int settings_tone_deadline_ = 0;
    int updates = 0;
    explicit SettingsApp(system_core::AppContext &context) : context_(&context) {}
    void subscribe_brightness_events();
    void shutdown_brightness_callbacks();
    void disconnect_device_events();
    void apply_scheduled_brightness(system_core::AppContext &, uint64_t);
    std::expected<void, std::string> refresh_display_state(system_core::AppContext &) { ++updates; return {}; }
};
'''

TAIL = r'''
service::EventItemMap event(double brightness) {
    return {{"OutputId", 7.0}, {"OutputName", std::string("panel")}, {"Brightness", brightness}};
}
int main() {
    system_core::AppContext context;
    {
        SettingsApp app(context);
        app.subscribe_brightness_events();
        auto timer = app.brightness_callbacks_->timer_id;
        assert(timer != system_core::INVALID_TIMER_ID);
        DisplayHelper::events("brightness", event(20));
        DisplayHelper::events("brightness", event(55));
        app.apply_scheduled_brightness(context, timer + 1);
        assert(app.updates == 0);
        app.apply_scheduled_brightness(context, timer);
        assert(app.updates == 1 && app.device_ui_state_.brightness == 55);
        app.shutdown_brightness_callbacks();
    }
    {
        auto app = std::make_unique<SettingsApp>(context);
        app->subscribe_brightness_events();
        auto state = app->brightness_callbacks_;
        std::promise<void> entered, release;
        DisplayHelper::entered = &entered;
        DisplayHelper::release = release.get_future().share();
        DisplayHelper::block = true;
        std::thread event_thread([] { DisplayHelper::events("brightness", event(60)); });
        entered.get_future().wait();
        signal<void()> other;
        auto stop = other.connect([&] { app->shutdown_brightness_callbacks(); app.reset(); });
        other();
        assert(!app && !state->open);
        release.set_value();
        event_thread.join();
        assert(!state->sync_queued);
        DisplayHelper::block = false;
        stop.disconnect();
    }
    {
        SettingsApp app(context);
        context.timer_.fail = true;
        app.subscribe_brightness_events();
        assert(app.device_event_connections_.empty());
        assert(app.brightness_callbacks_->timer_id == system_core::INVALID_TIMER_ID);
        app.shutdown_brightness_callbacks();
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++-13'))
    parser.add_argument('--sanitizer', choices=('address', 'thread'))
    args = parser.parse_args()
    source = (ROOT / 'app/brookesia_app_settings/src/service/device.ipp').read_text()
    methods = ('void SettingsApp::subscribe_brightness_events()',
               'void SettingsApp::shutdown_brightness_callbacks()',
               'void SettingsApp::disconnect_device_events()',
               'void SettingsApp::apply_scheduled_brightness(')
    program = PREAMBLE + '\n'.join(extract(source, method) for method in methods) + TAIL
    program = program.replace('system::core::', 'system_core::')
    with tempfile.TemporaryDirectory(prefix='settings-brightness-lifetime-') as temporary:
        source_path = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source_path.write_text(program)
        command = [args.cxx, '-std=c++23', '-Wall', '-Wextra', '-Werror', '-pthread',
                   f'-I{ROOT / "utils/brookesia_lib_utils/include"}', str(source_path), '-o', str(binary)]
        if args.sanitizer:
            command.extend([f'-fsanitize={args.sanitizer}', '-fno-omit-frame-pointer'])
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Settings brightness subscription lifetime tests passed')


if __name__ == '__main__':
    main()
