# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise the production EventMonitor with in-flight signal callbacks."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[5]


def extract_class(source):
    start = source.index('class EventMonitor {')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:source.index(';', end) + 1]


PREAMBLE = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>
#include "brookesia/lib_utils/signal.hpp"
using EventItem = std::variant<bool, double, std::string>;
using EventItemMap = std::map<std::string, EventItem>;
struct EventRegistry { using SignalConnection = esp_brookesia::lib_utils::scoped_connection; };
struct Helper {
    enum class EventId { Sample };
    using Signal = esp_brookesia::lib_utils::signal<void(const std::string &, const EventItemMap &)>;
    static inline Signal events;
    static inline std::atomic_bool block = false;
    static inline std::promise<void> *entered = nullptr;
    static inline std::shared_future<void> release;
    static EventRegistry::SignalConnection subscribe_event(EventId, Signal::slot_type callback) {
        return events.connect([callback = std::move(callback)](const auto &name, const auto &items) {
            if (block) { entered->set_value(); release.wait(); }
            callback(name, items);
        });
    }
    template<auto EventIdValue>
'''

TESTS = r'''
};
int main() {
    using Monitor = Helper::EventMonitor<Helper::EventId::Sample>;
    {
        Monitor monitor;
        assert(monitor.start());
        Helper::events("sample", {{"value", 42.0}});
        assert(monitor.get_count() == 1 && monitor.has({42.0}));
        monitor.stop();
        assert(!monitor.is_running() && monitor.get_count() == 0);
    }
    {
        auto monitor = std::make_unique<Monitor>();
        assert(monitor->start());
        std::promise<void> entered, release;
        Helper::entered = &entered;
        Helper::release = release.get_future().share();
        Helper::block = true;
        std::thread old_event([] { Helper::events("sample", {{"value", 1.0}}); });
        entered.get_future().wait();
        monitor->stop();
        assert(monitor->start());
        release.set_value();
        old_event.join();
        Helper::block = false;
        assert(monitor->get_count() == 0);
        Helper::events("sample", {{"value", 2.0}});
        assert(monitor->has({2.0}));
    }
    {
        auto monitor = std::make_unique<Monitor>();
        assert(monitor->start());
        std::promise<void> entered, release;
        Helper::entered = &entered;
        Helper::release = release.get_future().share();
        Helper::block = true;
        std::thread old_event([] { Helper::events("sample", {{"value", 3.0}}); });
        entered.get_future().wait();
        esp_brookesia::lib_utils::signal<void()> other;
        auto stop = other.connect([&] { monitor.reset(); });
        other();
        release.set_value();
        old_event.join();
        Helper::block = false;
        assert(stop.disconnect_and_wait());
    }
}
'''


def main():
    header = ROOT / 'service/framework/brookesia_service_manager/include/brookesia/service_manager/helper/base.hpp'
    program = PREAMBLE + extract_class(header.read_text()) + TESTS
    with tempfile.TemporaryDirectory(prefix='event-monitor-lifetime-') as temporary:
        source = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source.write_text(program)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer',
                        f'-I{ROOT / "utils/brookesia_lib_utils/include"}', str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), timeout=10)
    print('EventMonitor callback lifetime tests passed')


if __name__ == '__main__':
    main()
