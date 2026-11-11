# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise App Store HTTP subscriptions with real signal lifetimes and fake services."""
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
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/signal.hpp"

#define BROOKESIA_LOGE(...) ((void)0)
#define BROOKESIA_LOGW(...) ((void)0)
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)
namespace boost::json { struct object { int value = 0; }; }
namespace system_core {
using TimerId = int;
constexpr TimerId INVALID_TIMER_ID = 0;
struct Timer {
    bool fail = false;
    int starts = 0;
    int stops = 0;
    std::expected<int, std::string> start_periodic(const char *, int) {
        if (fail) return std::unexpected("injected timer failure");
        return ++starts;
    }
    bool stop(int) { ++stops; return true; }
};
struct AppContext { Timer value; Timer &timer() { return value; } };
}
using esp_brookesia::lib_utils::scoped_connection;
using esp_brookesia::lib_utils::signal;
constexpr auto HTTP_EVENT_TIMER_NAME = "http";
constexpr int HTTP_EVENT_POLL_INTERVAL_MS = 100;

struct HttpHelper {
    enum class EventId { RequestProgress, RequestCompleted, RequestFailed, RequestCanceled, GeneralStateChanged };
    using RequestCallback = std::function<void(const std::string &, double, const boost::json::object &)>;
    using StateCallback = std::function<void(const std::string &, const std::string &)>;
    using RequestSignal = signal<void(const std::string &, double, const boost::json::object &)>;
    static inline RequestSignal progress, completed, failed, canceled;
    static inline signal<void(const std::string &, const std::string &)> state;
    static inline std::atomic_bool block_callback = false;
    static inline std::promise<void> *entered = nullptr;
    static inline std::shared_future<void> release;
    static scoped_connection subscribe_event(EventId id, RequestCallback callback) {
        auto wrapper = [callback = std::move(callback)](const std::string &name, double request_id,
                                                        const boost::json::object &payload) {
            if (block_callback.load()) { entered->set_value(); release.wait(); }
            callback(name, request_id, payload);
        };
        switch (id) {
        case EventId::RequestProgress: return progress.connect(wrapper);
        case EventId::RequestCompleted: return completed.connect(wrapper);
        case EventId::RequestFailed: return failed.connect(wrapper);
        case EventId::RequestCanceled: return canceled.connect(wrapper);
        default: return {};
        }
    }
    static scoped_connection subscribe_event(EventId, StateCallback callback) {
        return state.connect(std::move(callback));
    }
};

