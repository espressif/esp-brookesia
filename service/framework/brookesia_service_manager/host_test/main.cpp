/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include "boost/chrono.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/future.hpp"
#include "boost/thread/thread.hpp"
#include "brookesia/service_manager.hpp"

using namespace esp_brookesia::service;

namespace {

class EchoService : public ServiceBase {
public:
    EchoService(std::string name, SchedulerType scheduler_type = SchedulerType::Main)
        : ServiceBase(Attributes{
        .name = std::move(name),
        .description = "Test service.",
        .version = "0.0.0",
        .scheduler_type = scheduler_type,
    })
    {
    }

    void set_peer(const std::shared_ptr<ServiceBase> &peer)
    {
        peer_ = peer;
    }

    int get_active_calls() const
    {
        return active_calls_.load();
    }

    int get_max_active_calls() const
    {
        return max_active_calls_.load();
    }

    bool has_registered_function(std::string_view name) const
    {
        auto registry = get_function_registry();
        if (!registry) {
            return false;
        }
        for (const auto &schema : registry->get_schemas()) {
            if (schema.name == name) {
                return true;
            }
        }
        return false;
    }

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {
            {
                .name = "echo",
                .description = "Return the input string",
                .parameters = {{
                        .name = "message",
                        .description = "Message to echo",
                        .type = FunctionValueType::String,
                    }
                },
                .require_scheduler = true,
                .return_value = FunctionReturnSchema{
                    .type = FunctionValueType::String,
                    .description = "Echoed message.",
                },
            },
            {
                .name = "nested",
                .description = "Synchronously call echo from the same call group",
                .require_scheduler = true,
                .return_value = FunctionReturnSchema{
                    .type = FunctionValueType::String,
                    .description = "Nested echo result.",
                },
            },
            {
                .name = "tracked",
                .description = "Track concurrent execution in this service call group",
                .require_scheduler = true,
            },
            {
                .name = "call_peer",
                .description = "Synchronously call the tracked function on a peer service",
                .require_scheduler = true,
            },
            {
                .name = "call_peer_chain",
                .description = "Synchronously call the next service in a peer chain",
                .require_scheduler = true,
            },
            {
                .name = "call_peer_short",
                .description = "Synchronously call a peer with a short timeout",
                .require_scheduler = true,
            },
        };
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        return {{
                .name = "changed",
                .description = "Test event.",
                .items = {{
                        .name = "Value",
                        .description = "Test value.",
                        .type = EventItemType::String,
                    }
                },
            }
        };
    }

protected:
    FunctionHandlerMap get_function_handlers() override
    {
        return {
            {
                "echo",
                [](FunctionParameterMap &&args) -> FunctionResult {
                    auto it = args.find("message");
                    if (it == args.end())
                    {
                        return FunctionResult{.success = false, .error_message = "missing message"};
                    }
                    return FunctionResult{.success = true, .data = it->second};
                },
            },
            {
                "nested",
                [this](FunctionParameterMap &&) -> FunctionResult {
                    return call_function_sync(
                        "echo",
                    FunctionParameterMap{{"message", FunctionValue(std::string("nested-ok"))}},
                    500
                    );
                },
            },
            {
                "tracked",
                [this](FunctionParameterMap &&) -> FunctionResult {
                    const int active_calls = active_calls_.fetch_add(1) + 1;
                    int max_active_calls = max_active_calls_.load();
                    while (active_calls > max_active_calls &&
                            !max_active_calls_.compare_exchange_weak(max_active_calls, active_calls))
                    {
                    }
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(150));
                    active_calls_.fetch_sub(1);
                    return FunctionResult{.success = true};
                },
            },
            {
                "call_peer",
                [this](FunctionParameterMap &&) -> FunctionResult {
                    auto peer = peer_.lock();
                    if (!peer)
                    {
                        return FunctionResult{.success = false, .error_message = "peer is not available"};
                    }
                    return peer->call_function_sync("tracked", FunctionParameterMap{}, 1000);
                },
            },
            {
                "call_peer_chain",
                [this](FunctionParameterMap &&) -> FunctionResult {
                    auto peer = peer_.lock();
                    if (!peer)
                    {
                        return FunctionResult{.success = true};
                    }
                    return peer->call_function_sync("call_peer_chain", FunctionParameterMap{}, 1000);
                },
            },
            {
                "call_peer_short",
                [this](FunctionParameterMap &&) -> FunctionResult {
                    auto peer = peer_.lock();
                    if (!peer)
                    {
                        return FunctionResult{.success = false, .error_message = "peer is not available"};
                    }
                    return peer->call_function_sync("tracked", FunctionParameterMap{}, 20);
                },
            },
        };
    }

