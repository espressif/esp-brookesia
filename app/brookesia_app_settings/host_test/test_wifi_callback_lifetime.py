# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise Settings Wi-Fi callback delivery and shutdown using production code."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def extract(source, signature, suffix='}'):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    if suffix == ';':
        end = source.index(';', end) + 1
    return source[start:end]


PREAMBLE = r'''
#include <atomic>
#include <chrono>
#include <string>
#include <cassert>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
namespace boost {
using mutex = std::mutex;
template<typename T> using lock_guard = std::lock_guard<T>;
template<typename T> using unique_lock = std::unique_lock<T>;
}
namespace esp_brookesia::lib_utils { struct scoped_connection {}; }
namespace detail { using SteadyTimePoint = std::chrono::steady_clock::time_point; }
namespace service { struct FunctionResult { int value = 0; bool success = true; }; }
#define BROOKESIA_DESCRIBE_TO_STR(...) "Connect"
struct WifiHelper {
    enum class FunctionId { SetConnectAp, TriggerGeneralAction };
    enum class GeneralAction { Connect };
    static inline bool inline_callbacks = true;
    static inline int calls = 0;
    static inline std::deque<std::function<void()>> pending;
    template<class Handler> static bool call_function_async(FunctionId, const std::string &, Handler handler) {
        ++calls;
        if (inline_callbacks) handler(service::FunctionResult{});
        else pending.push_back([handler] { handler(service::FunctionResult{}); });
        return true;
    }
    template<class Handler> static bool call_function_async(FunctionId id, const std::string &ssid,
                                                            const std::string &, Handler handler) {
        return call_function_async(id, ssid, handler);
    }
};

struct SettingsApp { int value = 0; };
'''

TESTS = r'''
void check_connect_chain(bool inline_callbacks, bool cancel) {
    WifiHelper::inline_callbacks = inline_callbacks;
    WifiHelper::calls = 0;
    auto operation = std::make_shared<WifiConnectOperation>();
    operation->ssid = "test";
    operation->password = "password";
    /* CONNECT_CALLBACKS */
    save_handler(service::FunctionResult{});
    if (cancel) {
        std::lock_guard lock(operation->mutex);
        operation->valid = false;
    }
    while (!WifiHelper::pending.empty()) {
        auto pending = std::move(WifiHelper::pending.front());
        WifiHelper::pending.pop_front();
        pending();
    }
    assert(operation->password.empty());
    if (cancel) assert(WifiHelper::calls == 1);
    else assert(WifiHelper::calls == 2 && operation->phase == WifiConnectOperation::Phase::WaitingForConnection);
}
int main() {
    check_connect_chain(true, false);
    check_connect_chain(false, false);
    check_connect_chain(false, true);
    auto mailbox = std::make_shared<WifiUiMailbox>();
    SettingsApp app;
    auto deliver = make_wifi_ui_result_handler(mailbox, [&app](service::FunctionResult &&result) {
        app.value = result.value;
    });
    deliver({42});
    assert(app.value == 0 && mailbox->pending.size() == 1);
    mailbox->pending.front()(app);
    mailbox->pending.clear();
    assert(app.value == 42);
    mailbox->close();
    deliver({99});
    assert(mailbox->pending.empty() && app.value == 42);

    auto owned = std::make_unique<SettingsApp>();
    auto *raw = owned.get();
    auto absent = make_wifi_ui_result_handler(std::shared_ptr<WifiUiMailbox>{}, [raw](service::FunctionResult &&result) {
        raw->value = result.value;
    });
    owned.reset();
    absent({1});

    auto old_mailbox = std::make_shared<WifiUiMailbox>();
    auto stale = make_wifi_ui_result_handler(old_mailbox, [raw](service::FunctionResult &&result) {
        raw->value = result.value;
    });
    old_mailbox->close();
    old_mailbox.reset();
    stale({2});
}
'''


def main():
    source = (ROOT / 'app/brookesia_app_settings/src/private/settings_app_internal.hpp').read_text()
    program = PREAMBLE + extract(source, 'struct WifiConnectOperation', ';') + extract(source, 'struct WifiUiMailbox', ';')
    program += '\ntemplate<typename Handler>\n' + extract(source, 'auto make_wifi_ui_result_handler(') + TESTS
    wifi = (ROOT / 'app/brookesia_app_settings/src/service/wifi.ipp').read_text()
    callbacks = '\n'.join(extract(wifi, declaration) + ';' for declaration in (
        '    const auto set_target_handler =', '    const auto save_handler ='))
    program = program.replace('/* CONNECT_CALLBACKS */', callbacks)
    with tempfile.TemporaryDirectory(prefix='settings-wifi-callback-') as temporary:
        source_path = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source_path.write_text(program)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer', str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Settings Wi-Fi callback lifetime tests passed')


if __name__ == '__main__':
    main()