class AppStoreApp {
public:
    enum class StartupLoadPhase { None };
    enum class LocalPackageScanPhase { None };
    enum class MessageDialogPurpose { None };
    enum class PendingHttpEventType { Progress, Completed, Failed, Canceled, GeneralStateChanged };
    struct PendingHttpEvent {
        PendingHttpEventType type = PendingHttpEventType::Completed;
        double request_id = 0;
        boost::json::object response;
        std::string state;
    };
    struct Impl {
        struct HttpEventMailbox {
            std::mutex mutex;
            bool open = true;
            std::vector<PendingHttpEvent> events;
        };
    };
    system::core::AppContext *context_ = nullptr;
    std::vector<scoped_connection> http_event_connections_;
    std::shared_ptr<Impl::HttpEventMailbox> http_event_mailbox_;
    system::core::TimerId http_event_timer_id_ = 0;
    bool http_available_ = true;
    unsigned async_generation_ = 0;
    std::vector<std::string> delivered;
    int binding_releases = 0;
    std::vector<int> entries_, local_packages_, installed_runtime_apps_, local_package_by_manifest_id_;
    std::vector<int> installed_manifest_ids_, installed_version_by_manifest_id_, i18n_strings_;
    std::vector<int> terminal_download_request_ids_, startup_cache_candidates_, local_package_scan_dirs_;
    std::vector<int> local_package_scan_candidates_, local_package_scan_results_, local_package_scan_seen_keys_;
    std::string applied_i18n_locale_, status_text_, storage_text_, refresh_dialog_message_;
    std::string pending_delete_local_package_name_, pending_delete_local_package_path_;
    std::string pending_delete_success_package_name_, pending_delete_success_package_path_, startup_cache_last_error_;
    scoped_connection primary_action_connection_, local_delete_action_connection_;
    unsigned refresh_request_id_ = 0, startup_cache_cursor_ = 0, local_package_scan_dir_cursor_ = 0;
    unsigned local_package_scan_candidate_cursor_ = 0;
    bool startup_cache_load_in_progress_ = false, local_package_scan_in_progress_ = false;
    bool local_package_scan_refresh_view_ = false, refresh_in_progress_ = false;
    bool storage_match_warning_logged_ = false, time_sync_waiting_ = false;
    StartupLoadPhase startup_load_phase_ = StartupLoadPhase::None;
    LocalPackageScanPhase local_package_scan_phase_ = LocalPackageScanPhase::None;
    MessageDialogPurpose message_dialog_purpose_ = MessageDialogPurpose::None;
    std::optional<int> pending_view_mode_, http_general_state_;
    explicit AppStoreApp(system::core::AppContext &context) : context_(&context) {}
    void release_http_service() { http_available_ = false; ++binding_releases; }
    void cancel_deferred_operation(system::core::AppContext &) {}
    void cancel_startup_load(system::core::AppContext &) {}
    void cancel_local_package_scan(system::core::AppContext &) {}
    void cancel_remote_refresh() {}
    void stop_view_mode_load_timer(system::core::AppContext &) {}
    void stop_refresh_icon_timer(system::core::AppContext &) {}
    void stop_size_metadata_timer(system::core::AppContext &) {}
    void stop_time_sync_timers(system::core::AppContext &) {}
    void hide_message_dialog_if_visible(system::core::AppContext &) {}
    void cancel_download(int &) {}
    void clear_entry_views(system::core::AppContext &) {}
    void unregister_icons(system::core::AppContext &) {}
    void release_device_service() {}
    void release_storage_service() {}
    void reset_refresh_icon_state() {}
    std::expected<void, std::string> on_stop(system::core::AppContext &context);
    void disconnect_http_events();
    void subscribe_http_events();
    void stop_http_event_timer(system::core::AppContext &context);
    void process_pending_http_events(system::core::AppContext &context);
    void handle_request_progress(double, const boost::json::object &v) { delivered.push_back("progress:" + std::to_string(v.value)); }
    void handle_request_completed(double, const boost::json::object &) { delivered.push_back("completed"); }
    void handle_request_failed(double, const boost::json::object &) { delivered.push_back("failed"); }
    void handle_request_canceled(double, const boost::json::object &) { delivered.push_back("canceled"); }
    void handle_http_general_state_changed(const std::string &s) { delivered.push_back(s); }
};
'''

TAIL = r'''
int main()
{
    system::core::AppContext context;
    {
        AppStoreApp app(context);
        app.subscribe_http_events();
        assert(app.http_available_ && app.http_event_connections_.size() == 5);
        HttpHelper::progress("progress", 7, {1});
        HttpHelper::progress("progress", 7, {2});
        HttpHelper::completed("completed", 7, {});
        HttpHelper::state("state", "Started");
        app.process_pending_http_events(context);
        assert((app.delivered == std::vector<std::string>{"progress:2", "completed", "Started"}));
        assert(app.on_stop(context));
        assert(context.value.stops == 1);
    }
    {
        auto app = std::make_unique<AppStoreApp>(context);
        app->subscribe_http_events();
        std::weak_ptr<AppStoreApp::Impl::HttpEventMailbox> mailbox = app->http_event_mailbox_;
        std::promise<void> entered;
        std::promise<void> release;
        HttpHelper::entered = &entered;
        HttpHelper::release = release.get_future().share();
        HttpHelper::block_callback = true;
        std::thread http([] { HttpHelper::progress("progress", 8, {3}); });
        entered.get_future().wait();
        signal<void()> stop_event;
        auto connection = stop_event.connect([&] { assert(app->on_stop(context)); app.reset(); });
        stop_event();
        assert(!app && mailbox.expired());
        release.set_value();
        http.join();
        HttpHelper::block_callback = false;
        connection.disconnect();
    }
    {
        AppStoreApp app(context);
        context.value.fail = true;
        app.subscribe_http_events();
        assert(!app.http_available_ && app.binding_releases == 1 && app.http_event_connections_.empty() &&
               !app.http_event_mailbox_);
        context.value.fail = false;
        app.http_available_ = true;
        app.subscribe_http_events();
        assert(app.http_available_ && app.http_event_connections_.size() == 5);
        assert(app.on_stop(context));
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++-13'))
    parser.add_argument('--sanitizer', choices=('address', 'thread'))
    args = parser.parse_args()
    component = ROOT / 'app/brookesia_app_store'
    storage = (component / 'src/storage/storage.ipp').read_text()
    http = (component / 'src/network/http.ipp').read_text()
    lifecycle = (component / 'src/app/lifecycle.ipp').read_text()
    program = PREAMBLE + '\n'.join(extract(storage, method) for method in (
        'void AppStoreApp::disconnect_http_events()', 'void AppStoreApp::subscribe_http_events()'))
    program += '\n'.join(extract(http, method) for method in (
        'void AppStoreApp::stop_http_event_timer(', 'void AppStoreApp::process_pending_http_events('))
    program += extract(lifecycle, 'std::expected<void, std::string> AppStoreApp::on_stop(')
    program += TAIL
    program = program.replace('system::core::', 'system_core::')
    with tempfile.TemporaryDirectory(prefix='app-store-http-lifetime-') as temporary:
        source = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source.write_text(program)
        command = [args.cxx, '-std=c++23', '-Wall', '-Wextra', '-Werror',
                   '-Wno-missing-field-initializers', '-pthread',
                   f'-I{ROOT / "utils/brookesia_lib_utils/include"}', str(source), '-o', str(binary)]
        if args.sanitizer:
            command.extend([f'-fsanitize={args.sanitizer}', '-fno-omit-frame-pointer'])
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('App Store HTTP subscription lifetime tests passed')


if __name__ == '__main__':
    main()
