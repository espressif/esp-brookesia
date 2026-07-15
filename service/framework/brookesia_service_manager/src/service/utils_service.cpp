/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <chrono>
#include <utility>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"

#include "brookesia/lib_utils/memory_profiler.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#if defined(ESP_PLATFORM)
#   include "brookesia/lib_utils/thread_profiler.hpp"
#endif

#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/service/utils_service.hpp"

namespace esp_brookesia::service {
namespace {

using FunctionId = UtilsService::FunctionId;
using EventId = UtilsService::EventId;

FunctionSchema make_function_schema(
    FunctionId id, std::string description, std::optional<FunctionValueType> return_type = std::nullopt,
    std::vector<FunctionParameterSchema> parameters = {}, bool require_scheduler = false
)
{
    FunctionSchema schema{
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .parameters = std::move(parameters),
        .require_scheduler = require_scheduler,
    };
    if (return_type) {
        schema.return_value = FunctionReturnSchema{
            .type = *return_type,
            .description = "Function result.",
        };
    }
    return schema;
}

FunctionParameterSchema make_parameter(std::string name, std::string description, FunctionValueType type)
{
    return {
        .name = std::move(name),
        .description = std::move(description),
        .type = type,
    };
}

EventSchema make_event_schema(EventId id, std::string description, std::string item_name)
{
    return {
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .items = {{
                .name = std::move(item_name),
                .description = "Latest debug runtime value.",
                .type = EventItemType::Object,
            }
        },
        .require_scheduler = true,
    };
}

uint64_t to_timestamp_ms(const std::chrono::system_clock::time_point &timestamp)
{
    return static_cast<uint64_t>(
               std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count()
           );
}

UtilsService::HeapDebugInfo make_heap_debug_info(const lib_utils::MemoryProfiler::HeapInfo &info)
{
    return {
        .total_size = info.total_size,
        .free_size = info.free_size,
        .largest_free_block = info.largest_free_block,
        .free_percent = static_cast<uint32_t>(info.free_percent),
    };
}

UtilsService::DebugConfig normalize_debug_config(UtilsService::DebugConfig config)
{
    config.memory_sample_interval_ms = std::clamp<uint32_t>(config.memory_sample_interval_ms, 100U, 60000U);
    config.memory_internal_free_percent_threshold =
        std::clamp<uint32_t>(config.memory_internal_free_percent_threshold, 1U, 100U);
    config.memory_external_free_percent_threshold =
        std::clamp<uint32_t>(config.memory_external_free_percent_threshold, 1U, 100U);
    config.memory_internal_largest_free_block_threshold_kb =
        std::clamp<uint32_t>(config.memory_internal_largest_free_block_threshold_kb, 1U, 1024U * 1024U);
    config.memory_external_largest_free_block_threshold_kb =
        std::clamp<uint32_t>(config.memory_external_largest_free_block_threshold_kb, 1U, 1024U * 1024U);
    config.thread_profiling_interval_ms = std::clamp<uint32_t>(config.thread_profiling_interval_ms, 200U, 60000U);
    config.thread_sampling_duration_ms = std::clamp<uint32_t>(
            config.thread_sampling_duration_ms, 10U, config.thread_profiling_interval_ms - 1U
                                         );
    config.thread_idle_cpu_percent_threshold =
        std::clamp<uint32_t>(config.thread_idle_cpu_percent_threshold, 0U, 100U);
    config.thread_stack_high_water_mark_threshold_bytes =
        std::clamp<uint32_t>(config.thread_stack_high_water_mark_threshold_bytes, 1U, 1024U * 1024U);
    return config;
}

} // namespace

class UtilsService::Impl {
public:
    explicit Impl(UtilsService &owner)
        : owner_(owner)
    {}

    DebugCapabilities get_debug_capabilities() const
    {
        return {
            .memory_debug_supported = true,
#if defined(ESP_PLATFORM)
            .thread_debug_supported = true,
#else
            .thread_debug_supported = false,
#endif
        };
    }

    DebugConfig get_debug_config() const
    {
        boost::lock_guard lock(mutex_);
        return config_;
    }