private:
    std::weak_ptr<ServiceBase> peer_;
    std::atomic<int> active_calls_{0};
    std::atomic<int> max_active_calls_{0};
};

class InvalidVersionService : public ServiceBase {
public:
    InvalidVersionService()
        : ServiceBase(Attributes{
        .name = "invalid_version",
        .description = "Test service.",
        .version = "",
    })
    {
    }
};

class InvalidDescriptionService : public ServiceBase {
public:
    InvalidDescriptionService()
        : ServiceBase(Attributes{
        .name = "invalid_description",
        .description = "",
        .version = "0.0.0",
    })
    {
    }
};

class TransitionService final: public ServiceBase {
public:
    TransitionService()
        : ServiceBase({
        .name = "transition",
        .description = "Test service.",
        .version = "1.2.3",
    })
    {
    }

    bool wait_for_start_entry()
    {
        boost::unique_lock lock(mutex_);
        return condition_.wait_for(
        lock, boost::chrono::seconds(1), [this]() {
            return start_entered_;
        }
               );
    }

    bool wait_for_stop_entry()
    {
        boost::unique_lock lock(mutex_);
        return condition_.wait_for(
        lock, boost::chrono::seconds(1), [this]() {
            return stop_entered_;
        }
               );
    }

    void allow_start()
    {
        boost::lock_guard lock(mutex_);
        allow_start_ = true;
        condition_.notify_all();
    }

    void allow_stop()
    {
        boost::lock_guard lock(mutex_);
        allow_stop_ = true;
        condition_.notify_all();
    }

protected:
    bool on_start() override
    {
        boost::unique_lock lock(mutex_);
        start_entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this]() {
            return allow_start_;
        });
        return true;
    }

    void on_stop() override
    {
        boost::unique_lock lock(mutex_);
        stop_entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this]() {
            return allow_stop_;
        });
    }

private:
    boost::mutex mutex_;
    boost::condition_variable condition_;
    bool start_entered_ = false;
    bool stop_entered_ = false;
    bool allow_start_ = false;
    bool allow_stop_ = false;
};

bool verify_version_metadata(ServiceManager &manager, EchoService &service)
{
    if (service.get_attributes().version != "0.0.0") {
        std::cerr << "Service attribute version mismatch" << '\n';
        return false;
    }

    const auto attributes_description = BROOKESIA_DESCRIBE_TO_STR(service.get_attributes());
    if (attributes_description.find("0.0.0") == std::string::npos) {
        std::cerr << "Service attribute description omits version" << '\n';
        return false;
    }

    if (service.has_registered_function("GetVersion") || !service.has_registered_function("echo")) {
        std::cerr << "Service function registry contains an unexpected version function" << '\n';
        return false;
    }

    auto manager_binding = manager.bind(ManagerService::get_name().data());
    if (!manager_binding.is_valid()) {
        std::cerr << "Failed to bind built-in Manager service" << '\n';
        return false;
    }
    auto manager_service = manager_binding.get_service();
    auto result = manager_service->call_function_sync(
                      BROOKESIA_DESCRIBE_TO_STR(ManagerService::FunctionId::GetServiceInfo),
    FunctionParameterMap{{
            BROOKESIA_DESCRIBE_TO_STR(ManagerService::FunctionGetServiceInfoParam::Name),
            std::string("echo"),
        }}
                  );
    if (!result.success || !result.has_data() || !std::holds_alternative<boost::json::object>(*result.data)) {
        std::cerr << "Manager service information query failed" << '\n';
        return false;
    }
    ManagerService::ServiceInfo info;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(std::get<boost::json::object>(*result.data), info) ||
            (info.name != "echo") || (info.version != "0.0.0") ||
            (info.state != ManagerService::ServiceState::Stopped)) {
        std::cerr << "Manager returned invalid service information" << '\n';
        return false;
    }

    return true;
}

