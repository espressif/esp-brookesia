# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Inject bridge polling failures into the production desktop executor, without a renderer or board."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


BRIDGE = r'''
#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
struct FakeGsp {};
using esp_gsp_handle_t = FakeGsp *;
struct esp_gsp_event_t {};
using esp_gsp_err_t = int;
struct esp_gsp_config_t { const void *bundle; size_t bundle_size; };
constexpr int ESP_GSP_OK = 0, ESP_GSP_ERR_INVALID_STATE = -1;
namespace esp_brookesia::hal::display { class PanelIface; class TouchIface; }
inline std::atomic_bool fail_poll = false;
inline std::atomic_int closes = 0;
inline int gsp_sim_bridge_open(const char *, esp_gsp_handle_t *handle) { *handle = new FakeGsp; return 0; }
inline int esp_gsp_on_event(esp_gsp_handle_t, void (*)(esp_gsp_handle_t, const esp_gsp_event_t *, void *), void *) { return 0; }
inline int gsp_sim_bridge_poll(esp_gsp_handle_t, int) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); return fail_poll ? -1 : 0;
}
inline void gsp_sim_bridge_close(esp_gsp_handle_t handle) { delete handle; ++closes; }
'''

DESKTOP = r'''
#pragma once
#include <map>
#include <vector>
namespace esp_brookesia::gui::gsp {
struct DesktopBundle {};
inline std::shared_ptr<const DesktopBundle> find_desktop_bundle(std::span<const std::byte>) {
    return std::make_shared<DesktopBundle>();
}
class DesktopDisplay {
public:
    struct Impl {
        std::mutex mutex;
        std::string failure, run_dir;
        std::expected<std::string, std::string> start(const DesktopBundle &) { return "fake endpoint"; }
        void stop() {}
    } state;
    Impl &implementation() { return state; }
};
}
'''

BUNDLE = r'''
#pragma once
namespace esp_brookesia::gui::gsp {
class Bundle { public: esp_gsp_config_t config() const { return {this, 4}; } };
}
'''

TESTS = r'''
#include <cassert>
using namespace esp_brookesia::gui::gsp;
void wait_for_exit(Session &session) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (session.running() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    assert(!session.running() && session.handle() == nullptr);
}
int main() {
    DisplayLease display;
    display.desktop = std::make_shared<DesktopDisplay>();
    auto bundle = std::make_shared<Bundle>();
    Session session;
    for (int i = 0; i < 20; ++i) {
        fail_poll = false;
        assert(session.start(bundle, display, nullptr, nullptr));
        assert(session.running());
        fail_poll = true;
        wait_for_exit(session);
        assert(closes == i + 1);
        // start() must join the exited thread before replacing its executor.
    }
    assert(session.stop());
    {
        Session destroyed_after_failure;
        fail_poll = false;
        assert(destroyed_after_failure.start(bundle, display, nullptr, nullptr));
        fail_poll = true;
        wait_for_exit(destroyed_after_failure);
    }
    assert(closes == 21);
}
'''


def main():
    source = (ROOT / 'src/session_desktop.cpp').read_text()
    # Property metadata is outside the executor lifecycle; compile the remaining production translation unit.
    source = source.replace(extract(source, 'esp_gsp_err_t desktop_property_info('), '')
    with tempfile.TemporaryDirectory(prefix='gsp-desktop-session-') as temporary:
        directory = Path(temporary)
        files = {
            'gsp_sim_bridge.h': BRIDGE,
            'private/desktop.hpp': DESKTOP,
            'brookesia/gui_gsp/bundle.hpp': BUNDLE,
            'brookesia/gui_gsp/session.hpp': (ROOT / 'include/brookesia/gui_gsp/session.hpp').read_text(),
            'test.cpp': source + TESTS,
        }
        for name, content in files.items():
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
        binary = directory / 'test'
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer', '-DBROOKESIA_GUI_GSP_HOST_TEST',
                        '-I' + str(directory), str(directory / 'test.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Desktop GSP exit, restart and join tests passed')


if __name__ == '__main__':
    main()
