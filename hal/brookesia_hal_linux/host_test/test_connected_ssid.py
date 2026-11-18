# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check SSID lookup against cached connections and fake nmcli responses, without changing networking."""
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


PROGRAM = r'''
#include <cassert>
#include <deque>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
std::deque<std::pair<std::string, int>> outputs;
std::vector<std::string> commands;
std::string get_first_wifi_device() { return "wlan0"; }
std::string run_command(const std::string &command, int *exit_code) {
    assert(command.find("wifi list") == std::string::npos && !outputs.empty());
    commands.push_back(command);
    auto output = std::move(outputs.front()); outputs.pop_front();
    *exit_code = output.second; return output.first;
}
/* HELPERS */
struct Backend {
    bool network_manager_available_ = true;
    bool connected_ = false;
    std::string active_ssid_;
    /* METHOD */
};
int main() {
    Backend backend;
    std::string ssid = "stale";
    backend.connected_ = true;
    backend.active_ssid_ = "hidden:ssid";
    assert(backend.get_connected_ssid(ssid) && ssid == "hidden:ssid" && commands.empty());
    backend.connected_ = false;
    backend.active_ssid_.clear();
    outputs = {{"GENERAL.CON-UUID:uuid-1\n", 0}, {"802-11-wireless.ssid:hidden\\:ssid\n", 0}};
    assert(backend.get_connected_ssid(ssid) && ssid == "hidden:ssid" && outputs.empty());
    assert(commands.back().find("connection show --active uuid") != std::string::npos);
    outputs = {{"GENERAL.CON-UUID:--\n", 0}};
    assert(!backend.get_connected_ssid(ssid) && ssid.empty());
    outputs = {{"GENERAL.CON-UUID:uuid-1\n", 0}, {"802-11-wireless.ssid:\n", 0}};
    assert(!backend.get_connected_ssid(ssid) && ssid.empty());
    outputs = {{"", 1}};
    assert(!backend.get_connected_ssid(ssid) && ssid.empty());
    backend.network_manager_available_ = false;
    assert(!backend.get_connected_ssid(ssid));
}
'''


def main():
    source = (ROOT / 'src/wifi/device.cpp').read_text()
    helpers = '\n'.join(extract(source, name) for name in (
        'std::string shell_quote(', 'std::vector<std::string> split_nmcli_line(',
        'std::vector<std::string> split_lines('))
    program = PROGRAM.replace('/* HELPERS */', helpers).replace(
        '/* METHOD */', extract(source, '    bool get_connected_ssid('))
    with tempfile.TemporaryDirectory(prefix='linux-connected-ssid-') as temporary:
        source_path, binary = Path(temporary) / 'test.cpp', Path(temporary) / 'test'
        source_path.write_text(program)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        '-DBROOKESIA_HAL_LINUX_WIFI_BACKEND_STUB=0', str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Linux connected SSID tests passed')


if __name__ == '__main__':
    main()