bool verify_echo_call(ServiceBase &service)
{
    auto result = service.call_function_sync(
    "echo", FunctionParameterMap{{"message", FunctionValue(std::string("host-ok"))}}, 500
                  );

    if (!result.success || !result.has_data()) {
        std::cerr << "Echo function failed" << std::endl;
        return false;
    }

    if (!std::holds_alternative<std::string>(*result.data)) {
        std::cerr << "Echo result type mismatch" << std::endl;
        return false;
    }

    if (std::get<std::string>(*result.data) != "host-ok") {
        std::cerr << "Echo result content mismatch" << std::endl;
        return false;
    }

    return true;
}

bool verify_nested_sync_call(ServiceBase &service)
{
    auto result = service.call_function_sync("nested", FunctionParameterMap{}, 500);
    if (!result.success || !result.has_data() || !std::holds_alternative<std::string>(*result.data)) {
        std::cerr << "Nested synchronous call failed" << '\n';
        return false;
    }
    if (std::get<std::string>(*result.data) != "nested-ok") {
        std::cerr << "Nested synchronous call result mismatch" << '\n';
        return false;
    }
    return true;
}

bool verify_cross_service_serialization(EchoService &target, EchoService &caller)
{
    boost::promise<bool> first_call_promise;
    auto first_call_future = first_call_promise.get_future();
    auto first_call_handler = [&first_call_promise](FunctionResult && result) {
        first_call_promise.set_value(result.success);
    };
    if (!target.call_function_async("tracked", FunctionParameterMap{}, first_call_handler)) {
        std::cerr << "Failed to start tracked call" << '\n';
        return false;
    }

    for (int retry = 0; retry < 100 && target.get_active_calls() == 0; ++retry) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(2));
    }
    if (target.get_active_calls() == 0) {
        std::cerr << "Tracked call did not start" << '\n';
        return false;
    }

    auto peer_result = caller.call_function_sync("call_peer", FunctionParameterMap{}, 1000);
    if (!peer_result.success) {
        std::cerr << "Cross-service synchronous call failed: " << peer_result.error_message << '\n';
        return false;
    }
    if (first_call_future.wait_for(boost::chrono::milliseconds(1000)) != boost::future_status::ready ||
            !first_call_future.get()) {
        std::cerr << "Initial tracked call failed" << '\n';
        return false;
    }
    if (target.get_max_active_calls() != 1) {
        std::cerr << "Target service call group was executed concurrently" << '\n';
        return false;
    }
    return true;
}

bool verify_wait_slot_release_after_timeout(EchoService &target, EchoService &caller)
{
    auto timeout_result = caller.call_function_sync("call_peer_short", FunctionParameterMap{}, 500);
    if (timeout_result.success || (timeout_result.error_message.find("Wait timeout") == std::string::npos)) {
        std::cerr << "Cross-service short timeout did not fail as expected" << '\n';
        return false;
    }

    boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
    auto retry_result = caller.call_function_sync("call_peer", FunctionParameterMap{}, 1000);
    if (!retry_result.success || (target.get_active_calls() != 0)) {
        std::cerr << "Worker wait slot was not released after a timeout" << '\n';
        return false;
    }
    return true;
}

bool verify_cross_service_wait_capacity(ServiceManager &manager)
{
    auto service_a = std::make_shared<EchoService>("chain_a");
    auto service_b = std::make_shared<EchoService>("chain_b");
    auto service_c = std::make_shared<EchoService>("chain_c");
    auto service_d = std::make_shared<EchoService>("chain_d");
    service_a->set_peer(service_b);
    service_b->set_peer(service_c);

    for (const auto &service : {
                service_a, service_b, service_c, service_d
            }) {
        if (!manager.add_service(service)) {
            std::cerr << "Failed to add cross-service chain service" << '\n';
            return false;
        }
    }

    std::vector<ServiceBinding> bindings;
    for (const auto *name : {
                "chain_a", "chain_b", "chain_c", "chain_d"
            }) {
        auto binding = manager.bind(name);
        if (!binding.is_valid()) {
            std::cerr << "Failed to bind cross-service chain service: " << name << '\n';
            return false;
        }
        bindings.push_back(std::move(binding));
    }

    auto three_service_result = service_a->call_function_sync("call_peer_chain", FunctionParameterMap{}, 1000);
    if (!three_service_result.success) {
        std::cerr << "Three-service synchronous chain failed: " << three_service_result.error_message << '\n';
        return false;
    }

    service_c->set_peer(service_d);
    const auto started_at = boost::chrono::steady_clock::now();
    auto exhausted_result = service_a->call_function_sync("call_peer_chain", FunctionParameterMap{}, 1000);
    const auto elapsed_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(
                                boost::chrono::steady_clock::now() - started_at
                            ).count();
    if (exhausted_result.success ||
            (exhausted_result.error_message.find("no available scheduler worker") == std::string::npos) ||
            (elapsed_ms >= 250)) {
        std::cerr << "Exhausted worker wait capacity did not fail quickly" << '\n';
        return false;
    }
    return true;
}