    std::expected<void, std::string> set_debug_config(DebugConfig config)
    {
        config = normalize_debug_config(std::move(config));
        bool restart_memory = false;
        bool restart_thread = false;
        {
            boost::lock_guard lock(mutex_);
            restart_memory = state_.memory == DebugFeatureState::Running;
            restart_thread = state_.thread == DebugFeatureState::Running;
            config_ = config;
        }

        if (restart_memory) {
            stop_memory_debug();
            auto result = start_memory_debug();
            if (!result) {
                return result;
            }
        }
        if (restart_thread) {
            stop_thread_debug();
            auto result = start_thread_debug();
            if (!result) {
                return result;
            }
        }
        return {};
    }

    std::expected<void, std::string> start_memory_debug()
    {
        DebugConfig config;
        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        {
            boost::lock_guard lock(mutex_);
            if ((state_.memory == DebugFeatureState::Running) ||
                    (state_.memory == DebugFeatureState::Starting)) {
                return {};
            }
            if (state_.memory == DebugFeatureState::Stopping) {
                return std::unexpected("Memory debug is stopping");
            }
            state_.memory = DebugFeatureState::Starting;
            auto scheduler_result = ensure_scheduler_locked();
            if (!scheduler_result) {
                state_.memory = DebugFeatureState::Stopped;
                return scheduler_result;
            }
            scheduler = debug_scheduler_;
            config = config_;
        }
        publish_state_changed();

        auto &profiler = lib_utils::MemoryProfiler::get_instance();
        profiler.configure_profiling({
            .sample_interval_ms = config.memory_sample_interval_ms,
            .enable_auto_logging = false,
        });
        auto connection = profiler.connect_profiling_signal(
        [this](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
            on_memory_snapshot(snapshot);
        }
                          );
        if (!profiler.start_profiling(scheduler, config.memory_sample_interval_ms)) {
            connection.disconnect();
            {
                boost::lock_guard lock(mutex_);
                state_.memory = DebugFeatureState::Stopped;
            }
            publish_state_changed();
            return std::unexpected("Failed to start memory debug");
        }
        {
            boost::lock_guard lock(mutex_);
            memory_connection_ = std::move(connection);
            state_.memory = DebugFeatureState::Running;
        }
        publish_state_changed();
        return {};
    }

    void stop_memory_debug()
    {
        {
            boost::lock_guard lock(mutex_);
            if ((state_.memory == DebugFeatureState::Stopped) ||
                    (state_.memory == DebugFeatureState::Stopping)) {
                return;
            }
            state_.memory = DebugFeatureState::Stopping;
        }
        publish_state_changed();
        memory_connection_.disconnect();
        lib_utils::MemoryProfiler::get_instance().stop_profiling();
        {
            boost::lock_guard lock(mutex_);
            state_.memory = DebugFeatureState::Stopped;
        }
        publish_state_changed();
    }

    std::expected<void, std::string> start_thread_debug()
    {
#if !defined(ESP_PLATFORM)
        return std::unexpected("Thread debug is unsupported on this platform");
#else
        DebugConfig config;
        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        {
            boost::lock_guard lock(mutex_);
            if ((state_.thread == DebugFeatureState::Running) ||
                    (state_.thread == DebugFeatureState::Starting)) {
                return {};
            }
            if (state_.thread == DebugFeatureState::Stopping) {
                return std::unexpected("Thread debug is stopping");
            }
            state_.thread = DebugFeatureState::Starting;
            auto scheduler_result = ensure_scheduler_locked();
            if (!scheduler_result) {
                state_.thread = DebugFeatureState::Stopped;
                return scheduler_result;
            }
            scheduler = debug_scheduler_;
            config = config_;
        }
        publish_state_changed();

        auto &profiler = lib_utils::ThreadProfiler::get_instance();
        profiler.configure_profiling({
            .sampling_duration_ms = config.thread_sampling_duration_ms,
            .profiling_interval_ms = config.thread_profiling_interval_ms,
            .primary_sort = lib_utils::ThreadProfiler::PrimarySortBy::CoreId,
            .secondary_sort = lib_utils::ThreadProfiler::SecondarySortBy::CpuPercent,
            .enable_auto_logging = false,
        });
        auto connection = profiler.connect_profiling_signal(
        [this](const lib_utils::ThreadProfiler::ProfileSnapshot & snapshot) {
            on_thread_snapshot(snapshot);
        }
                          );
        if (!profiler.start_profiling(
                    scheduler, config.thread_sampling_duration_ms, config.thread_profiling_interval_ms
                )) {
            connection.disconnect();
            {
                boost::lock_guard lock(mutex_);
                state_.thread = DebugFeatureState::Stopped;
            }
            publish_state_changed();
            return std::unexpected("Failed to start thread debug");
        }
        {
            boost::lock_guard lock(mutex_);
            thread_connection_ = std::move(connection);
            state_.thread = DebugFeatureState::Running;
        }
        publish_state_changed();
        return {};
#endif
    }

