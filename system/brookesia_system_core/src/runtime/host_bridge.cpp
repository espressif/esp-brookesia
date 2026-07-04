/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/heap_trace.hpp"
#include "private/runtime/host_bridge.hpp"
#include "private/runtime/service_json.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

#include "brookesia/service_manager.hpp"

namespace esp_brookesia::system::core {
namespace {

using RuntimeAppId = runtime::AppId;
using NativeArgs = runtime::NativeArgs;
using NativeAsyncCallback = runtime::NativeAsyncCallback;
using NativeModule = runtime::NativeModule;
using NativeResult = runtime::NativeResult;
using NativeValue = runtime::NativeValue;

constexpr const char *STORAGE_PATH_MARKER_KEY = "$brookesiaStoragePath";
constexpr const char *STORAGE_URL_MARKER_KEY = "$brookesiaStorageUrl";
constexpr const char *RUNTIME_APP_ID_CALL_CONTEXT_KEY = "brookesia.system.runtime_app_id";

bool get_string_arg(const NativeArgs &args, std::size_t index, std::string &value)
{
    if ((args.size() <= index) || !std::holds_alternative<std::string>(args[index])) {
        return false;
    }
    value = std::get<std::string>(args[index]);
    return true;
}

int64_t native_value_to_handle(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> int64_t {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            return static_cast<int64_t>(item);
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item ? 1 : 0;
        } else
        {
            return 0;
        }
    },
    value
           );
}

bool native_value_to_timeout_ms(const NativeValue &value, uint32_t &timeout_ms)
{
    constexpr auto max_timeout_ms = static_cast<uint32_t>(std::numeric_limits<int>::max());
    return std::visit(
    [&timeout_ms, max_timeout_ms](const auto & item) -> bool {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            if ((item < 0) || (item > static_cast<int64_t>(max_timeout_ms))) {
                return false;
            }
            timeout_ms = static_cast<uint32_t>(item);
            return true;
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            if ((item < 0) || (item > static_cast<double>(max_timeout_ms))) {
                return false;
            }
            const auto integer_timeout = static_cast<uint32_t>(item);
            if (static_cast<double>(integer_timeout) != item) {
                return false;
            }
            timeout_ms = integer_timeout;
            return true;
        } else
        {
            return false;
        }
    },
    value
           );
}

NativeResult make_success()
{
    return NativeValue{true};
}

std::string native_value_to_print_string(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> std::string {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, std::string>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return std::to_string(item);
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), "%g", item);
            return buffer;
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item ? "true" : "false";
        } else
        {
            return "nil";
        }
    },
    value
           );
}

std::expected<std::string, std::string> get_object_string_flexible(
    const boost::json::object &object,
    std::string_view primary_key,
    std::string_view fallback_key,
    bool required
)
{
    auto it = object.find(primary_key);
    if (it == object.end()) {
        it = object.find(fallback_key);
    }
    if (it == object.end()) {
        if (!required) {
            return std::string();
        }
        return std::unexpected("Missing storage path marker field: " + std::string(primary_key));
    }
    if (!it->value().is_string()) {
        return std::unexpected("Storage path marker field must be string: " + std::string(primary_key));
    }
    return std::string(it->value().as_string());
}

std::expected<StorageFileLocation, std::string> storage_location_from_marker(const boost::json::object &object)
{
    auto kind_string = get_object_string_flexible(object, "Kind", "kind", true);
    if (!kind_string) {
        return std::unexpected(kind_string.error());
    }

    StoragePathKind kind = StoragePathKind::AppFiles;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*kind_string, kind)) {
        return std::unexpected("Invalid storage path marker kind: " + *kind_string);
    }

    auto relative_path = get_object_string_flexible(object, "RelativePath", "relative_path", false);
    if (!relative_path) {
        return std::unexpected(relative_path.error());
    }
    auto volume_id = get_object_string_flexible(object, "VolumeId", "volume_id", false);
    if (!volume_id) {
        return std::unexpected(volume_id.error());
    }

    return StorageFileLocation{
        .kind = kind,
        .volume_id = std::move(*volume_id),
        .relative_path = std::move(*relative_path),
    };
}

std::string file_url_from_path(std::string path)
{
    return "file://" + std::move(path);
}

service::CallContext make_runtime_call_context(RuntimeAppId app_id)
{
    auto context = service::get_current_call_context();
    context[RUNTIME_APP_ID_CALL_CONTEXT_KEY] = std::to_string(app_id);
    return context;
}

} // namespace

class SystemHostBridge::Impl {
public:
    struct ServiceRecord {
        RuntimeAppId app_id = runtime::INVALID_APP_ID;
        service::ServiceBinding binding;
        std::shared_ptr<service::ServiceBase> service;
    };

    struct SubscriptionRecord {
        RuntimeAppId app_id = runtime::INVALID_APP_ID;
        service::EventRegistry::SignalConnection connection;
    };

    struct AsyncCallRecord {
        RuntimeAppId app_id = runtime::INVALID_APP_ID;
        NativeAsyncCallback callback;
        bool completed = false;
    };

    explicit Impl(std::shared_ptr<runtime::RuntimeFunctionBridge> function_bridge)
        : Impl(std::move(function_bridge), nullptr, {}, {})
    {}

