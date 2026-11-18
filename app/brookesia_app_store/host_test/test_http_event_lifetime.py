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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "boost/json.hpp"
#include <vector>
#include "brookesia/lib_utils/signal.hpp"

#define BROOKESIA_LOGE(...) ((void)0)
#define BROOKESIA_LOGW(...) ((void)0)
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)

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
namespace service {
struct FunctionResult { bool success = true; double id = 0; };
struct ServiceBase { using FunctionResultHandler = std::function<void(FunctionResult &&)>; };
}
template<class T> boost::json::value to_json(const T &) { return boost::json::object{}; }
#define BROOKESIA_DESCRIBE_TO_JSON(v) to_json(v)

struct HttpHelper {
    enum class EventId { RequestProgress, RequestCompleted, RequestFailed, RequestCanceled, GeneralStateChanged };
    using RequestCallback = std::function<void(const std::string &, double, const boost::json::object &)>;
    using StateCallback = std::function<void(const std::string &, const std::string &)>;
    using RequestSignal = signal<void(const std::string &, double, const boost::json::object &)>;
    static inline RequestSignal progress, completed, failed, canceled;
    enum class FunctionId { CancelRequest, RequestAsync };
    struct HttpRequest { uint32_t timeout_ms = 15000; uint32_t retry_count = 1; };
    static inline bool defer_submission = false, reject_submission = false, early_completion = false;
    static inline uint64_t next_id = 50000;
    static inline std::vector<std::function<void()>> submissions;
    template<class T> static std::expected<T, std::string> process_function_result(service::FunctionResult &result) {
        if (!result.success) return std::unexpected("injected submit failure");
        return static_cast<T>(result.id);
    }
    static bool call_function_async(FunctionId, const boost::json::object &, service::ServiceBase::FunctionResultHandler h) {
        if (reject_submission) return false;
        const double id = next_id++;
        if (early_completion) completed("early", id, {});
        auto deliver = [h, id] { h({true, id}); };
        if (defer_submission) submissions.push_back(std::move(deliver));
        else deliver();
        return true;
    }
    static inline unsigned cancellations = 0;
    static inline std::vector<uint64_t> canceled_ids;
    static bool call_function_async(FunctionId, double id) { ++cancellations; canceled_ids.push_back(id); return true; }
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

namespace service::helper { using Http = ::HttpHelper; }
class AppStoreApp {
public:
    enum class StartupLoadPhase { None };
    enum class LocalPackageScanPhase { None };
    enum class MessageDialogPurpose { None };
    /* PENDING_HTTP_EVENT_TYPES */
    struct Impl {
        /* HTTP_MAILBOX */
    };
    system::core::AppContext *context_ = nullptr;
    std::vector<scoped_connection> http_event_connections_;
    std::shared_ptr<Impl::HttpEventMailbox> http_event_mailbox_;
    system::core::TimerId http_event_timer_id_ = 0;
    bool http_available_ = true;
    unsigned async_generation_ = 0;
    std::vector<std::string> delivered;
    std::vector<uint64_t> completed_ids, failed_ids;
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
    using HttpRequestSubmitSuccessHandler = std::function<void(uint64_t)>;
    using HttpRequestSubmitFailureHandler = std::function<void(std::string)>;
    bool submit_http_request_async(system::core::AppContext &, HttpHelper::HttpRequest,
                                   HttpRequestSubmitSuccessHandler, HttpRequestSubmitFailureHandler);
    void handle_http_request_submit_result(uint64_t, HttpRequestSubmitSuccessHandler,
                                           HttpRequestSubmitFailureHandler, service::FunctionResult &&);
    void disconnect_http_events();
    void subscribe_http_events();
    void stop_http_event_timer(system::core::AppContext &context);
    void process_pending_http_events(system::core::AppContext &context);
    void handle_request_progress(double, const boost::json::object &v) { delivered.push_back("progress:" + std::to_string(v.at("value").as_int64())); }
    void handle_request_completed(double id, const boost::json::object &) { delivered.push_back("completed"); completed_ids.push_back(id); }
    void handle_request_failed(double id, const boost::json::object &) { delivered.push_back("failed"); failed_ids.push_back(id); }
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
        assert(app.http_event_mailbox_->begin_submission());
        assert(app.http_event_mailbox_->finish_submission(7));
        HttpHelper::progress("progress", 7, {{"value", 1}});
        HttpHelper::progress("progress", 7, {{"value", 2}});
        HttpHelper::completed("completed", 7, {});
        HttpHelper::state("state", "Started");
        app.process_pending_http_events(context);
        assert((app.delivered == std::vector<std::string>{"completed", "Started"}));
        assert(app.on_stop(context));
        assert(context.value.stops == 1);
    }
    {
        AppStoreApp app(context);
        app.subscribe_http_events();
        auto mailbox = app.http_event_mailbox_;
        assert(mailbox->begin_submission() && mailbox->finish_submission(7));
        const boost::json::object huge{{"body", std::string(2 * mailbox->MAX_BYTES, 'x')}};
        for (int id = 100; id < 20100; ++id) {
            HttpHelper::completed("other app", id, huge);
        }
        assert(mailbox->events.empty() && mailbox->pending_bytes == 0);
        for (int i = 0; i < 20000; ++i) {
            HttpHelper::progress("progress", 7, {{"value", i}});
            HttpHelper::state("state", "Started");
        }
        assert(mailbox->events.size() == 2 && mailbox->pending_bytes <= mailbox->MAX_BYTES);
        HttpHelper::completed("completed", 7, {});
        app.process_pending_http_events(context);
        assert(std::count(app.delivered.begin(), app.delivered.end(), "completed") == 1);
        assert(mailbox->requests.empty());

        // A response that completes before RequestAsync returns must survive id registration.
        assert(mailbox->begin_submission());
        HttpHelper::completed("early", 8, {});
        assert(mailbox->finish_submission(8));
        app.process_pending_http_events(context);
        assert(app.delivered.back() == "completed");

        // Oversized owned responses fail explicitly, release admission and allow a retry.
        assert(mailbox->begin_submission() && mailbox->finish_submission(9));
        HttpHelper::completed("oversized", 9, huge);
        assert(mailbox->events.empty() && mailbox->pending_bytes <= mailbox->MAX_BYTES);
        app.process_pending_http_events(context);
        assert(app.delivered.back() == "failed" && HttpHelper::cancellations == 1);
        assert(mailbox->requests.empty());

        const boost::json::object body{{"body", std::string(mailbox->MAX_BYTES / 8, 'x')}};
        for (uint64_t id = 10; id < 10 + mailbox->MAX_REQUESTS; ++id) {
            assert(mailbox->begin_submission() && mailbox->finish_submission(id));
            HttpHelper::completed("budget", id, body);
            assert(mailbox->events.size() <= mailbox->MAX_EVENTS && mailbox->pending_bytes <= mailbox->MAX_BYTES);
        }
        assert(!mailbox->begin_submission());
        app.process_pending_http_events(context);
        assert(mailbox->requests.empty() && mailbox->events.empty() && mailbox->pending_bytes == 0);
        assert(mailbox->begin_submission() && mailbox->finish_submission(99));
        HttpHelper::completed("retry", 99, {});
        app.process_pending_http_events(context);
        assert(app.delivered.back() == "completed");
        assert(app.on_stop(context));
    }
    {
        AppStoreApp app(context);
        app.subscribe_http_events();
        auto mailbox = app.http_event_mailbox_;
        assert(mailbox->begin_submission() && mailbox->begin_submission());
        const boost::json::object huge{{"body", std::string(2 * mailbox->MAX_BYTES, 'x')}};
        HttpHelper::completed("lost A before id registration", 90001, huge);
        assert(mailbox->finish_submission(90001));
        assert(mailbox->finish_submission(90002));
        HttpHelper::completed("healthy B", 90002, {});
        const auto canceled_before = HttpHelper::canceled_ids.size();
        app.process_pending_http_events(context);
        assert((app.completed_ids == std::vector<uint64_t>{90002}));
        assert((app.failed_ids == std::vector<uint64_t>{90001}));
        assert(HttpHelper::canceled_ids.size() == canceled_before + 1 && HttpHelper::canceled_ids.back() == 90001);

        // B may still be running when the lost A response is drained.
        assert(mailbox->begin_submission() && mailbox->begin_submission());
        HttpHelper::completed("lost A", 90003, huge);
        assert(mailbox->finish_submission(90003) && mailbox->finish_submission(90004));
        app.process_pending_http_events(context);
        assert(app.failed_ids.back() == 90003 && mailbox->requests.contains(90004));
        assert(HttpHelper::canceled_ids.back() == 90003);
        HttpHelper::completed("later B", 90004, {});
        app.process_pending_http_events(context);
        assert(app.completed_ids.back() == 90004 && HttpHelper::canceled_ids.back() == 90003);

        // Losing another application's early result cannot fail either owned request.
        assert(mailbox->begin_submission() && mailbox->begin_submission());
        HttpHelper::completed("other app", 90005, huge);
        assert(mailbox->finish_submission(90006) && mailbox->finish_submission(90007));
        app.process_pending_http_events(context);
        assert(mailbox->requests.size() == 2 && HttpHelper::canceled_ids.back() == 90003);
        HttpHelper::completed("owned", 90006, {});
        HttpHelper::completed("owned", 90007, {});
        app.process_pending_http_events(context);
        assert(mailbox->requests.empty() && app.completed_ids.back() == 90007);
        assert(app.on_stop(context));
    }
    {
        AppStoreApp app(context);
        app.subscribe_http_events();
        auto mailbox = app.http_event_mailbox_;
        assert(mailbox->begin_submission() && mailbox->begin_submission());
        const boost::json::object huge{{"body", std::string(2 * mailbox->MAX_BYTES, 'x')}};
        for (int id = 100000; id < 120000; ++id) {
            HttpHelper::completed("other app", id, huge);
            assert(mailbox->lost_early_requests.size() <= mailbox->MAX_EVENTS);
            assert(mailbox->pending_bytes <= mailbox->MAX_BYTES && mailbox->events.size() <= mailbox->MAX_EVENTS);
        }
        assert(mailbox->finish_submission(130000) && mailbox->finish_submission(130001));
        app.process_pending_http_events(context);
        assert(mailbox->requests.size() == 2 && app.failed_ids.empty());
        // Activity renews the fallback even when the earlier loss history was saturated.
        mailbox->requests.at(130001).missing_result_deadline = std::chrono::steady_clock::now();
        HttpHelper::progress("healthy", 130001, {{"value", 50}});
        app.process_pending_http_events(context);
        assert(mailbox->requests.size() == 2 && app.failed_ids.empty());
        mailbox->requests.at(130000).missing_result_deadline = std::chrono::steady_clock::now();
        app.process_pending_http_events(context);
        assert((app.failed_ids == std::vector<uint64_t>{130000}) && mailbox->requests.size() == 1);
        HttpHelper::completed("healthy", 130001, {});
        app.process_pending_http_events(context);
        assert((app.completed_ids == std::vector<uint64_t>{130001}) && mailbox->requests.empty());
        assert(app.on_stop(context));
    }
    {
        auto app = std::make_unique<AppStoreApp>(context);
        app->subscribe_http_events();
        bool accepted = false;
        HttpHelper::early_completion = true;
        assert(app->submit_http_request_async(context, {}, [&](uint64_t) { accepted = true; }, nullptr));
        assert(accepted && app->http_event_mailbox_->requests.size() == 1);
        app->process_pending_http_events(context);
        assert(app->delivered.back() == "completed");
        HttpHelper::early_completion = false;
        HttpHelper::reject_submission = true;
        assert(!app->submit_http_request_async(context, {}, nullptr, nullptr));
        assert(app->http_event_mailbox_->pending_submissions == 0);
        HttpHelper::reject_submission = false;
        HttpHelper::defer_submission = true;
        assert(app->submit_http_request_async(context, {}, [](uint64_t) { assert(false); }, nullptr));
        const auto canceled_before = HttpHelper::cancellations;
        assert(app->on_stop(context));
        app.reset();
        auto result = std::move(HttpHelper::submissions.back());
        HttpHelper::submissions.clear();
        result();
        assert(HttpHelper::cancellations == canceled_before + 1);
        HttpHelper::defer_submission = false;
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
        std::thread http([] { HttpHelper::progress("progress", 8, {{"value", 3}}); });
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
    parser.add_argument('--boost-root', default=os.environ.get('BOOST_ROOT'))
    args = parser.parse_args()
    component = ROOT / 'app/brookesia_app_store'
    header = (component / 'include/brookesia/app_store/app_store_app.hpp').read_text()
    storage = (component / 'src/storage/storage.ipp').read_text()
    http = (component / 'src/network/http.ipp').read_text()
    lifecycle = (component / 'src/app/lifecycle.ipp').read_text()
    event_types = '\n'.join(extract(header, declaration) + ';' for declaration in (
        'enum class PendingHttpEventType {', 'struct PendingHttpEvent {'))
    impl = (component / 'src/private/app_store_impl.hpp').read_text()
    mailbox = extract(impl, '    struct HttpEventMailbox {') + ';'
    program = PREAMBLE.replace('/* PENDING_HTTP_EVENT_TYPES */', event_types).replace('/* HTTP_MAILBOX */', mailbox)
    program += '\n'.join(extract(storage, method) for method in (
        'void AppStoreApp::disconnect_http_events()', 'void AppStoreApp::subscribe_http_events()'))
    program += '\n'.join(extract(http, method) for method in (
        'void AppStoreApp::stop_http_event_timer(', 'void AppStoreApp::process_pending_http_events(',
        'bool AppStoreApp::submit_http_request_async(', 'void AppStoreApp::handle_http_request_submit_result('))
    program += extract(lifecycle, 'std::expected<void, std::string> AppStoreApp::on_stop(')
    program += TAIL
    program = program.replace('system::core::', 'system_core::')
    with tempfile.TemporaryDirectory(prefix='app-store-http-lifetime-') as temporary:
        source = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source.write_text(program)
        command = [args.cxx, '-std=c++23', '-Wall', '-Wextra', '-Werror', '-pthread',
                   f'-I{ROOT / "utils/brookesia_lib_utils/include"}', str(source), '-o', str(binary)]
        command.append('-lboost_json')
        if args.boost_root:
            command.extend([f'-I{args.boost_root}/include', f'-L{args.boost_root}/lib'])
        if args.sanitizer:
            command.extend([f'-fsanitize={args.sanitizer}', '-fno-omit-frame-pointer'])
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('App Store HTTP subscription lifetime tests passed')


if __name__ == '__main__':
    main()