    void stop_thread_debug()
    {
#if defined(ESP_PLATFORM)
        {
            boost::lock_guard lock(mutex_);
            if ((state_.thread == DebugFeatureState::Stopped) ||
                    (state_.thread == DebugFeatureState::Stopping))
            {
                return;
            }
            state_.thread = DebugFeatureState::Stopping;
        }
        publish_state_changed();
        thread_connection_.disconnect();
        lib_utils::ThreadProfiler::get_instance().stop_profiling();
        {
            boost::lock_guard lock(mutex_);
            state_.thread = DebugFeatureState::Stopped;
        }
        publish_state_changed();
#endif
    }

    DebugRuntimeState get_debug_state() const
    {
        boost::lock_guard lock(mutex_);
        return state_;
    }

    DebugSnapshot get_debug_snapshot() const
    {
        boost::lock_guard lock(mutex_);
        return {
            .state = state_,
            .config = config_,
            .memory = memory_snapshot_,
            .thread = thread_snapshot_,
        };
    }

    std::expected<MemoryDebugSnapshot, std::string> get_memory_snapshot() const
    {
        auto snapshot = lib_utils::MemoryProfiler::take_snapshot();
        if (!snapshot) {
            return std::unexpected("Failed to capture the current memory snapshot");
        }
        return MemoryDebugSnapshot{
            .timestamp_ms = to_timestamp_ms(snapshot->timestamp),
            .internal = make_heap_debug_info(snapshot->memory.internal),
            .external = make_heap_debug_info(snapshot->memory.external),
        };
    }

    void stop_all()
    {
        stop_memory_debug();
        stop_thread_debug();
        std::shared_ptr<lib_utils::TaskScheduler> scheduler;
        {
            boost::lock_guard lock(mutex_);
            memory_connection_.disconnect();
#if defined(ESP_PLATFORM)
            thread_connection_.disconnect();
#endif
            scheduler = std::move(debug_scheduler_);
        }
        if (scheduler) {
            scheduler->stop();
        }
    }

private:
    std::expected<void, std::string> ensure_scheduler_locked()
    {
        if (debug_scheduler_) {
            return {};
        }
        auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
        lib_utils::TaskScheduler::StartConfig start_config{
            .worker_configs = {{
                    .name = "UtilsDebug",
                    .stack_size = 12 * 1024,
                }
            },
        };
        if (!scheduler->start(start_config)) {
            return std::unexpected("Failed to start Utils debug scheduler");
        }
        debug_scheduler_ = std::move(scheduler);
        return {};
    }

    bool has_subscribers(EventId id) const
    {
        auto registry = owner_.get_event_registry();
        return registry && registry->has_subscribers(BROOKESIA_DESCRIBE_TO_STR(id));
    }

    void publish_state_changed()
    {
        if (!has_subscribers(EventId::DebugStateChanged)) {
            return;
        }
        DebugRuntimeState state = get_debug_state();
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(EventId::DebugStateChanged),
        EventItemMap{{
                BROOKESIA_DESCRIBE_TO_STR(EventDebugStateChangedItem::State),
                BROOKESIA_DESCRIBE_TO_JSON(state).as_object(),
            }
        },
        true
        );
    }

