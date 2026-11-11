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
#include <cassert>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
namespace boost {
using mutex = std::mutex;
template<typename T> using lock_guard = std::lock_guard<T>;
}
namespace service { struct FunctionResult { int value = 0; }; }
struct SettingsApp { int value = 0; };
'''

TESTS = r'''
int main() {
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
    program = PREAMBLE + extract(source, 'struct WifiUiMailbox', ';')
    program += '\ntemplate<typename Handler>\n' + extract(source, 'auto make_wifi_ui_result_handler(') + TESTS
    with tempfile.TemporaryDirectory(prefix='settings-wifi-callback-') as temporary:
        source_path = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source_path.write_text(program)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address', '-fno-omit-frame-pointer', str(source_path), '-o', str(binary)], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        subprocess.run([str(binary)], check=True, env=env, timeout=10)
    print('Settings Wi-Fi callback lifetime tests passed')


if __name__ == '__main__':
    main()