    Impl(
        std::shared_ptr<runtime::RuntimeFunctionBridge> function_bridge,
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler,
        SystemHostBridge::StoragePathResolver storage_path_resolver,
        SystemHostBridge::EventDispatcher event_dispatcher
    )
        : function_bridge_(std::move(function_bridge))
        , task_scheduler_(std::move(task_scheduler))
        , storage_path_resolver_(std::move(storage_path_resolver))
        , event_dispatcher_(std::move(event_dispatcher))
    {
        auto result = function_bridge_->add_module(make_brookesia_module());
        if (!result) {
            BROOKESIA_LOGW("Failed to add system Brookesia runtime module: %1%", result.error());
        }
    }

    NativeModule make_brookesia_module();
    NativeResult service_available(const NativeArgs &args);
    NativeResult start_service(const NativeArgs &args);
    NativeResult stop_service(const NativeArgs &args);
    NativeResult call_function(const NativeArgs &args);
    void call_function_async(const NativeArgs &args, NativeAsyncCallback callback);
#if BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE
    void debug_delay_async(const NativeArgs &args, NativeAsyncCallback callback);
#endif
    NativeResult call_functions(const NativeArgs &args);
    NativeResult call_service_function(const NativeArgs &args);
    void call_service_function_async(const NativeArgs &args, NativeAsyncCallback callback);
    NativeResult call_service_functions(const NativeArgs &args);
    NativeResult subscribe_event(const NativeArgs &args);
    NativeResult subscribe_service_event(const NativeArgs &args);
    NativeResult unsubscribe_event(const NativeArgs &args);
    NativeResult current_app_context(const NativeArgs &args);
    NativeResult finish_app(const NativeArgs &args);
    NativeResult attach_app_context(const NativeArgs &args);
    NativeResult detach_app_context(const NativeArgs &args);
    NativeResult print(const NativeArgs &args);
    std::shared_ptr<AsyncCallRecord> create_async_call(RuntimeAppId app_id, NativeAsyncCallback callback);
    void complete_async_call(
        const std::shared_ptr<AsyncCallRecord> &record, NativeResult result, const char *source
    );
    void request_finish_app(RuntimeAppId id);
    bool consume_app_finish_request(RuntimeAppId id);
    void release_app_resources(RuntimeAppId id);
    void release_all_app_resources();
    std::expected<void, std::string> resolve_storage_path_markers(RuntimeAppId app_id, boost::json::value &value);
    std::expected<service::FunctionParameterMap, std::string> parse_params_with_storage_paths(
        RuntimeAppId app_id,
        std::string_view params_json
    );
    std::expected<std::vector<service::FunctionCall>, std::string> parse_calls_with_storage_paths(
        RuntimeAppId app_id,
        std::string_view calls_json
    );

    std::shared_ptr<runtime::RuntimeFunctionBridge> function_bridge_;
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    SystemHostBridge::StoragePathResolver storage_path_resolver_;
    SystemHostBridge::EventDispatcher event_dispatcher_;
    mutable std::mutex mutex_;
    std::map<int64_t, ServiceRecord> services_;
    std::map<int64_t, SubscriptionRecord> subscriptions_;
    std::map<uint64_t, std::shared_ptr<AsyncCallRecord>> async_calls_;
    std::map<RuntimeAppId, std::set<int64_t>> app_services_;
    std::map<RuntimeAppId, std::set<int64_t>> app_subscriptions_;
    std::set<RuntimeAppId> finish_requests_;
    int64_t next_handle_id_ = 1;
    uint64_t next_async_call_id_ = 1;
};

std::shared_ptr<SystemHostBridge::Impl::AsyncCallRecord> SystemHostBridge::Impl::create_async_call(
    RuntimeAppId app_id, NativeAsyncCallback callback
)
{
    auto record = std::make_shared<AsyncCallRecord>();
    record->app_id = app_id;
    record->callback = std::move(callback);

    std::lock_guard lock(mutex_);
    async_calls_.emplace(next_async_call_id_++, record);
    return record;
}

void SystemHostBridge::Impl::complete_async_call(
    const std::shared_ptr<AsyncCallRecord> &record, NativeResult result, const char *source
)
{
    (void)source;
    NativeAsyncCallback callback;
    {
        std::lock_guard lock(mutex_);
        if (!record || record->completed || !record->callback) {
            BROOKESIA_LOGD("Ignore stale runtime async service completion from %1%", source);
            return;
        }
        record->completed = true;
        callback = std::move(record->callback);

        for (auto it = async_calls_.begin(); it != async_calls_.end(); ++it) {
            if (it->second == record) {
                async_calls_.erase(it);
                break;
            }
        }
    }

    if (callback) {
        callback(std::move(result));
    }
}

void SystemHostBridge::Impl::request_finish_app(RuntimeAppId id)
{
    std::lock_guard lock(mutex_);
    finish_requests_.insert(id);
    BROOKESIA_LOGD("App finish requested: app_id(%1%)", id);
}

bool SystemHostBridge::Impl::consume_app_finish_request(RuntimeAppId id)
{
    std::lock_guard lock(mutex_);
    return finish_requests_.erase(id) > 0;
}

