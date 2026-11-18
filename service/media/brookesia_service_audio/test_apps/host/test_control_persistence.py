# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check playback control success independently from asynchronous persistence failures."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


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
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <vector>
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)
#define BROOKESIA_LOGD(...) ((void)0)
int warnings = 0;
#define BROOKESIA_LOGW(...) (++warnings)
struct FunctionResult { bool success; std::string error_message; };
constexpr auto KEY_VOLUME = "Volume", KEY_MUTE = "Mute";
namespace hal {
namespace audio { struct CodecPlayerIface { static constexpr auto NAME = "player"; }; }
bool has_interface(const char *) { return true; }
}
std::expected<std::string, std::string> make_storage_namespace(const std::string &v) { return v; }
std::expected<std::string, std::string> make_storage_key(const std::string &v) { return v; }
struct StorageHelper {
    static inline bool available = true, submit = true;
    static inline std::vector<int> values;
    static inline std::vector<std::function<void(FunctionResult &&)>> completions;
    static bool is_available() { return available; }
    template<class T, class Handler> static bool save_key_value_async(const std::string &, const std::string &,
                                                                    T value, Handler handler) {
        if (!submit) return false;
        values.push_back(value); completions.push_back(handler); return true;
    }
};
struct AudioPlayback {
    uint8_t desired_volume_ = 20;
    bool desired_mute_ = false;
    int hal_volume = 20, changes = 0;
    bool hal_muted = false;
    struct Attributes { std::string name = "AudioPlayback"; };
    Attributes get_attributes() { return {}; }
    std::expected<uint8_t, std::string> validate_percentage(double v, const char *) { return static_cast<uint8_t>(v); }
    bool apply_control_state_to_hal() { hal_volume = desired_volume_; hal_muted = desired_mute_; return true; }
    void publish_volume_changed() { ++changes; }
    void publish_mute_changed() { ++changes; }
    std::expected<void, std::string> function_set_volume(double);
    std::expected<void, std::string> function_set_mute(bool);
    std::expected<void, std::string> save_volume_data();
    std::expected<void, std::string> save_mute_data();
};
/* METHODS */
int main() {
    AudioPlayback audio;
    assert(audio.function_set_volume(20) && audio.function_set_mute(false));
    assert(StorageHelper::values.empty() && audio.changes == 0);
    StorageHelper::available = false;
    assert(audio.function_set_volume(40) && audio.hal_volume == 40);
    assert(audio.function_set_mute(true) && audio.hal_muted);
    assert(warnings == 2 && StorageHelper::values.empty());
    StorageHelper::available = true;
    assert(audio.function_set_volume(60));
    assert(audio.hal_volume == 60 && StorageHelper::values.back() == 60);
    // A later KV failure cannot retroactively fail a control that was already applied.
    StorageHelper::completions.back()({false, "injected storage failure"});
    assert(audio.hal_volume == 60 && warnings == 3);
    const auto writes = StorageHelper::values.size();
    assert(audio.function_set_volume(60) && StorageHelper::values.size() == writes);
    StorageHelper::submit = false;
    assert(audio.function_set_mute(false) && !audio.hal_muted && warnings == 4);
}
'''


def main():
    source = (ROOT / 'src/audio_playback_control.cpp').read_text()
    methods = '\n'.join(extract(source, name) for name in (
        'std::expected<void, std::string> AudioPlayback::function_set_volume(',
        'std::expected<void, std::string> AudioPlayback::function_set_mute(',
        'std::expected<void, std::string> AudioPlayback::save_volume_data()',
        'std::expected<void, std::string> AudioPlayback::save_mute_data()'))
    with tempfile.TemporaryDirectory(prefix='audio-control-persistence-') as temporary:
        source_path, binary = Path(temporary) / 'test.cpp', Path(temporary) / 'test'
        source_path.write_text(PROGRAM.replace('/* METHODS */', methods))
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('Audio control persistence tests passed')


if __name__ == '__main__':
    main()