    void on_memory_snapshot(const lib_utils::MemoryProfiler::ProfileSnapshot &snapshot)
    {
        MemoryDebugSnapshot converted{
            .timestamp_ms = to_timestamp_ms(snapshot.timestamp),
            .internal = make_heap_debug_info(snapshot.memory.internal),
            .external = make_heap_debug_info(snapshot.memory.external),
        };
        {
            boost::lock_guard lock(mutex_);
            if (state_.memory != DebugFeatureState::Running) {
                return;
            }
            memory_snapshot_ = converted;
        }
        if (!has_subscribers(EventId::MemoryDebugSnapshotUpdated)) {
            return;
        }
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(EventId::MemoryDebugSnapshotUpdated),
        EventItemMap{{
                BROOKESIA_DESCRIBE_TO_STR(EventMemoryDebugSnapshotUpdatedItem::Snapshot),
                BROOKESIA_DESCRIBE_TO_JSON(converted).as_object(),
            }
        },
        true
        );
    }

#if defined(ESP_PLATFORM)
    void on_thread_snapshot(const lib_utils::ThreadProfiler::ProfileSnapshot &snapshot)
    {
        ThreadDebugSnapshot converted{
            .timestamp_ms = to_timestamp_ms(snapshot.timestamp),
            .tasks = {},
        };
        converted.tasks.reserve(snapshot.tasks.size());
        for (const auto &task : snapshot.tasks) {
            if (task.status != lib_utils::ThreadProfiler::TaskStatus::Normal) {
                continue;
            }
            converted.tasks.push_back({
                .name = task.name,
                .core_id = task.core_id,
                .cpu_percent = task.cpu_percent,
                .stack_high_water_mark = task.stack_high_water_mark,
            });
        }
        {
            boost::lock_guard lock(mutex_);
            if (state_.thread != DebugFeatureState::Running) {
                return;
            }
            thread_snapshot_ = converted;
        }
        if (!has_subscribers(EventId::ThreadDebugSnapshotUpdated)) {
            return;
        }
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(EventId::ThreadDebugSnapshotUpdated),
        EventItemMap{{
                BROOKESIA_DESCRIBE_TO_STR(EventThreadDebugSnapshotUpdatedItem::Snapshot),
                BROOKESIA_DESCRIBE_TO_JSON(converted).as_object(),
            }
        },
        true
        );
    }
#endif

    UtilsService &owner_;
    mutable boost::mutex mutex_;
    DebugConfig config_;
    DebugRuntimeState state_;
    std::optional<MemoryDebugSnapshot> memory_snapshot_;
    std::optional<ThreadDebugSnapshot> thread_snapshot_;
    std::shared_ptr<lib_utils::TaskScheduler> debug_scheduler_;
    lib_utils::MemoryProfiler::SignalConnection memory_connection_;
#if defined(ESP_PLATFORM)
    lib_utils::ThreadProfiler::SignalConnection thread_connection_;
#endif
};

UtilsService::UtilsService()
    : ServiceBase({
    .name = get_name().data(),
    .description = "Inspect memory and thread debug runtimes.",
    .version = make_version(
        BROOKESIA_SERVICE_MANAGER_VER_MAJOR,
        BROOKESIA_SERVICE_MANAGER_VER_MINOR,
        BROOKESIA_SERVICE_MANAGER_VER_PATCH
    ),
    .bindable = true,
})
, impl_(std::make_unique<Impl>(*this))
{
}

UtilsService::~UtilsService() = default;

std::span<const FunctionSchema> UtilsService::get_static_function_schemas()
{
    static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> SCHEMAS = {{
            make_function_schema(
                FunctionId::GetDebugCapabilities, "Get supported debug features.", FunctionValueType::Object
            ),
            make_function_schema(FunctionId::GetDebugConfig, "Get the debug configuration.", FunctionValueType::Object),
            make_function_schema(
                FunctionId::SetDebugConfig, "Update the debug configuration.", std::nullopt,
            {
                make_parameter(
                    BROOKESIA_DESCRIBE_TO_STR(FunctionSetDebugConfigParam::Config),
                    "Complete debug configuration.", FunctionValueType::Object
                )
            }, true
            ),
            make_function_schema(FunctionId::StartMemoryDebug, "Start memory profiling.", std::nullopt, {}, true),
            make_function_schema(FunctionId::StopMemoryDebug, "Stop memory profiling.", std::nullopt, {}, true),
            make_function_schema(FunctionId::StartThreadDebug, "Start thread profiling.", std::nullopt, {}, true),
            make_function_schema(FunctionId::StopThreadDebug, "Stop thread profiling.", std::nullopt, {}, true),
            make_function_schema(
                FunctionId::GetDebugState, "Get memory and thread debug states.", FunctionValueType::Object
            ),
            make_function_schema(
                FunctionId::GetDebugSnapshot, "Get the cached debug snapshot.", FunctionValueType::Object
            ),
            make_function_schema(
                FunctionId::GetMemorySnapshot, "Capture the current memory state.", FunctionValueType::Object
            ),
        }
    };
    return SCHEMAS;
}