void SystemHostBridge::Impl::release_app_resources(RuntimeAppId id)
{
    std::vector<service::EventRegistry::SignalConnection> subscriptions;
    std::vector<service::ServiceBinding> services;
    std::vector<std::shared_ptr<AsyncCallRecord>> async_records;
    {
        std::lock_guard lock(mutex_);
        auto subscription_it = app_subscriptions_.find(id);
        if (subscription_it != app_subscriptions_.end()) {
            for (auto handle : subscription_it->second) {
                auto it = subscriptions_.find(handle);
                if (it != subscriptions_.end()) {
                    subscriptions.push_back(std::move(it->second.connection));
                    subscriptions_.erase(it);
                }
            }
            app_subscriptions_.erase(subscription_it);
        }

        auto service_it = app_services_.find(id);
        if (service_it != app_services_.end()) {
            for (auto handle : service_it->second) {
                auto it = services_.find(handle);
                if (it != services_.end()) {
                    services.push_back(std::move(it->second.binding));
                    services_.erase(it);
                }
            }
            app_services_.erase(service_it);
        }

        for (auto it = async_calls_.begin(); it != async_calls_.end();) {
            if (it->second && (it->second->app_id == id)) {
                it->second->completed = true;
                it->second->callback = nullptr;
                async_records.push_back(it->second);
                it = async_calls_.erase(it);
            } else {
                ++it;
            }
        }

        finish_requests_.erase(id);
    }
    function_bridge_->release_app_context(id);
    if (!services.empty() || !subscriptions.empty() || !async_records.empty()) {
        BROOKESIA_LOGD(
            "Release runtime app service resources: app_id(%1%), services(%2%), subscriptions(%3%), async_calls(%4%)",
            id, services.size(), subscriptions.size(), async_records.size()
        );
    }
}

void SystemHostBridge::Impl::release_all_app_resources()
{
    std::vector<RuntimeAppId> app_ids;
    {
        std::lock_guard lock(mutex_);
        app_ids.reserve(app_services_.size() + app_subscriptions_.size());
        for (const auto &[id, services] : app_services_) {
            app_ids.push_back(id);
        }
        for (const auto &[id, subscriptions] : app_subscriptions_) {
            if (std::find(app_ids.begin(), app_ids.end(), id) == app_ids.end()) {
                app_ids.push_back(id);
            }
        }
        for (auto id : finish_requests_) {
            if (std::find(app_ids.begin(), app_ids.end(), id) == app_ids.end()) {
                app_ids.push_back(id);
            }
        }
        for (const auto &[_, record] : async_calls_) {
            if (record && (std::find(app_ids.begin(), app_ids.end(), record->app_id) == app_ids.end())) {
                app_ids.push_back(record->app_id);
            }
        }
    }
    for (auto id : app_ids) {
        release_app_resources(id);
    }
    function_bridge_->release_all_app_contexts();
}

std::expected<void, std::string> SystemHostBridge::Impl::resolve_storage_path_markers(
    RuntimeAppId app_id,
    boost::json::value &value
)
{
    if (value.is_object()) {
        auto &object = value.as_object();
        if (object.size() == 1) {
            auto marker = object.find(STORAGE_PATH_MARKER_KEY);
            const bool convert_to_url = marker == object.end();
            if (convert_to_url) {
                marker = object.find(STORAGE_URL_MARKER_KEY);
            }
            if (marker != object.end()) {
                if (!storage_path_resolver_) {
                    return std::unexpected("Runtime storage path resolver is not available");
                }
                if (!marker->value().is_object()) {
                    return std::unexpected("Runtime storage marker must be an object");
                }
                auto location = storage_location_from_marker(marker->value().as_object());
                if (!location) {
                    return std::unexpected(location.error());
                }
                auto path = storage_path_resolver_(app_id, *location);
                if (!path) {
                    return std::unexpected(path.error());
                }
                value = convert_to_url ? file_url_from_path(std::move(*path)) : std::move(*path);
                return {};
            }
        }

        for (auto &[_, child] : object) {
            auto result = resolve_storage_path_markers(app_id, child);
            if (!result) {
                return result;
            }
        }
        return {};
    }

    if (value.is_array()) {
        for (auto &child : value.as_array()) {
            auto result = resolve_storage_path_markers(app_id, child);
            if (!result) {
                return result;
            }
        }
    }
    return {};
}

std::expected<service::FunctionParameterMap, std::string> SystemHostBridge::Impl::parse_params_with_storage_paths(
    RuntimeAppId app_id,
    std::string_view params_json
)
{
    if (params_json.empty()) {
        return service::FunctionParameterMap{};
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(params_json, error_code);
    if (error_code || !parsed.is_object()) {
        return std::unexpected("Function parameters must be a JSON object");
    }

    auto resolve_result = resolve_storage_path_markers(app_id, parsed);
    if (!resolve_result) {
        return std::unexpected(resolve_result.error());
    }
    return parse_function_params_object(parsed.as_object());
}

std::expected<std::vector<service::FunctionCall>, std::string> SystemHostBridge::Impl::parse_calls_with_storage_paths(
    RuntimeAppId app_id,
    std::string_view calls_json
)
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(calls_json, error_code);
    if (error_code || !parsed.is_array()) {
        return std::unexpected("Function calls must be a JSON array");
    }

    auto resolve_result = resolve_storage_path_markers(app_id, parsed);
    if (!resolve_result) {
        return std::unexpected(resolve_result.error());
    }
    return parse_function_calls(boost::json::serialize(parsed));
}

NativeResult SystemHostBridge::Impl::service_available(const NativeArgs &args)
{
    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        return std::unexpected("brookesia.service_available(name) requires a service name");
    }
    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_initialized() && !manager.init()) {
        return std::unexpected("Failed to initialize service manager");
    }
    return NativeValue{manager.get_service(service_name) != nullptr};
}