bool verify_single_worker_sync_failure(ServiceManager &manager)
{
    if (!manager.init()) {
        return false;
    }
    auto target = std::make_shared<EchoService>("single_target");
    auto caller = std::make_shared<EchoService>("single_caller");
    caller->set_peer(target);
    if (!manager.add_service(target) || !manager.add_service(caller)) {
        return false;
    }

    esp_brookesia::lib_utils::TaskScheduler::StartConfig config{
        .worker_configs = {{
                .name = "SvcMgrSingle",
                .stack_size = 8 * 1024,
            }
        },
    };
    if (!manager.start(config)) {
        return false;
    }
    auto target_binding = manager.bind("single_target");
    auto caller_binding = manager.bind("single_caller");
    if (!target_binding.is_valid() || !caller_binding.is_valid()) {
        return false;
    }

    const auto started_at = boost::chrono::steady_clock::now();
    auto result = caller->call_function_sync("call_peer", FunctionParameterMap{}, 1000);
    const auto elapsed_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(
                                boost::chrono::steady_clock::now() - started_at
                            ).count();
    const bool success = !result.success &&
                         (result.error_message.find("no available scheduler worker") != std::string::npos) &&
                         (elapsed_ms < 250);
    if (!success) {
        std::cerr << "Single-worker cross-service call did not fail quickly" << '\n';
    }

    caller_binding.release();
    target_binding.release();
    manager.stop();
    manager.deinit();
    return success;
}

bool verify_service_states(ServiceManager &manager)
{
    auto service = std::make_shared<TransitionService>();
    if (!manager.add_service(service)) {
        std::cerr << "Failed to add transition service" << '\n';
        return false;
    }

    auto initial_info = manager.get_service_info("transition");
    if (!initial_info || (initial_info->state != ManagerService::ServiceState::Stopped)) {
        std::cerr << "Transition service did not start in Stopped state" << '\n';
        return false;
    }

    std::unique_ptr<ServiceBinding> binding;
    boost::thread bind_thread([&]() {
        binding = std::make_unique<ServiceBinding>(manager.bind("transition"));
    });
    bool success = service->wait_for_start_entry();
    auto starting_info = manager.get_service_info("transition");
    success = success && starting_info &&
              (starting_info->state == ManagerService::ServiceState::Starting) &&
              (starting_info->reference_count == 1);
    service->allow_start();
    bind_thread.join();

    auto running_info = manager.get_service_info("transition");
    success = success && binding && binding->is_valid() && running_info &&
              (running_info->state == ManagerService::ServiceState::Running) &&
              (running_info->reference_count == 1);

    auto second_binding = manager.bind("transition");
    auto twice_bound_info = manager.get_service_info("transition");
    success = success && second_binding.is_valid() && twice_bound_info &&
              (twice_bound_info->reference_count == 2);
    second_binding.release();

    boost::thread stop_thread([&]() {
        binding->release();
    });
    success = service->wait_for_stop_entry() && success;
    auto stopping_info = manager.get_service_info("transition");
    success = success && stopping_info &&
              (stopping_info->state == ManagerService::ServiceState::Stopping) &&
              (stopping_info->reference_count == 0);
    service->allow_stop();
    stop_thread.join();

    auto stopped_info = manager.get_service_info("transition");
    success = success && stopped_info &&
              (stopped_info->state == ManagerService::ServiceState::Stopped) &&
              (stopped_info->reference_count == 0);
    if (!success) {
        std::cerr << "Service lifecycle state snapshot verification failed" << '\n';
    }
    return success;
}