std::span<const EventSchema> UtilsService::get_static_event_schemas()
{
    static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> SCHEMAS = {{
            make_event_schema(
                EventId::DebugStateChanged, "Debug runtime state changed.",
                BROOKESIA_DESCRIBE_TO_STR(EventDebugStateChangedItem::State)
            ),
            make_event_schema(
                EventId::MemoryDebugSnapshotUpdated, "Memory debug snapshot updated.",
                BROOKESIA_DESCRIBE_TO_STR(EventMemoryDebugSnapshotUpdatedItem::Snapshot)
            ),
            make_event_schema(
                EventId::ThreadDebugSnapshotUpdated, "Thread debug snapshot updated.",
                BROOKESIA_DESCRIBE_TO_STR(EventThreadDebugSnapshotUpdatedItem::Snapshot)
            ),
        }
    };
    return SCHEMAS;
}

std::vector<FunctionSchema> UtilsService::get_function_schemas()
{
    auto schemas = get_static_function_schemas();
    return {schemas.begin(), schemas.end()};
}

std::vector<EventSchema> UtilsService::get_event_schemas()
{
    auto schemas = get_static_event_schemas();
    return {schemas.begin(), schemas.end()};
}

ServiceBase::FunctionHandlerMap UtilsService::get_function_handlers()
{
    return {
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDebugCapabilities),
            [this](FunctionParameterMap &&)
            {
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(impl_->get_debug_capabilities()).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDebugConfig),
            [this](FunctionParameterMap &&)
            {
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(impl_->get_debug_config()).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetDebugConfig),
            [this](FunctionParameterMap &&args)
            {
                const auto &value = std::get<boost::json::object>(
                    args.at(BROOKESIA_DESCRIBE_TO_STR(FunctionSetDebugConfigParam::Config))
                );
                DebugConfig config;
                if (!BROOKESIA_DESCRIBE_FROM_JSON(value, config)) {
                    return to_function_result(std::expected<void, std::string>(
                                                  std::unexpected("Failed to parse Utils debug configuration")
                                              ));
                }
                return to_function_result(impl_->set_debug_config(std::move(config)));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartMemoryDebug),
            [this](FunctionParameterMap &&) { return to_function_result(impl_->start_memory_debug()); },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopMemoryDebug),
            [this](FunctionParameterMap &&)
            {
                impl_->stop_memory_debug();
                return to_function_result(std::expected<void, std::string> {});
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartThreadDebug),
            [this](FunctionParameterMap &&) { return to_function_result(impl_->start_thread_debug()); },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopThreadDebug),
            [this](FunctionParameterMap &&)
            {
                impl_->stop_thread_debug();
                return to_function_result(std::expected<void, std::string> {});
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDebugState),
            [this](FunctionParameterMap &&)
            {
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(impl_->get_debug_state()).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDebugSnapshot),
            [this](FunctionParameterMap &&)
            {
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(impl_->get_debug_snapshot()).as_object()
                                          ));
            },
        },
        {
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetMemorySnapshot),
            [this](FunctionParameterMap &&)
            {
                auto result = impl_->get_memory_snapshot();
                if (!result) {
                    return to_function_result(std::expected<boost::json::object, std::string>(
                                                  std::unexpected(result.error())
                                              ));
                }
                return to_function_result(std::expected<boost::json::object, std::string>(
                                              BROOKESIA_DESCRIBE_TO_JSON(*result).as_object()
                                          ));
            },
        },
    };
}

void UtilsService::on_stop()
{
    impl_->stop_all();
}

void UtilsService::on_deinit()
{
    impl_->stop_all();
}

} // namespace esp_brookesia::service