NativeResult SystemHostBridge::Impl::start_service(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }

    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        return std::unexpected("brookesia.start_service(name) requires a service name");
    }

    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_running() && (!manager.init() || !manager.start())) {
        return std::unexpected("Failed to start service manager");
    }

    auto heap_before_start_service = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name;
        heap_trace::log_detail(
            "runtime.host",
            "start_service before bind",
            runtime_app_id,
            details,
            heap_before_start_service
        );
    }
    auto binding = manager.bind(service_name);
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name;
        heap_trace::log_detail(
            "runtime.host",
            "start_service after bind",
            runtime_app_id,
            details,
            heap_trace::capture(),
            &heap_before_start_service
        );
    }
    if (!binding) {
        return std::unexpected("Failed to start service: " + service_name);
    }

    auto service = binding.get_service();
    int64_t handle = 0;
    std::size_t service_record_count = 0;
    {
        std::lock_guard lock(mutex_);
        handle = next_handle_id_++;
        services_.emplace(handle, ServiceRecord{
            .app_id = app_id.value(),
            .binding = std::move(binding),
            .service = std::move(service),
        });
        app_services_[app_id.value()].insert(handle);
        service_record_count = services_.size();
    }
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name +
                             " handle=" + std::to_string(handle) +
                             " service_records=" + std::to_string(service_record_count);
        heap_trace::log_detail(
            "runtime.host",
            "start_service after host record insert",
            runtime_app_id,
            details,
            heap_trace::capture(),
            &heap_before_start_service
        );
    }
    return NativeValue{handle};
}

NativeResult SystemHostBridge::Impl::stop_service(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.empty()) {
        return std::unexpected("brookesia.stop_service(handle) requires a service handle");
    }

    const int64_t handle = native_value_to_handle(args[0]);
    std::optional<service::ServiceBinding> binding;
    {
        std::lock_guard lock(mutex_);
        auto it = services_.find(handle);
        if (it == services_.end()) {
            return std::unexpected("Service handle not found");
        }
        if (it->second.app_id != app_id.value()) {
            return std::unexpected("Service handle does not belong to current app");
        }
        app_services_[it->second.app_id].erase(handle);
        binding.emplace(std::move(it->second.binding));
        services_.erase(it);
    }
    return make_success();
}

NativeResult SystemHostBridge::Impl::call_function(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected(
                   "brookesia.call_function(handle, function_name, params_json) requires a handle and function name"
               );
    }

    const int64_t handle = native_value_to_handle(args[0]);
    std::string function_name;
    if (!get_string_arg(args, 1, function_name)) {
        return std::unexpected("brookesia.call_function() requires a function name");
    }

    std::string params_json = "{}";
    if ((args.size() > 2) && std::holds_alternative<std::string>(args[2])) {
        params_json = std::get<std::string>(args[2]);
    }

    uint32_t timeout_ms = BROOKESIA_SERVICE_MANAGER_DEFAULT_CALL_FUNCTION_TIMEOUT_MS;
    if (args.size() > 3) {
        if (!native_value_to_timeout_ms(args[3], timeout_ms)) {
            return std::unexpected("brookesia.call_function() timeout_ms must be a non-negative integer");
        }
    }

    std::shared_ptr<service::ServiceBase> service;
    {
        std::lock_guard lock(mutex_);
        auto it = services_.find(handle);
        if (it == services_.end()) {
            return std::unexpected("Service handle not found");
        }
        if (it->second.app_id != app_id.value()) {
            return std::unexpected("Service handle does not belong to current app");
        }
        service = it->second.service;
    }

    const auto service_name = service == nullptr ? std::string() : service->get_attributes().name;
    auto heap_before_call = heap_trace::capture();
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name +
                             " function=" + function_name +
                             " params_bytes=" + std::to_string(params_json.size());
        heap_trace::log_detail("runtime.host", "call_function before parse params", runtime_app_id, details, heap_before_call);
    }
    auto params = parse_params_with_storage_paths(app_id.value(), params_json);
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name +
                             " function=" + function_name +
                             " params_bytes=" + std::to_string(params_json.size());
        heap_trace::log_detail(
            "runtime.host",
            "call_function after parse params",
            runtime_app_id,
            details,
            heap_trace::capture(),
            &heap_before_call
        );
    }
    if (!params) {
        return std::unexpected(params.error());
    }
    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    auto result = service->call_function_sync(function_name, std::move(*params), timeout_ms);
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name +
                             " function=" + function_name +
                             " success=" + std::to_string(result.success);
        heap_trace::log_detail(
            "runtime.host",
            "call_function after service call",
            runtime_app_id,
            details,
            heap_trace::capture(),
            &heap_before_call
        );
    }
    auto result_json = function_result_to_json(std::move(result));
    if constexpr (heap_trace::enabled) {
        const auto runtime_app_id = std::to_string(app_id.value());
        const auto details = "runtime_app_id=" + runtime_app_id + " service=" + service_name +
                             " function=" + function_name +
                             " result_bytes=" + std::to_string(result_json.size());
        heap_trace::log_detail(
            "runtime.host",
            "call_function after result JSON serialization",
            runtime_app_id,
            details,
            heap_trace::capture(),
            &heap_before_call
        );
    }
    return NativeValue{std::move(result_json)};
}