bool verify_schema_query_during_removal(ServiceManager &manager)
{
    auto service = std::make_shared<EchoService>("schema_removal");
    if (!manager.add_service(service)) {
        return false;
    }
    std::atomic<bool> stop{false};
    boost::thread query_thread([&]() {
        while (!stop.load()) {
            (void)manager.get_service_schema("schema_removal");
            (void)manager.get_service_function_schema("schema_removal", "echo");
            (void)manager.get_service_event_schema("schema_removal", "changed");
        }
    });
    const bool removed = manager.remove_service("schema_removal");
    stop.store(true);
    query_thread.join();
    if (!removed || manager.get_service_schema("schema_removal")) {
        std::cerr << "Concurrent schema query/removal verification failed" << '\n';
        return false;
    }
    return true;
}

bool verify_manager_service(ServiceManager &manager)
{
    if ((ManagerService::get_static_function_schemas().size() != 5) ||
            !ManagerService::get_static_event_schemas().empty()) {
        std::cerr << "Manager schema still contains debug members" << '\n';
        return false;
    }

    auto binding = manager.bind(ManagerService::get_name().data());
    if (!binding.is_valid()) {
        std::cerr << "Failed to bind Manager service" << '\n';
        return false;
    }
    auto service = binding.get_service();
    auto names_result = service->call_function_sync(
                            BROOKESIA_DESCRIBE_TO_STR(ManagerService::FunctionId::GetServiceNames),
                            FunctionParameterMap{}
                        );
    std::vector<std::string> names;
    if (!names_result.success || !names_result.has_data() ||
            !std::holds_alternative<boost::json::array>(*names_result.data) ||
            !BROOKESIA_DESCRIBE_FROM_JSON(std::get<boost::json::array>(*names_result.data), names) ||
            !std::is_sorted(names.begin(), names.end()) ||
            !std::binary_search(names.begin(), names.end(), ManagerService::get_name().data()) ||
            !std::binary_search(names.begin(), names.end(), UtilsService::get_name().data())) {
        std::cerr << "Manager service names are invalid" << '\n';
        return false;
    }

    auto manager_info = manager.get_service_info(ManagerService::get_name().data());
    auto utils_info = manager.get_service_info(UtilsService::get_name().data());
    if (!manager_info || !utils_info || (manager_info->reference_count != 1) ||
            (utils_info->reference_count != 0)) {
        std::cerr << "Built-in service reference counts are invalid" << '\n';
        return false;
    }

    auto overview = manager.get_service_schema("echo");
    if (!overview || (overview->description != "Test service.") ||
            !std::binary_search(overview->function_names.begin(), overview->function_names.end(), "echo") ||
            !std::binary_search(overview->event_names.begin(), overview->event_names.end(), "changed")) {
        std::cerr << "Manager service schema overview is invalid" << '\n';
        return false;
    }
    auto function_schema = manager.get_service_function_schema("echo", "echo");
    auto event_schema = manager.get_service_event_schema("echo", "changed");
    if (!function_schema || (function_schema->name != "echo") ||
            !event_schema || (event_schema->name != "changed")) {
        std::cerr << "Manager copied function or event schema incorrectly" << '\n';
        return false;
    }
    return true;
}

