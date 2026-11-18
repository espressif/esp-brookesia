# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise production GSP startup and deinit failure paths with fault-injectable adapters."""
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


COMMON = r'''
#include <atomic>
#include <cassert>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>
#define CONFIG_BROOKESIA_GUI_BACKEND_GSP 1
template<class... A> void fake_log(A &&...) {}
#define BROOKESIA_LOGE(...) fake_log(__VA_ARGS__)
#define BROOKESIA_LOGW(...) fake_log(__VA_ARGS__)
#define BROOKESIA_LOGI(...) fake_log(__VA_ARGS__)
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)
using AppId = int;
constexpr auto SYSTEM_APP_TASK_GROUP = "app";
constexpr auto SYSTEM_GUI_TASK_GROUP = "gui";
'''

STARTUP = r'''
constexpr int ESP_GSP_OK = 0, ESP_GSP_NO_TRANSITION = 0, STORAGE_FS_TIMEOUT_MS = 5;
struct esp_gsp_config_t {};
using esp_gsp_handle_t = void *;
namespace hal::display { struct PanelIface {}; struct TouchIface {}; }
namespace esp_brookesia::hal::display { using PanelIface = ::hal::display::PanelIface;
                                      using TouchIface = ::hal::display::TouchIface; }
bool fail_scene = true;
int active_sessions = 0;
int esp_gsp_goto_scene(void *, uint16_t, int) { return fail_scene ? -1 : 0; }
int esp_gsp_set_swipe_enabled(void *, bool) { return 0; }
namespace service::helper {
struct Storage {
    enum class FileType { File };
    struct Info { bool exists = true; FileType type = FileType::File; size_t size = 4; };
    static std::expected<Info, std::string> fs_stat(const std::string &, int) { return Info{}; }
    template<class T> static std::expected<size_t, std::string> fs_read(const std::string &, T, int) { return 4; }
};
}
namespace service { struct RawBuffer { RawBuffer(std::byte *, size_t) {} }; }
namespace gui::gsp {
struct Bundle {
    template<class F> static std::expected<std::shared_ptr<const Bundle>, std::string> load(size_t, F) {
        return std::make_shared<Bundle>();
    }
};
struct DisplayLease { hal::display::PanelIface *panel; hal::display::TouchIface *touch;
                      std::shared_ptr<int> desktop; std::shared_ptr<void> lifetime; };
struct AppHost {
    bool alive = false;
    template<class... A> std::expected<void, std::string> start(A &&...) { alive = true; ++active_sessions; return {}; }
    bool running() const { return alive; }
    void *handle() { return this; }
    std::expected<void, std::string> stop() { if (alive) --active_sessions; alive = false; return {}; }
};
}
enum class AppKind { Runtime };
enum class GuiRootKind { GspBundle };
struct Manifest { AppKind kind = AppKind::Runtime; std::string id = "test"; };
std::string resolve_app_resource_path(const Manifest &, const std::string &) { return "bundle"; }
struct Native {
    std::span<const std::byte> gui_bundle() { return {}; }
    void *gui_runtime_config() { return nullptr; }
};
class System { public: class Impl; };
struct AppRecord {
    struct GspCallbackGate { std::mutex mutex; System::Impl *impl = nullptr; };
    struct GspEventTarget { std::shared_ptr<GspCallbackGate> gate; AppId app_id;
                            std::shared_ptr<std::atomic<uint64_t>> epoch; };
    struct { int app_id = 1; Manifest manifest; } info;
    struct { GuiRootKind root_kind = GuiRootKind::GspBundle; std::string root;
             std::map<std::string, uint16_t> start_scenes{{"dark", 9}}; } gui_descriptor;
    std::shared_ptr<Native> native_app;
    std::unique_ptr<gui::gsp::AppHost> gsp_host;
    std::shared_ptr<std::atomic<uint64_t>> gsp_epoch;
    std::shared_ptr<GspEventTarget> gsp_event_target;
    bool gsp_scene_known = false;
    uint16_t gsp_current_scene = 0;
};
struct PanelOwner { std::shared_ptr<hal::display::PanelIface> get() { return {}; } };
struct TouchOwner { std::shared_ptr<hal::display::TouchIface> get() { return {}; } };
class System::Impl {
public:
    struct { std::shared_ptr<int> gsp_desktop_display = std::make_shared<int>(1); } config_;
    struct { std::string theme_id = "dark"; } environment_;
    void *gsp_panel_ = nullptr, *gsp_touch_ = nullptr;
    std::shared_ptr<PanelOwner> gsp_panel_owner_;
    std::shared_ptr<TouchOwner> gsp_touch_owner_;
    std::vector<std::shared_ptr<AppRecord>> snapshot_app_records() { return {}; }
    bool gsp_session_running(const AppRecord &record) { return record.gsp_host && record.gsp_host->running(); }
    std::expected<void, std::string> hold_gsp_display_devices() { return {}; }
    static void forward_gsp_event() {}
    std::expected<void, std::string> ensure_gsp_loaded(AppRecord &);
    std::expected<void, std::string> stop_gsp_session(AppRecord &);
};
'''

