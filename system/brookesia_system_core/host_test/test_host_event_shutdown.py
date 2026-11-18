# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check service event posting during system shutdown."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PROGRAM = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#define BROOKESIA_LOGW(...) ((void)0)
using RuntimeAppId = int;
namespace lib_utils {
struct TaskScheduler {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool allow_post = false;
    std::function<void()> pending;
    bool post(std::function<void()> task) {
        std::unique_lock lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait(lock, [&] { return allow_post; });
        pending = std::move(task);
        return true;
    }
};
}
struct SystemHostBridge {
    using EventDispatcher = std::function<std::expected<void, std::string>(
        RuntimeAppId, std::string, std::string, std::string)>;
};
struct FakeImpl {
'''

DECLARATIONS = r'''
    std::shared_ptr<EventDispatchState> event_dispatch_ = std::make_shared<EventDispatchState>();
    void stop_event_dispatch();
    static void post_service_event(const std::weak_ptr<EventDispatchState> &, RuntimeAppId,
                                   std::string, std::string, std::string);
};
'''

TESTS = r'''
int main() {
    FakeImpl impl;
    auto state = impl.event_dispatch_;
    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    state->scheduler = scheduler;
    std::atomic_int calls{0};
    state->dispatcher = [&](RuntimeAppId, std::string, std::string, std::string) {
        ++calls;
        return std::expected<void, std::string>{};
    };
    auto posting = std::async(std::launch::async, [&] {
        FakeImpl::post_service_event(state, 1, "service", "event", "{}");
    });
    {
        std::unique_lock lock(scheduler->mutex);
        assert(scheduler->condition.wait_for(lock, std::chrono::seconds(5), [&] { return scheduler->entered; }));
    }
    std::promise<void> stopping_started;
    auto started = stopping_started.get_future();
    auto stopping = std::async(std::launch::async, [&] {
        stopping_started.set_value();
        impl.stop_event_dispatch();
    });
    started.wait();
    assert(stopping.wait_for(std::chrono::milliseconds(25)) == std::future_status::timeout);
    {
        std::lock_guard lock(scheduler->mutex);
        scheduler->allow_post = true;
        scheduler->condition.notify_all();
    }
    posting.get();
    stopping.get();
    assert(scheduler->pending);
    auto pending = std::exchange(scheduler->pending, {});
    pending();
    FakeImpl::post_service_event(state, 1, "service", "event", "{}");
    assert(calls == 0);
}
'''


def main():
    source = (ROOT / 'src/runtime/host_bridge.cpp').read_text()
    state = extract(source, '    struct EventDispatchState {') + ';\n'
    shutdown = extract(source, 'void SystemHostBridge::Impl::stop_event_dispatch()')
    post = extract(source, 'void SystemHostBridge::Impl::post_service_event(')
    methods = (shutdown + '\n' + post).replace('SystemHostBridge::Impl::', 'FakeImpl::')
    lifecycle = (ROOT / 'src/system/lifecycle.cpp').read_text()
    abandon = extract(lifecycle, 'void System::Impl::abandon_system_references()')
    assert abandon.index('host_bridge_->stop_event_dispatch()') < abandon.index('task_scheduler_->stop()')
    assert abandon.index('task_scheduler_->stop()') < abandon.index('runtime_->deinit()')
    with tempfile.TemporaryDirectory(prefix='system-event-shutdown-') as temporary:
        source_path = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source_path.write_text(PROGRAM + state + DECLARATIONS + methods + TESTS)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer', str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('System service event shutdown tests passed')


if __name__ == '__main__':
    main()