bool verify_utils_service(ServiceManager &manager)
{
    if ((UtilsService::get_static_function_schemas().size() != 10) ||
            (UtilsService::get_static_event_schemas().size() != 3)) {
        std::cerr << "Utils schema has unexpected members" << '\n';
        return false;
    }

    auto binding = manager.bind(UtilsService::get_name().data());
    if (!binding.is_valid()) {
        std::cerr << "Failed to bind Utils service" << '\n';
        return false;
    }
    auto service = binding.get_service();

    std::atomic<int> state_event_count{0};
    std::atomic<int> memory_event_count{0};
    auto state_connection = service->subscribe_event(
                                BROOKESIA_DESCRIBE_TO_STR(UtilsService::EventId::DebugStateChanged),
    [&state_event_count](const std::string &, const EventItemMap &) {
        state_event_count.fetch_add(1);
    }
                            );
    auto memory_connection = service->subscribe_event(
                                 BROOKESIA_DESCRIBE_TO_STR(UtilsService::EventId::MemoryDebugSnapshotUpdated),
    [&memory_event_count](const std::string &, const EventItemMap &) {
        memory_event_count.fetch_add(1);
    }
                             );

    auto initial_state_result = service->call_function_sync(
                                    BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::GetDebugState),
                                    FunctionParameterMap{}
                                );
    UtilsService::DebugRuntimeState initial_state;
    if (!initial_state_result.success || !initial_state_result.has_data() ||
            !BROOKESIA_DESCRIBE_FROM_JSON(
                std::get<boost::json::object>(*initial_state_result.data), initial_state
            ) || (initial_state.memory != UtilsService::DebugFeatureState::Stopped)) {
        std::cerr << "Utils did not start with stopped memory debug" << '\n';
        return false;
    }

    auto take_memory_snapshot = [&]() -> std::optional<UtilsService::MemoryDebugSnapshot> {
        auto result = service->call_function_sync(
            BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::GetMemorySnapshot),
        FunctionParameterMap{}
        );
        UtilsService::MemoryDebugSnapshot snapshot;
        if (!result.success || !result.has_data() ||
                !BROOKESIA_DESCRIBE_FROM_JSON(std::get<boost::json::object>(*result.data), snapshot))
        {
            return std::nullopt;
        }
        return snapshot;
    };
    auto first_snapshot = take_memory_snapshot();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(2));
    auto second_snapshot = take_memory_snapshot();
    if (!first_snapshot || !second_snapshot ||
            (second_snapshot->timestamp_ms <= first_snapshot->timestamp_ms) ||
            (state_event_count.load() != 0) || (memory_event_count.load() != 0)) {
        std::cerr << "Utils immediate memory snapshot changed profiler state or events" << '\n';
        return false;
    }

    auto cached_result = service->call_function_sync(
                             BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::GetDebugSnapshot),
                             FunctionParameterMap{}
                         );
    UtilsService::DebugSnapshot cached_snapshot;
    if (!cached_result.success || !cached_result.has_data() ||
            !BROOKESIA_DESCRIBE_FROM_JSON(
                std::get<boost::json::object>(*cached_result.data), cached_snapshot
            ) || cached_snapshot.memory) {
        std::cerr << "Utils immediate memory snapshot modified the profiler cache" << '\n';
        return false;
    }

    UtilsService::DebugConfig new_config;
    new_config.memory_sample_interval_ms = 100;
    auto set_result = service->call_function_sync(
                          BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::SetDebugConfig),
    FunctionParameterMap{{
            BROOKESIA_DESCRIBE_TO_STR(
                UtilsService::FunctionSetDebugConfigParam::Config
            ),
            BROOKESIA_DESCRIBE_TO_JSON(new_config).as_object(),
        }}
                      );
    if (!set_result.success) {
        std::cerr << "Utils debug configuration call failed" << '\n';
        return false;
    }

    auto thread_result = service->call_function_sync(
                             BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::StartThreadDebug),
                             FunctionParameterMap{}
                         );
    if (thread_result.success || (thread_result.error_message.find("unsupported") == std::string::npos)) {
        std::cerr << "Utils did not reject PC thread debug" << '\n';
        return false;
    }

    auto start_memory_result = service->call_function_sync(
                                   BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::StartMemoryDebug),
                                   FunctionParameterMap{}
                               );
    if (!start_memory_result.success) {
        std::cerr << "Utils failed to start memory debug" << '\n';
        return false;
    }
    boost::this_thread::sleep_for(boost::chrono::milliseconds(250));

    cached_result = service->call_function_sync(
                        BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::GetDebugSnapshot),
                        FunctionParameterMap{}
                    );
    if (!cached_result.success || !cached_result.has_data() ||
            !BROOKESIA_DESCRIBE_FROM_JSON(
                std::get<boost::json::object>(*cached_result.data), cached_snapshot
            ) || (cached_snapshot.state.memory != UtilsService::DebugFeatureState::Running) ||
            !cached_snapshot.memory) {
        std::cerr << "Utils cached memory debug snapshot is invalid" << '\n';
        return false;
    }

    auto running_snapshot = take_memory_snapshot();
    if (!running_snapshot ||
            (cached_snapshot.memory && (running_snapshot->timestamp_ms < cached_snapshot.memory->timestamp_ms))) {
        std::cerr << "Utils failed to capture memory while profiling" << '\n';
        return false;
    }

    auto stop_memory_result = service->call_function_sync(
                                  BROOKESIA_DESCRIBE_TO_STR(UtilsService::FunctionId::StopMemoryDebug),
                                  FunctionParameterMap{}
                              );
    if (!stop_memory_result.success) {
        std::cerr << "Utils failed to stop memory debug" << '\n';
        return false;
    }
    for (int retry = 0; retry < 100 && state_event_count.load() < 4; ++retry) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(5));
    }
    if (!state_connection.connected() || !memory_connection.connected() ||
            (state_event_count.load() < 4) || (memory_event_count.load() < 1)) {
        std::cerr << "Utils did not publish debug state or memory snapshot events" << '\n';
        return false;
    }
    const auto stopped_memory_event_count = memory_event_count.load();
    boost::this_thread::sleep_for(boost::chrono::milliseconds(150));
    if (memory_event_count.load() != stopped_memory_event_count || !take_memory_snapshot()) {
        std::cerr << "Utils memory snapshot behavior after stopping is invalid" << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    auto &manager = ServiceManager::get_instance();

    if (!manager.init()) {
        std::cerr << "Failed to initialize service manager" << std::endl;
        return EXIT_FAILURE;
    }

    if (manager.add_service(std::make_shared<InvalidVersionService>())) {
        std::cerr << "Service with empty version was accepted" << '\n';
        return EXIT_FAILURE;
    }
    if (manager.add_service(std::make_shared<InvalidDescriptionService>())) {
        std::cerr << "Service with empty description was accepted" << '\n';
        return EXIT_FAILURE;
    }
    auto service = std::make_shared<EchoService>("echo");
    if (!manager.add_service(service)) {
        std::cerr << "Failed to add test service" << std::endl;
        return EXIT_FAILURE;
    }
    if (!manager.start()) {
        std::cerr << "Failed to start service manager" << std::endl;
        return EXIT_FAILURE;
    }
    if (!verify_version_metadata(manager, *service)) {
        return EXIT_FAILURE;
    }
    auto secondary_service = std::make_shared<EchoService>("echo_secondary", ServiceBase::SchedulerType::Secondary);
    if (!manager.add_service(secondary_service)) {
        std::cerr << "Failed to add secondary scheduler test service" << std::endl;
        return EXIT_FAILURE;
    }
    auto caller_service = std::make_shared<EchoService>("echo_caller");
    caller_service->set_peer(service);
    if (!manager.add_service(caller_service)) {
        std::cerr << "Failed to add caller test service" << std::endl;
        return EXIT_FAILURE;
    }

    auto binding = manager.bind("echo");
    if (!binding.is_valid()) {
        std::cerr << "Failed to bind test service" << std::endl;
        return EXIT_FAILURE;
    }

    auto bound_service = binding.get_service();
    if (!bound_service || !verify_echo_call(*bound_service) || !verify_nested_sync_call(*bound_service)) {
        return EXIT_FAILURE;
    }

    auto caller_binding = manager.bind("echo_caller");
    if (!caller_binding.is_valid()) {
        std::cerr << "Failed to bind caller test service" << std::endl;
        return EXIT_FAILURE;
    }
    if (!verify_cross_service_serialization(*service, *caller_service) ||
            !verify_wait_slot_release_after_timeout(*service, *caller_service) ||
            !verify_cross_service_wait_capacity(manager)) {
        return EXIT_FAILURE;
    }
    if (!verify_service_states(manager) || !verify_schema_query_during_removal(manager) ||
            !verify_manager_service(manager) || !verify_utils_service(manager)) {
        return EXIT_FAILURE;
    }

    auto secondary_binding = manager.bind("echo_secondary");
    if (!secondary_binding.is_valid()) {
        std::cerr << "Failed to bind secondary scheduler test service" << std::endl;
        return EXIT_FAILURE;
    }

    auto secondary_bound_service = secondary_binding.get_service();
    if (!secondary_bound_service || !verify_echo_call(*secondary_bound_service)) {
        return EXIT_FAILURE;
    }

    auto external_manager_binding = manager.bind(std::string(ManagerService::get_name()));
    auto external_utils_binding = manager.bind(std::string(UtilsService::get_name()));
    if (!external_manager_binding.is_valid() || !external_utils_binding.is_valid()) {
        std::cerr << "Failed to retain external built-in service bindings" << std::endl;
        return EXIT_FAILURE;
    }

    manager.stop();
    if (external_manager_binding.is_valid() || external_utils_binding.is_valid()) {
        std::cerr << "A built-in service remained running after ServiceManager stopped" << std::endl;
        return EXIT_FAILURE;
    }
    manager.deinit();

    if (!verify_single_worker_sync_failure(manager)) {
        return EXIT_FAILURE;
    }

    std::cout << "brookesia_service_manager host smoke test passed" << std::endl;
    return EXIT_SUCCESS;
}