void SystemHostBridge::Impl::call_function_async(const NativeArgs &args, NativeAsyncCallback callback)
{
    auto complete_error = [&callback](std::string error) mutable {
        if (callback)
        {
            callback(NativeResult(std::unexpected(std::move(error))));
        }
    };

    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        complete_error(app_id.error());
        return;
    }
    if (args.size() < 2) {
        complete_error(
            "brookesia.call_function_async(handle, function_name, params_json) requires a handle and function name"
        );
        return;
    }

    const int64_t handle = native_value_to_handle(args[0]);
    std::string function_name;
    if (!get_string_arg(args, 1, function_name)) {
        complete_error("brookesia.call_function_async() requires a function name");
        return;
    }

    std::string params_json = "{}";
    if ((args.size() > 2) && std::holds_alternative<std::string>(args[2])) {
        params_json = std::get<std::string>(args[2]);
    }

    uint32_t timeout_ms = 0;
    if (args.size() > 3) {
        if (!native_value_to_timeout_ms(args[3], timeout_ms)) {
            complete_error("brookesia.call_function_async() timeout_ms must be a non-negative integer");
            return;
        }
    }

    std::shared_ptr<service::ServiceBase> service;
    {
        std::lock_guard lock(mutex_);
        auto it = services_.find(handle);
        if (it == services_.end()) {
            complete_error("Service handle not found");
            return;
        }
        if (it->second.app_id != app_id.value()) {
            complete_error("Service handle does not belong to current app");
            return;
        }
        service = it->second.service;
    }

    auto params = parse_params_with_storage_paths(app_id.value(), params_json);
    if (!params) {
        complete_error(params.error());
        return;
    }

    if ((timeout_ms > 0) && (!task_scheduler_ || !task_scheduler_->is_running())) {
        complete_error("Async timeout scheduler unavailable");
        return;
    }

    auto async_record = create_async_call(app_id.value(), std::move(callback));
    if (timeout_ms > 0) {
        auto timeout_task = [this, async_record, timeout_ms]() {
            complete_async_call(
                async_record,
                NativeResult(
                    std::unexpected("Async service function timeout after " + std::to_string(timeout_ms) + " ms")
                ),
                "timeout"
            );
        };
        if (!task_scheduler_->post_delayed(std::move(timeout_task), static_cast<int>(timeout_ms))) {
            complete_async_call(
                async_record,
                NativeResult(std::unexpected("Failed to schedule async service function timeout")),
                "timeout-schedule"
            );
            return;
        }
    }

    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    const bool accepted = service->call_function_async(
                              function_name,
                              std::move(*params),
    [this, async_record](service::FunctionResult && result) mutable {
        complete_async_call(
            async_record,
        NativeValue{function_result_to_json(std::move(result))},
        "service"
        );
    }
                          );
    if (!accepted) {
        complete_async_call(
            async_record,
            NativeResult(std::unexpected("Failed to submit async service function call: " + function_name)),
            "submit"
        );
    }
}

#if BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE
void SystemHostBridge::Impl::debug_delay_async(const NativeArgs &args, NativeAsyncCallback callback)
{
    auto complete_error = [&callback](std::string error) mutable {
        if (callback)
        {
            callback(NativeResult(std::unexpected(std::move(error))));
        }
    };

    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        complete_error(app_id.error());
        return;
    }
    if (args.empty()) {
        complete_error("brookesia.debug_delay_async(delay_ms, timeout_ms) requires delay_ms");
        return;
    }

    uint32_t delay_ms = 0;
    if (!native_value_to_timeout_ms(args[0], delay_ms)) {
        complete_error("brookesia.debug_delay_async() delay_ms must be a non-negative integer");
        return;
    }

    uint32_t timeout_ms = 0;
    if (args.size() > 1) {
        if (!native_value_to_timeout_ms(args[1], timeout_ms)) {
            complete_error("brookesia.debug_delay_async() timeout_ms must be a non-negative integer");
            return;
        }
    }

    if (!task_scheduler_ || !task_scheduler_->is_running()) {
        complete_error("Async debug delay scheduler unavailable");
        return;
    }

    auto async_record = create_async_call(app_id.value(), std::move(callback));
    if ((timeout_ms > 0) && (delay_ms > timeout_ms)) {
        complete_async_call(
            async_record,
            NativeResult(std::unexpected("Async debug delay timeout after " + std::to_string(timeout_ms) + " ms")),
            "debug-delay-timeout"
        );
        return;
    }

    if (timeout_ms > 0) {
        auto timeout_task = [this, async_record, timeout_ms]() {
            complete_async_call(
                async_record,
                NativeResult(std::unexpected("Async debug delay timeout after " + std::to_string(timeout_ms) + " ms")),
                "debug-delay-timeout"
            );
        };
        if (!task_scheduler_->post_delayed(std::move(timeout_task), static_cast<int>(timeout_ms))) {
            complete_async_call(
                async_record,
                NativeResult(std::unexpected("Failed to schedule async debug delay timeout")),
                "debug-delay-timeout-schedule"
            );
            return;
        }
    }

    auto delay_task = [this, async_record, delay_ms]() {
        complete_async_call(
            async_record,
            NativeValue{"Async debug delay completed after " + std::to_string(delay_ms) + " ms"},
            "debug-delay"
        );
    };
    if (!task_scheduler_->post_delayed(std::move(delay_task), static_cast<int>(delay_ms))) {
        complete_async_call(
            async_record,
            NativeResult(std::unexpected("Failed to schedule async debug delay")),
            "debug-delay-schedule"
        );
    }
}
#endif

