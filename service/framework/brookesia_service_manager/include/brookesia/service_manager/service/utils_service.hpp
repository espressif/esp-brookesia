/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

class UtilsService final: public ServiceBase {
public:
    enum class DebugFeatureState : uint8_t {
        Stopped,
        Starting,
        Running,
        Stopping,
    };

    struct DebugConfig {
        uint32_t memory_sample_interval_ms = 1000;
        uint32_t memory_internal_free_percent_threshold = 10;
        uint32_t memory_internal_largest_free_block_threshold_kb = 100;
        uint32_t memory_external_free_percent_threshold = 10;
        uint32_t memory_external_largest_free_block_threshold_kb = 500;
        uint32_t thread_profiling_interval_ms = 5000;
        uint32_t thread_sampling_duration_ms = 1000;
        uint32_t thread_idle_cpu_percent_threshold = 1;
        uint32_t thread_stack_high_water_mark_threshold_bytes = 128;
    };

    struct DebugCapabilities {
        bool memory_debug_supported = true;
        bool thread_debug_supported = false;
    };

    struct DebugRuntimeState {
        DebugFeatureState memory = DebugFeatureState::Stopped;
        DebugFeatureState thread = DebugFeatureState::Stopped;
    };

    struct HeapDebugInfo {
        uint64_t total_size = 0;
        uint64_t free_size = 0;
        uint64_t largest_free_block = 0;
        uint32_t free_percent = 0;
    };

    struct MemoryDebugSnapshot {
        uint64_t timestamp_ms = 0;
        HeapDebugInfo internal;
        HeapDebugInfo external;
    };

    struct ThreadTaskDebugInfo {
        std::string name;
        int32_t core_id = -1;
        uint32_t cpu_percent = 0;
        uint32_t stack_high_water_mark = 0;
    };

    struct ThreadDebugSnapshot {
        uint64_t timestamp_ms = 0;
        std::vector<ThreadTaskDebugInfo> tasks;
    };

    struct DebugSnapshot {
        DebugRuntimeState state;
        DebugConfig config;
        std::optional<MemoryDebugSnapshot> memory;
        std::optional<ThreadDebugSnapshot> thread;
    };

    enum class FunctionId : uint8_t {
        GetDebugCapabilities,
        GetDebugConfig,
        SetDebugConfig,
        StartMemoryDebug,
        StopMemoryDebug,
        StartThreadDebug,
        StopThreadDebug,
        GetDebugState,
        GetDebugSnapshot,
        GetMemorySnapshot,
        Max,
    };

    enum class EventId : uint8_t {
        DebugStateChanged,
        MemoryDebugSnapshotUpdated,
        ThreadDebugSnapshotUpdated,
        Max,
    };

    enum class FunctionSetDebugConfigParam : uint8_t { Config };
    enum class EventDebugStateChangedItem : uint8_t { State };
    enum class EventMemoryDebugSnapshotUpdatedItem : uint8_t { Snapshot };
    enum class EventThreadDebugSnapshotUpdatedItem : uint8_t { Snapshot };

    UtilsService();
    ~UtilsService() override;

    static constexpr std::string_view get_name()
    {
        return "Utils";
    }

    static std::span<const FunctionSchema> get_static_function_schemas();
    static std::span<const EventSchema> get_static_event_schemas();

private:
    class Impl;

    std::vector<FunctionSchema> get_function_schemas() override;
    std::vector<EventSchema> get_event_schemas() override;
    FunctionHandlerMap get_function_handlers() override;
    void on_stop() override;
    void on_deinit() override;

    std::unique_ptr<Impl> impl_;
};

BROOKESIA_DESCRIBE_ENUM(UtilsService::DebugFeatureState, Stopped, Starting, Running, Stopping);
BROOKESIA_DESCRIBE_STRUCT(
    UtilsService::DebugConfig, (),
    (
        memory_sample_interval_ms,
        memory_internal_free_percent_threshold,
        memory_internal_largest_free_block_threshold_kb,
        memory_external_free_percent_threshold,
        memory_external_largest_free_block_threshold_kb,
        thread_profiling_interval_ms,
        thread_sampling_duration_ms,
        thread_idle_cpu_percent_threshold,
        thread_stack_high_water_mark_threshold_bytes
    )
);
BROOKESIA_DESCRIBE_STRUCT(
    UtilsService::DebugCapabilities, (), (memory_debug_supported, thread_debug_supported)
);
BROOKESIA_DESCRIBE_STRUCT(UtilsService::DebugRuntimeState, (), (memory, thread));
BROOKESIA_DESCRIBE_STRUCT(
    UtilsService::HeapDebugInfo, (), (total_size, free_size, largest_free_block, free_percent)
);
BROOKESIA_DESCRIBE_STRUCT(
    UtilsService::MemoryDebugSnapshot, (), (timestamp_ms, internal, external)
);
BROOKESIA_DESCRIBE_STRUCT(
    UtilsService::ThreadTaskDebugInfo, (), (name, core_id, cpu_percent, stack_high_water_mark)
);
BROOKESIA_DESCRIBE_STRUCT(UtilsService::ThreadDebugSnapshot, (), (timestamp_ms, tasks));
BROOKESIA_DESCRIBE_STRUCT(UtilsService::DebugSnapshot, (), (state, config, memory, thread));
BROOKESIA_DESCRIBE_ENUM(
    UtilsService::FunctionId,
    GetDebugCapabilities,
    GetDebugConfig,
    SetDebugConfig,
    StartMemoryDebug,
    StopMemoryDebug,
    StartThreadDebug,
    StopThreadDebug,
    GetDebugState,
    GetDebugSnapshot,
    GetMemorySnapshot,
    Max
);
BROOKESIA_DESCRIBE_ENUM(
    UtilsService::EventId, DebugStateChanged, MemoryDebugSnapshotUpdated, ThreadDebugSnapshotUpdated, Max
);
BROOKESIA_DESCRIBE_ENUM(UtilsService::FunctionSetDebugConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(UtilsService::EventDebugStateChangedItem, State);
BROOKESIA_DESCRIBE_ENUM(UtilsService::EventMemoryDebugSnapshotUpdatedItem, Snapshot);
BROOKESIA_DESCRIBE_ENUM(UtilsService::EventThreadDebugSnapshotUpdatedItem, Snapshot);

} // namespace esp_brookesia::service
