# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Compile production lifecycle slices with fault-injectable GSP/service adapters.

This test executes the real request manager and shell methods. GSP, timers and
service dispatch are fakes; it does not validate rendering or real HAL teardown.
The desktop display stays null to exercise the synchronous board lifecycle path.
"""
import argparse
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


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
#include <algorithm>
#include <atomic>
#include <cassert>
#include <deque>
#include <expected>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#define BROOKESIA_LOGW(...) ((void)0)
#define BROOKESIA_LOGE(...) ((void)0)
using AppId = int;
using TimerId = int;
using MessageDialogRequestId = int;
using KeyboardRequestId = int;
constexpr int INVALID_APP_ID = 0;
constexpr int INVALID_TIMER_ID = 0;
constexpr int INVALID_MESSAGE_DIALOG_REQUEST_ID = 0;
constexpr int INVALID_KEYBOARD_REQUEST_ID = 0;
constexpr auto GSP_CLOCK_TIMER_NAME = "clock";
enum class MessageDialogCloseReason { Closed, Button, Timeout };
enum class MessageDialogButtonRole { Invalid, Accept };
struct Button { MessageDialogButtonRole role = MessageDialogButtonRole::Accept; };
struct MessageDialogOptions { std::vector<Button> buttons; };
struct MessageDialogResult {
    int request_id; int app_id; int button_index;
    MessageDialogButtonRole button_role; MessageDialogCloseReason reason;
};
using MessageDialogResultHandler = std::function<void(MessageDialogResult)>;
namespace lib_utils {
struct FunctionGuard {
    std::function<void()> cleanup;
    explicit FunctionGuard(std::function<void()> f) : cleanup(std::move(f)) {}
    ~FunctionGuard() { cleanup(); }
};
}
namespace service {
using EventItemMap = std::map<std::string, int>;
struct Connection { void disconnect() {} };
namespace helper {
struct SNTP {
    enum class EventId { TimezoneChanged };
    static inline std::function<void(const std::string &, const EventItemMap &)> callback;
    static bool is_available() { return true; }
    static Connection subscribe_event(EventId, decltype(callback) cb) {
        callback = std::move(cb); return {};
    }
};
}
}
struct Timer {
    int starts = 0;
    int stops = 0;
    std::expected<int, std::string> start_delayed(const char *, int) { return ++starts; }
    bool stop(int) { ++stops; return true; }
};
struct AppContext {
    Timer clock;
    Timer &timer() { return clock; }
};
namespace boost::json {
struct object { int &operator[](const char *) { static int value; return value; } };
std::string serialize(const object &) { return "{}"; }
}
constexpr auto SYSTEM_GUI_TASK_GROUP = "gui";
constexpr auto LIFECYCLE_ON_ACTION = "on_action";
struct NativeApp {
    int resume_failures = 0;
    int resume_calls = 0;
    std::expected<void, std::string> on_resume(AppContext &) {
        ++resume_calls;
        if (resume_failures > 0) { --resume_failures; return std::unexpected("injected on_resume failure"); }
        return {};
    }
};
struct GspHost {
    bool alive = true;
    bool running() const { return alive; }
    void *handle() { return this; }
};
int replayed_updates = 0;
int esp_gsp_flush(void *, int) { return 0; }
std::expected<void, std::string> submit_gsp_component_update(void *, int) { ++replayed_updates; return {}; }
constexpr int ESP_GSP_OK = 0;
struct Host {
    bool fail_stop = false;
    bool alive = true;
    bool running() const { return alive; }
    std::expected<void, std::string> stop() {
        if (fail_stop) { return std::unexpected("injected stop failure"); }
        alive = false; return {};
    }
    ~Host() { assert(!alive); }
};
struct FakeService { void publish_message_dialog_closed(MessageDialogResult) {} };
enum class AppState { Running, Stopped, Error };
struct AppRecord {
    struct Info {
        AppState state = AppState::Running;
        std::string last_error;
    } info;
    bool gsp_suspended = false;
    std::shared_ptr<int> gsp_suspended_contract;
    std::shared_ptr<NativeApp> native_app;
    AppContext *context = nullptr;
    std::unique_ptr<GspHost> gsp_host;
    std::optional<int> runtime_app_id;
    bool gsp_scene_known = false;
    int gsp_current_scene = 0;
    std::vector<int> gsp_scene_updates;
};
struct FakeTaskScheduler {
    bool is_running() const { return false; }
    bool post(std::function<void()>, std::nullptr_t, const char *) {
        assert(false && "Board lifecycle slices must not schedule desktop dialog work");
        return false;
    }
};
class Shell;
class TestOwner {
public:
    struct Impl {
        struct MessageDialogRequestRecord {
            int request_id; int app_id;
            MessageDialogOptions options;
            MessageDialogResultHandler handler;
        };
        struct Config {
            void *gsp_desktop_display = nullptr;
        } config_;
        FakeTaskScheduler *task_scheduler_ = nullptr;
        std::map<int, AppRecord> app_records_;
        std::shared_ptr<Shell> gsp_shell_app_;
        int gsp_shell_app_id_ = 99;
        bool gsp_launching_app_ = false;
        bool gsp_stopping_ = false;
        int load_failures = 0;
        int load_calls = 0;
        int stop_session_calls = 0;
        int runtime_lifecycle_calls = 0;
        int next_message_dialog_request_id_ = 1;
        std::map<int, MessageDialogRequestRecord> message_dialog_requests_;
        std::optional<int> active_message_dialog_request_id_;
        std::vector<int> queued_message_dialog_requests_;
        FakeService *system_service_ = nullptr;
        bool should_defer_gui_task(const char *) { return false; }
        std::expected<void, std::string> post_gui_input_task(std::function<void()> f) { f(); return {}; }
        std::expected<AppRecord *, std::string> get_record(int app_id) {
            auto it = app_records_.find(app_id);
            if (it == app_records_.end()) { return std::unexpected("App not found"); }
            return &it->second;
        }
        template<class Result, class Fn>
        Result run_task_sync(const char *, Fn fn, Result) { return fn(); }
        bool gsp_session_running(const AppRecord &record) const {
            return record.gsp_host && record.gsp_host->running();
        }
        std::expected<void, std::string> stop_gsp_session(AppRecord &record) {
            ++stop_session_calls; record.gsp_host.reset(); return {};
        }
        std::expected<void, std::string> ensure_gsp_loaded(AppRecord &record) {
            ++load_calls;
            if (load_failures > 0) { --load_failures; return std::unexpected("injected GSP load failure"); }
            record.gsp_host = std::make_unique<GspHost>(); return {};
        }
        std::expected<void, std::string> call_runtime_lifecycle(
            AppRecord &, const char *, std::initializer_list<std::string>) {
            ++runtime_lifecycle_calls; return {};
        }
    };
    std::unique_ptr<Impl> impl_ = std::make_unique<Impl>();
    std::deque<std::function<void()>> tasks;
    Shell *shell = nullptr;
    int shown = 0;
    uint64_t next_task_id = 1;
    std::map<uint64_t, std::function<void()>> delayed_tasks;
    int stopped_apps = 0;
    int started_shells = 0;
    bool fail_stop_app = false;
    bool retain_session_on_stop_failure = false;
    TestOwner() { impl_->app_records_.try_emplace(42); }
    bool is_blocking_runtime_service_call_context() const { return false; }
    std::expected<void, std::string> on_show_message_dialog(int, int, const MessageDialogOptions &);
    void on_hide_message_dialog(int, int);
    void on_message_dialog_idle() {}
    std::expected<void, std::string> hide_app_keyboard(int, int);
    std::expected<void, std::string> post_system_app_task(std::function<void()> task) {
        tasks.push_back(std::move(task)); return {};
    }
    std::expected<uint64_t, std::string> post_system_app_delayed_task(int, std::function<void()> task) {
        const auto id = next_task_id++; delayed_tasks.emplace(id, std::move(task)); return id;
    }
    void cancel_system_app_delayed_task(uint64_t id) { delayed_tasks.erase(id); }
    void run_delayed() {
        assert(!delayed_tasks.empty());
        auto it = delayed_tasks.begin(); auto task = std::move(it->second); delayed_tasks.erase(it); task();
    }
    bool has_queued_message_dialogs() const { return !impl_->queued_message_dialog_requests_.empty(); }
    std::expected<void, std::string> resume_gsp_session_after_modal(AppId);
    std::expected<void, std::string> stop_app(AppId id) {
        assert(impl_->gsp_launching_app_); ++stopped_apps;
        auto &record = impl_->app_records_.at(id);
        if (fail_stop_app) {
            record.info.state = AppState::Error;
            if (!retain_session_on_stop_failure) { record.gsp_host.reset(); }
            return std::unexpected("injected app stop failure");
        }
        record.info.state = AppState::Stopped;
        record.gsp_suspended = false; record.gsp_host.reset(); return {};
    }
    std::expected<void, std::string> start_app(AppId id) {
        assert(id == impl_->gsp_shell_app_id_); ++started_shells; return {};
    }
    void drain() { while (!tasks.empty()) { auto f = std::move(tasks.front()); tasks.pop_front(); f(); } }
    std::expected<MessageDialogRequestId, std::string> enqueue_message_dialog(
        AppId, bool, MessageDialogOptions, MessageDialogResultHandler);
    std::expected<void, std::string> show_next_message_dialog();
    std::expected<void, std::string> close_message_dialog(
        AppId, MessageDialogRequestId, int32_t, MessageDialogCloseReason, bool, bool);
    void remove_queued_message_dialog(MessageDialogRequestId);
    void publish_message_dialog_closed(MessageDialogResult, MessageDialogResultHandler);
};
constexpr auto SYSTEM_GUI_INPUT_TASK_GROUP = "gui";
constexpr auto SYSTEM_APP_TASK_GROUP = "app";
class Shell {
public:
    struct ModalCallbackGate {
        boost::mutex mutex;
        Shell *shell = nullptr;
        std::atomic<uint64_t> epoch{0};
    };
    explicit Shell(TestOwner &owner) : owner_(owner) { owner.shell = this; }
    TestOwner &owner_;
    std::unique_ptr<Host> modal_host_;
    std::shared_ptr<ModalCallbackGate> modal_gate_ = std::make_shared<ModalCallbackGate>();
    std::shared_ptr<ModalCallbackGate> sntp_gate_ = std::make_shared<ModalCallbackGate>();
    bool closing_modals_ = false;
    int suspended_app_id_ = 0;
    int dialog_request_id_ = 0;
    uint64_t modal_resume_retry_task_id_ = 0;
    uint64_t modal_resume_generation_ = 0;
    unsigned modal_resume_attempts_ = 0;
    int keyboard_request_id_ = 0;
    int keyboard_owner_ = 0;
    AppContext *context_ = nullptr;
    int clock_timer_id_ = 0;
    service::Connection sntp_event_connection_;
'''

TAIL = r'''
};
struct SessionTracker {
    Shell &shell;
    std::map<int, bool> apps_;
    std::shared_ptr<int> gsp_panel_owner_ = std::make_shared<int>(1);
    std::shared_ptr<int> gsp_touch_owner_ = std::make_shared<int>(1);
    bool gsp_shell_session_running() const { return shell.modal_session_running(); }
    bool gsp_session_running(bool running) const { return running; }
    std::vector<std::shared_ptr<bool>> snapshot_app_records() const {
        std::vector<std::shared_ptr<bool>> records;
        for (const auto &[_, running] : apps_) records.push_back(std::make_shared<bool>(running));
        return records;
    }
    bool any_gsp_session_running() const;
    void release_gsp_display_devices();
};
std::expected<void, std::string> TestOwner::on_show_message_dialog(
    int, int, const MessageDialogOptions &)
{
    if (shell->closing_modals_) { return std::unexpected("closing"); }
    ++shown; return {};
}
void TestOwner::on_hide_message_dialog(int, int) {}
std::expected<void, std::string> TestOwner::hide_app_keyboard(int, int)
{
    shell->keyboard_request_id_ = 0; return {};
}
int main()
{
    TestOwner owner;
    auto shell_owner = std::make_shared<Shell>(owner);
    owner.impl_->gsp_shell_app_ = shell_owner;
    auto &shell = *shell_owner;
    int closed = 0;
    auto handler = [&closed](MessageDialogResult r) {
        assert(r.reason == MessageDialogCloseReason::Closed);
        assert(r.button_index == -1); ++closed;
    };
    auto first = owner.enqueue_message_dialog(0, false, {}, handler);
    auto second = owner.enqueue_message_dialog(42, false, {}, handler);
    assert(first && second && owner.shown == 1);
    shell.modal_host_ = std::make_unique<Host>();
    shell.suspended_app_id_ = 42;
    shell.modal_host_->fail_stop = true;
    SessionTracker tracker{shell, {{42, false}}};
    assert(tracker.any_gsp_session_running());
    auto *retained = shell.modal_host_.get();
    assert(!shell.close_modals_for_app_switch());
    assert(shell.modal_host_.get() == retained && shell.modal_session_running());
    assert(owner.impl_->active_message_dialog_request_id_ == first && closed == 0);
    assert(shell.suspended_app_id_ == 42);
    tracker.release_gsp_display_devices();
    assert(tracker.gsp_panel_owner_ && tracker.gsp_touch_owner_);
    retained->fail_stop = false;
    assert(shell.close_modals_for_app_switch());
    assert(!shell.modal_host_ && !shell.modal_session_running());
    assert(!tracker.any_gsp_session_running());
    tracker.release_gsp_display_devices();
    assert(!tracker.gsp_panel_owner_ && !tracker.gsp_touch_owner_);
    assert(closed == 2 && owner.impl_->message_dialog_requests_.empty());
    assert(!owner.impl_->active_message_dialog_request_id_);
    assert(owner.impl_->queued_message_dialog_requests_.empty());
    assert(owner.shown == 1 && !shell.closing_modals_);
    auto third = owner.enqueue_message_dialog(0, false, {}, handler);
    assert(third && owner.shown == 2);
    assert(shell.close_modals_for_app_switch());
    assert(closed == 3);
    std::cout << "F1: system/app queued requests close once; subsequent dialog displays\n";
    std::cout << "F2: injected stop failure preserves running host and suspended owner; retry succeeds\n";
    AppContext context;
    shell.context_ = &context;
    shell.subscribe_sntp_events();
    auto late_callback = service::helper::SNTP::callback;
    late_callback("timezone", {});
    assert(context.clock.starts == 0 && owner.tasks.size() == 1);
    owner.drain();
    assert(context.clock.starts == 1);
    late_callback("timezone", {});
    shell.disconnect_sntp_events();
    shell.context_ = nullptr;
    owner.drain();
    assert(context.clock.starts == 1);
    late_callback("timezone", {});
    assert(owner.tasks.empty());
    shell.context_ = &context;
    shell.subscribe_sntp_events();
    late_callback("timezone", {});
    assert(owner.tasks.empty());
    service::helper::SNTP::callback("timezone", {});
    shell.disconnect_sntp_events();
    shell.subscribe_sntp_events();
    owner.drain();
    assert(context.clock.starts == 1);
    service::helper::SNTP::callback("timezone", {});
    owner.drain();
    assert(context.clock.starts == 2);
    shell.disconnect_sntp_events();
    shell.context_ = nullptr;
    std::cout << "F3: service callback only enqueues; stop/restart rejects stale queued work and late callbacks\n";

    auto &app = owner.impl_->app_records_.at(42);
    app.info.state = AppState::Running;
    app.gsp_suspended = true;
    app.native_app = std::make_shared<NativeApp>();
    app.context = &context;
    owner.impl_->load_failures = 1;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    assert(app.gsp_suspended && shell.suspended_app_id_ == 42 && owner.delayed_tasks.size() == 1);
    owner.run_delayed();
    assert(!app.gsp_suspended && shell.suspended_app_id_ == 0 && app.native_app->resume_calls == 1);

    app.gsp_suspended = true;
    app.gsp_host.reset();
    app.native_app->resume_failures = 1;
    shell.suspended_app_id_ = 42;
    const int stopped_before = owner.impl_->stop_session_calls;
    shell.resume_suspended_app();
    assert(app.gsp_suspended && app.gsp_host && owner.delayed_tasks.size() == 1);
    owner.run_delayed();
    assert(!app.gsp_suspended && owner.impl_->stop_session_calls == stopped_before + 1);

    app.gsp_suspended = true;
    app.gsp_host.reset();
    app.native_app.reset();
    app.context = nullptr;
    app.runtime_app_id = 7;
    app.gsp_scene_known = true;
    app.gsp_scene_updates = {1, 2};
    owner.impl_->load_failures = 1;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    assert(app.gsp_suspended && shell.suspended_app_id_ == 42);
    owner.run_delayed();
    assert(!app.gsp_suspended && replayed_updates == 2 && owner.impl_->runtime_lifecycle_calls == 1);

    app.gsp_suspended = true;
    app.gsp_host.reset();
    owner.impl_->load_failures = 4;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    while (!owner.delayed_tasks.empty()) { owner.run_delayed(); }
    assert(owner.stopped_apps == 1 && owner.started_shells == 1 && !app.gsp_suspended);
    assert(shell.suspended_app_id_ == 0 && !owner.impl_->gsp_launching_app_);

    app.info.state = AppState::Running;
    app.gsp_suspended = true;
    app.gsp_host.reset();
    owner.fail_stop_app = true;
    owner.impl_->load_failures = 4;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    while (!owner.delayed_tasks.empty()) { owner.run_delayed(); }
    assert(app.info.state == AppState::Error && owner.stopped_apps == 2 && owner.started_shells == 2);

    app.info.state = AppState::Running;
    app.gsp_suspended = true;
    app.native_app = std::make_shared<NativeApp>();
    app.native_app->resume_failures = 4;
    app.context = &context;
    owner.retain_session_on_stop_failure = true;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    while (!owner.delayed_tasks.empty()) { owner.run_delayed(); }
    assert(app.info.state == AppState::Error && app.gsp_host && shell.suspended_app_id_ == 42);
    assert(owner.stopped_apps == 3 && owner.started_shells == 2);
    owner.fail_stop_app = false;
    owner.retain_session_on_stop_failure = false;
    app.gsp_host.reset();
    assert(shell.close_modals_for_app_switch());

    app.info.state = AppState::Running;
    app.gsp_suspended = true;
    app.native_app.reset();
    app.context = nullptr;
    owner.impl_->load_failures = 1;
    shell.suspended_app_id_ = 42;
    shell.resume_suspended_app();
    auto stale_retry = owner.delayed_tasks.begin()->second;
    const auto loads_before_switch = owner.impl_->load_calls;
    assert(shell.close_modals_for_app_switch());
    stale_retry();
    assert(owner.delayed_tasks.empty() && owner.impl_->load_calls == loads_before_switch);
    std::cout << "F4: native/runtime resume retries, fallback, and app-switch cancellation\n";
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--boost-include', default=os.environ.get('BOOST_INCLUDEDIR'))
    args = parser.parse_args()
    shell_source = (ROOT / 'src/system/core.cpp').read_text()
    manager_source = (ROOT / 'src/app/manager.cpp').read_text()
    gui_source = (ROOT / 'src/system/gui.cpp').read_text()
    shell_methods = [
        '    std::expected<void, std::string> close_modals_for_app_switch()',
        '    bool modal_session_running() const',
        '    void detach_modal_callbacks()',
        '    std::expected<void, std::string> stop_modal_session()',
        '    void disconnect_sntp_events()',
        '    void subscribe_sntp_events()',
        '    void stop_clock_timer()',
        '    void cancel_modal_resume_retry()',
        '    void resume_suspended_app()',
    ]
    manager_methods = [
        'std::expected<MessageDialogRequestId, std::string> System::enqueue_message_dialog(',
        'std::expected<void, std::string> System::show_next_message_dialog()',
        'void System::remove_queued_message_dialog(',
        'void System::publish_message_dialog_closed(',
        'std::expected<void, std::string> System::close_message_dialog(',
    ]
    program = PREAMBLE + '\n'.join(
        extract(shell_source, m).replace('GspSystemShellApp', 'Shell').replace('std::weak_ptr<IApp>', 'std::weak_ptr<Shell>')
        for m in shell_methods
    ) + TAIL
    program += '\n'.join(extract(manager_source, m).replace('System::', 'TestOwner::')
                         .replace('Impl::MessageDialogRequestRecord', 'TestOwner::Impl::MessageDialogRequestRecord')
                         for m in manager_methods)
    program += '\n'.join(extract(gui_source, method).replace('System::Impl::', 'SessionTracker::')
                         for method in ['bool System::Impl::any_gsp_session_running() const',
                                        'void System::Impl::release_gsp_display_devices()'])
    program += extract(gui_source, 'std::expected<void, std::string> System::resume_gsp_session_after_modal(').replace(
        'System::resume_gsp_session_after_modal', 'TestOwner::resume_gsp_session_after_modal'
    )
    program = '#define CONFIG_BROOKESIA_GUI_BACKEND_GSP 1\n' + program
    with tempfile.TemporaryDirectory(prefix='gsp-shell-lifecycle-') as temporary:
        directory = pathlib.Path(temporary)
        source = directory / 'test.cpp'
        source.write_text(program)
        binary = directory / 'test'
        compiler = [args.cxx, '-std=c++23', '-Wall', '-Wextra', '-Werror']
        if args.boost_include:
            compiler.append(f'-I{args.boost_include}')
        subprocess.run(compiler + [str(source), '-o', str(binary), '-pthread'], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