NativeResult SystemHostBridge::Impl::call_functions(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected("brookesia.call_functions(handle, calls_json) requires a handle and calls JSON array");
    }

    const int64_t handle = native_value_to_handle(args[0]);
    std::string calls_json;
    if (!get_string_arg(args, 1, calls_json)) {
        return std::unexpected("brookesia.call_functions() requires calls_json string");
    }

    std::shared_ptr<service::ServiceBase> service;
    {
        std::lock_guard lock(mutex_);
        auto it = services_.find(handle);
        if (it == services_.end()) {
            return std::unexpected("Service handle not found");
        }
        if (it->second.app_id != app_id.value()) {
            return std::unexpected("Service handle does not belong to current app");
        }
        service = it->second.service;
    }

    auto calls = parse_calls_with_storage_paths(app_id.value(), calls_json);
    if (!calls) {
        return std::unexpected(calls.error());
    }
    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    return NativeValue{function_batch_result_to_json(service->call_functions_sync(std::move(*calls)))};
}

NativeResult SystemHostBridge::Impl::call_service_function(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected(
                   "brookesia.call_service_function(service_name, function_name, params_json) "
                   "requires a service name and function name"
               );
    }

    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        return std::unexpected("brookesia.call_service_function() requires a service name");
    }
    std::string function_name;
    if (!get_string_arg(args, 1, function_name)) {
        return std::unexpected("brookesia.call_service_function() requires a function name");
    }

    std::string params_json = "{}";
    if ((args.size() > 2) && std::holds_alternative<std::string>(args[2])) {
        params_json = std::get<std::string>(args[2]);
    }

    uint32_t timeout_ms = BROOKESIA_SERVICE_MANAGER_DEFAULT_CALL_FUNCTION_TIMEOUT_MS;
    if (args.size() > 3) {
        if (!native_value_to_timeout_ms(args[3], timeout_ms)) {
            return std::unexpected("brookesia.call_service_function() timeout_ms must be a non-negative integer");
        }
    }

    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_initialized() && !manager.init()) {
        return std::unexpected("Failed to initialize service manager");
    }
    auto service = manager.get_service(service_name);
    if (!service) {
        return std::unexpected("Service not found: " + service_name);
    }

    auto params = parse_params_with_storage_paths(app_id.value(), params_json);
    if (!params) {
        return std::unexpected(params.error());
    }
    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    auto result = service->call_function_sync(function_name, std::move(*params), timeout_ms);
    return NativeValue{function_result_to_json(std::move(result))};
}

void SystemHostBridge::Impl::call_service_function_async(const NativeArgs &args, NativeAsyncCallback callback)
{
    auto complete_error = [&callback](std::string error) mutable {
        if (callback)
        {
            callback(NativeResult(std::unexpected(std::move(error))));
        }
    };

    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        complete_error(app_id.error());
        return;
    }
    if (args.size() < 2) {
        complete_error(
            "brookesia.call_service_function_async(service_name, function_name, params_json) "
            "requires a service name and function name"
        );
        return;
    }

    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        complete_error("brookesia.call_service_function_async() requires a service name");
        return;
    }
    std::string function_name;
    if (!get_string_arg(args, 1, function_name)) {
        complete_error("brookesia.call_service_function_async() requires a function name");
        return;
    }

    std::string params_json = "{}";
    if ((args.size() > 2) && std::holds_alternative<std::string>(args[2])) {
        params_json = std::get<std::string>(args[2]);
    }

    uint32_t timeout_ms = 0;
    if (args.size() > 3) {
        if (!native_value_to_timeout_ms(args[3], timeout_ms)) {
            complete_error("brookesia.call_service_function_async() timeout_ms must be a non-negative integer");
            return;
        }
    }

    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_initialized() && !manager.init()) {
        complete_error("Failed to initialize service manager");
        return;
    }
    auto service = manager.get_service(service_name);
    if (!service) {
        complete_error("Service not found: " + service_name);
        return;
    }

    auto params = parse_params_with_storage_paths(app_id.value(), params_json);
    if (!params) {
        complete_error(params.error());
        return;
    }

    if ((timeout_ms > 0) && (!task_scheduler_ || !task_scheduler_->is_running())) {
        complete_error("Async timeout scheduler unavailable");
        return;
    }

    auto async_record = create_async_call(app_id.value(), std::move(callback));
    if (timeout_ms > 0) {
        auto timeout_task = [this, async_record, timeout_ms]() {
            complete_async_call(
                async_record,
                NativeResult(
                    std::unexpected("Async service function timeout after " + std::to_string(timeout_ms) + " ms")
                ),
                "timeout"
            );
        };
        if (!task_scheduler_->post_delayed(std::move(timeout_task), static_cast<int>(timeout_ms))) {
            complete_async_call(
                async_record,
                NativeResult(std::unexpected("Failed to schedule async service function timeout")),
                "timeout-schedule"
            );
            return;
        }
    }

    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    const bool accepted = service->call_function_async(
                              function_name,
                              std::move(*params),
    [this, async_record](service::FunctionResult && result) mutable {
        complete_async_call(
            async_record,
        NativeValue{function_result_to_json(std::move(result))},
        "service"
        );
    }
                          );
    if (!accepted) {
        complete_async_call(
            async_record,
            NativeResult(std::unexpected("Failed to submit async service function call: " + function_name)),
            "submit"
        );
    }
}