STARTUP_TESTS = r'''
int main() {
    System::Impl impl;
    AppRecord record;
    assert(!impl.ensure_gsp_loaded(record));
    assert(active_sessions == 0 && !record.gsp_host && !record.gsp_event_target);
    assert(!record.gsp_scene_known && record.gsp_current_scene == 0);
    fail_scene = false;
    assert(impl.ensure_gsp_loaded(record));
    assert(record.gsp_scene_known && record.gsp_current_scene == 9 && active_sessions == 1);
    assert(impl.stop_gsp_session(record));
    assert(active_sessions == 0);
}
'''

DEINIT = r'''
enum class AppState { Running, Paused, Stopped };
struct Native { int uninstalls = 0; void on_uninstall(int &) { ++uninstalls; } };
struct Record {
    struct { int app_id = 1; AppState state = AppState::Running; } info;
    std::shared_ptr<Native> native_app = std::make_shared<Native>();
    std::shared_ptr<int> context = std::make_shared<int>(0);
    bool fail_stop = false, running = true;
};
class System {
public:
    struct Impl {
        bool initialized_ = true, shell_fail = false;
        std::vector<std::shared_ptr<Record>> records;
        void hide_startup_overlay() {}
        std::expected<void, std::string> stop_gsp_shell_session() {
            if (shell_fail) return std::unexpected("injected shell stop failure");
            return {};
        }
        template<class R, class F> R run_task_sync(const char *, F f, R) { return f(); }
        auto snapshot_app_records() { return records; }
        std::expected<void, std::string> stop_gsp_session(Record &r) {
            if (r.fail_stop) return std::unexpected("injected app stop failure");
            r.running = false; return {};
        }
        bool gsp_session_running(Record &r) { return r.running; }
        bool any_gsp_session_running() { for (auto &r : records) if (r->running) return true; return false; }
        void cancel_app_timers(Record &) {}
        void unload_runtime(Record &) {}
        void clear_pending_gui_bindings(int) {}
        std::expected<void, std::string> unload_gui(Record &) { return {}; }
        void unregister_app_gui_resources(Record &) {}
        void unregister_app_icon_resource(Record &) {}
    };
    std::unique_ptr<Impl> impl_ = std::make_unique<Impl>();
    void stop() {}
    std::expected<void, std::string> stop_app(int id) {
        for (auto &r : impl_->records) if (r->info.app_id == id) r->info.state = AppState::Stopped;
        return {};
    }
    void deinit();
};
'''

DEINIT_TESTS = r'''
int main() {
    System system;
    auto first = std::make_shared<Record>(), second = std::make_shared<Record>();
    second->info.app_id = 2;
    system.impl_->records = {first, second};
    system.impl_->shell_fail = true;
    system.deinit();
    assert(first->native_app->uninstalls == 0 && first->info.state == AppState::Running);
    system.impl_->shell_fail = false;
    second->fail_stop = true;
    system.deinit();
    system.deinit();
    assert(system.impl_->initialized_ && first->native_app->uninstalls == 0 && second->native_app->uninstalls == 0);
    second->fail_stop = false;
    system.deinit();
    system.deinit();
    assert(!system.impl_->initialized_ && first->native_app->uninstalls == 1 && second->native_app->uninstalls == 1);
}
'''


def main():
    gui = (ROOT / 'src/system/gui.cpp').read_text()
    startup = COMMON + STARTUP + '\n'.join(extract(gui, name) for name in (
        'std::expected<void, std::string> System::Impl::ensure_gsp_loaded(',
        'std::expected<void, std::string> System::Impl::stop_gsp_session(')) + STARTUP_TESTS
    lifecycle = (ROOT / 'src/system/lifecycle.cpp').read_text()
    deinit = extract(lifecycle, 'void System::deinit()')
    # Exercise the production stop/uninstall phases; service-manager shutdown is outside this adapter test.
    deinit = deinit[:deinit.index('#if CONFIG_BROOKESIA_GUI_BACKEND_GSP')]
    deinit += 'impl_->initialized_ = false; }'
    with tempfile.TemporaryDirectory(prefix='gsp-failure-cleanup-') as temporary:
        for name, program in (('startup', startup), ('deinit', COMMON + DEINIT + deinit + DEINIT_TESTS)):
            source, binary = Path(temporary) / (name + '.cpp'), Path(temporary) / name
            source.write_text(program)
            subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-pthread', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address', '-fno-omit-frame-pointer', str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)
    print('GSP startup and retryable deinit tests passed')


if __name__ == '__main__':
    main()