NativeResult SystemHostBridge::Impl::call_service_functions(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected(
                   "brookesia.call_service_functions(service_name, calls_json) "
                   "requires a service name and calls JSON array"
               );
    }

    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        return std::unexpected("brookesia.call_service_functions() requires a service name");
    }
    std::string calls_json;
    if (!get_string_arg(args, 1, calls_json)) {
        return std::unexpected("brookesia.call_service_functions() requires calls_json string");
    }

    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_initialized() && !manager.init()) {
        return std::unexpected("Failed to initialize service manager");
    }
    auto service = manager.get_service(service_name);
    if (!service) {
        return std::unexpected("Service not found: " + service_name);
    }

    auto calls = parse_calls_with_storage_paths(app_id.value(), calls_json);
    if (!calls) {
        return std::unexpected(calls.error());
    }
    service::ScopedCallContext call_context(make_runtime_call_context(app_id.value()));
    return NativeValue{function_batch_result_to_json(service->call_functions_sync(std::move(*calls)))};
}

NativeResult SystemHostBridge::Impl::subscribe_event(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected("brookesia.subscribe_event(handle, event_name) requires a handle and event name");
    }

    const int64_t service_handle = native_value_to_handle(args[0]);
    std::string event_name;
    if (!get_string_arg(args, 1, event_name)) {
        return std::unexpected("brookesia.subscribe_event() requires an event name");
    }

    std::shared_ptr<service::ServiceBase> service;
    {
        std::lock_guard lock(mutex_);
        auto it = services_.find(service_handle);
        if (it == services_.end()) {
            return std::unexpected("Service handle not found");
        }
        if (it->second.app_id != app_id.value()) {
            return std::unexpected("Service handle does not belong to current app");
        }
        service = it->second.service;
    }

    auto connection = service->subscribe_event(
                          event_name,
                          [this, app_id = app_id.value(), service_name = service->get_attributes().name](
                              const std::string & event_name, const service::EventItemMap & items
    ) {
        const auto items_json = event_items_to_json(items);
        BROOKESIA_LOGD("Service event [%1%.%2%]: %3%", service_name, event_name, items_json);
        if (!event_dispatcher_) {
            return;
        }
        auto dispatch_result = event_dispatcher_(app_id, service_name, event_name, items_json);
        if (!dispatch_result) {
            BROOKESIA_LOGW(
                "Failed to dispatch runtime service event: service(%1%), event(%2%), error(%3%)",
                service_name, event_name, dispatch_result.error()
            );
        }
    }
                      );
    if (!connection.connected()) {
        return std::unexpected("Failed to subscribe service event: " + event_name);
    }

    int64_t handle = 0;
    std::lock_guard lock(mutex_);
    handle = next_handle_id_++;
    subscriptions_.emplace(handle, SubscriptionRecord{
        .app_id = app_id.value(),
        .connection = std::move(connection),
    });
    app_subscriptions_[app_id.value()].insert(handle);
    return NativeValue{handle};
}

NativeResult SystemHostBridge::Impl::subscribe_service_event(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.size() < 2) {
        return std::unexpected(
                   "brookesia.subscribe_service_event(service_name, event_name) requires a service name and event name"
               );
    }

    std::string service_name;
    if (!get_string_arg(args, 0, service_name)) {
        return std::unexpected("brookesia.subscribe_service_event() requires a service name");
    }
    std::string event_name;
    if (!get_string_arg(args, 1, event_name)) {
        return std::unexpected("brookesia.subscribe_service_event() requires an event name");
    }

    auto &manager = service::ServiceManager::get_instance();
    if (!manager.is_initialized() && !manager.init()) {
        return std::unexpected("Failed to initialize service manager");
    }
    auto service = manager.get_service(service_name);
    if (!service) {
        return std::unexpected("Service not found: " + service_name);
    }

    auto connection = service->subscribe_event(
                          event_name,
                          [this, app_id = app_id.value(), service_name = service->get_attributes().name](
                              const std::string & event_name, const service::EventItemMap & items
    ) {
        const auto items_json = event_items_to_json(items);
        BROOKESIA_LOGD("Service event [%1%.%2%]: %3%", service_name, event_name, items_json);
        if (!event_dispatcher_) {
            return;
        }
        auto dispatch_result = event_dispatcher_(app_id, service_name, event_name, items_json);
        if (!dispatch_result) {
            BROOKESIA_LOGW(
                "Failed to dispatch runtime service event: service(%1%), event(%2%), error(%3%)",
                service_name, event_name, dispatch_result.error()
            );
        }
    }
                      );
    if (!connection.connected()) {
        return std::unexpected("Failed to subscribe service event: " + service_name + "." + event_name);
    }

    int64_t handle = 0;
    std::lock_guard lock(mutex_);
    handle = next_handle_id_++;
    subscriptions_.emplace(handle, SubscriptionRecord{
        .app_id = app_id.value(),
        .connection = std::move(connection),
    });
    app_subscriptions_[app_id.value()].insert(handle);
    return NativeValue{handle};
}

NativeResult SystemHostBridge::Impl::unsubscribe_event(const NativeArgs &args)
{
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    if (args.empty()) {
        return std::unexpected("brookesia.unsubscribe_event(handle) requires a subscription handle");
    }

    const int64_t handle = native_value_to_handle(args[0]);
    service::EventRegistry::SignalConnection connection;
    {
        std::lock_guard lock(mutex_);
        auto it = subscriptions_.find(handle);
        if (it == subscriptions_.end()) {
            return std::unexpected("Subscription handle not found");
        }
        if (it->second.app_id != app_id.value()) {
            return std::unexpected("Subscription handle does not belong to current app");
        }
        app_subscriptions_[it->second.app_id].erase(handle);
        connection = std::move(it->second.connection);
        subscriptions_.erase(it);
    }
    return make_success();
}

NativeResult SystemHostBridge::Impl::current_app_context(const NativeArgs &args)
{
    (void)args;
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    return NativeValue{function_bridge_->get_or_create_context_token(app_id.value())};
}

NativeResult SystemHostBridge::Impl::finish_app(const NativeArgs &args)
{
    (void)args;
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    request_finish_app(app_id.value());
    return make_success();
}

NativeResult SystemHostBridge::Impl::attach_app_context(const NativeArgs &args)
{
    if (args.empty()) {
        return std::unexpected("brookesia.attach_app_context(token) requires a context token");
    }
    const int64_t token = native_value_to_handle(args[0]);
    auto result = function_bridge_->attach_context(token);
    if (!result) {
        return std::unexpected(result.error());
    }
    return make_success();
}

NativeResult SystemHostBridge::Impl::detach_app_context(const NativeArgs &args)
{
    (void)args;
    function_bridge_->detach_context();
    return make_success();
}

NativeResult SystemHostBridge::Impl::print(const NativeArgs &args)
{
    std::string message;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            message += ' ';
        }
        message += native_value_to_print_string(args[i]);
    }
    BROOKESIA_LOGI("%1%", message);
    return NativeValue{};
}

NativeModule SystemHostBridge::Impl::make_brookesia_module()
{
    NativeModule module;
    module.name = "brookesia";
    module.functions = {
        {
            .name = "service_available",
            .function = [this](const NativeArgs & args)
            {
                return service_available(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "start_service",
            .function = [this](const NativeArgs & args)
            {
                return start_service(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "stop_service",
            .function = [this](const NativeArgs & args)
            {
                return stop_service(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "call_function",
            .function = [this](const NativeArgs & args)
            {
                return call_function(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "call_function_async",
            .function = nullptr,
            .async_function = [this](const NativeArgs & args, NativeAsyncCallback callback)
            {
                call_function_async(args, std::move(callback));
            },
        },
#if BROOKESIA_SYSTEM_CORE_RUNTIME_ASYNC_DEBUG_PROBE
        {
            .name = "debug_delay_async",
            .function = nullptr,
            .async_function = [this](const NativeArgs & args, NativeAsyncCallback callback)
            {
                debug_delay_async(args, std::move(callback));
            },
        },
#endif
        {
            .name = "call_functions",
            .function = [this](const NativeArgs & args)
            {
                return call_functions(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "call_service_function",
            .function = [this](const NativeArgs & args)
            {
                return call_service_function(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "call_service_function_async",
            .function = nullptr,
            .async_function = [this](const NativeArgs & args, NativeAsyncCallback callback)
            {
                call_service_function_async(args, std::move(callback));
            },
        },
        {
            .name = "call_service_functions",
            .function = [this](const NativeArgs & args)
            {
                return call_service_functions(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "subscribe_event",
            .function = [this](const NativeArgs & args)
            {
                return subscribe_event(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "subscribe_service_event",
            .function = [this](const NativeArgs & args)
            {
                return subscribe_service_event(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "unsubscribe_event",
            .function = [this](const NativeArgs & args)
            {
                return unsubscribe_event(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "current_app_context",
            .function = [this](const NativeArgs & args)
            {
                return current_app_context(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "finish_app",
            .function = [this](const NativeArgs & args)
            {
                return finish_app(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "attach_app_context",
            .function = [this](const NativeArgs & args)
            {
                return attach_app_context(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "detach_app_context",
            .function = [this](const NativeArgs & args)
            {
                return detach_app_context(args);
            },
            .async_function = nullptr,
        },
        {
            .name = "print",
            .function = [this](const NativeArgs & args)
            {
                return print(args);
            },
            .async_function = nullptr,
        },
    };
    return module;
}

SystemHostBridge::SystemHostBridge(
    std::shared_ptr<runtime::RuntimeFunctionBridge> function_bridge,
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler,
    StoragePathResolver storage_path_resolver,
    EventDispatcher event_dispatcher
)
    : impl_(std::make_unique<Impl>(
                function_bridge ? std::move(function_bridge) : std::make_shared<runtime::RuntimeFunctionBridge>(),
                std::move(task_scheduler),
                std::move(storage_path_resolver),
                std::move(event_dispatcher)
            ))
{}

SystemHostBridge::~SystemHostBridge() = default;

void SystemHostBridge::add_module(runtime::NativeModule module)
{
    auto result = impl_->function_bridge_->add_module(std::move(module));
    if (!result) {
        BROOKESIA_LOGW("Add runtime native module failed: %1%", result.error());
    }
}

std::vector<runtime::NativeModule> SystemHostBridge::get_modules() const
{
    return impl_->function_bridge_->get_modules();
}

std::vector<runtime::NativeModule> SystemHostBridge::get_modules_for_app(runtime::AppId id) const
{
    return impl_->function_bridge_->get_modules_for_app(id);
}

SystemHostBridge::AppContextGuard SystemHostBridge::make_app_context_guard(
    runtime::AppId id
) const
{
    return impl_->function_bridge_->make_app_context_guard(id);
}

std::shared_ptr<runtime::RuntimeFunctionBridge> SystemHostBridge::get_function_bridge() const
{
    return impl_->function_bridge_;
}

bool SystemHostBridge::consume_app_finish_request(runtime::AppId id)
{
    return impl_->consume_app_finish_request(id);
}

void SystemHostBridge::release_app_resources(runtime::AppId id)
{
    impl_->release_app_resources(id);
}

void SystemHostBridge::release_all_app_resources()
{
    impl_->release_all_app_resources();
}

} // namespace esp_brookesia::system::core
