/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_super/macro_configs.h"
#if !BROOKESIA_SYSTEM_SUPER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/shell_app.hpp"
#include "private/system_constants.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(ESP_PLATFORM)
#   include "freertos/FreeRTOS.h"
#endif

namespace esp_brookesia::system::super {
namespace {

using DebugFeatureState = service::UtilsService::DebugFeatureState;
using HeapDebugInfo = service::UtilsService::HeapDebugInfo;
using ThreadTaskDebugInfo = service::UtilsService::ThreadTaskDebugInfo;

inline constexpr const char *DEBUG_NORMAL_BG = "#111827";
inline constexpr const char *DEBUG_WARNING_BG = "#991b1b";
inline constexpr const char *DEBUG_NORMAL_TEXT = "#f8fafc";
inline constexpr const char *DEBUG_WARNING_TEXT = "#fff7ed";

std::string bool_to_binding(bool value)
{
    return value ? "true" : "false";
}

uint32_t bytes_to_kb(uint64_t bytes)
{
    return static_cast<uint32_t>(bytes / 1024);
}

std::string format_bytes_compact(uint64_t bytes)
{
    return bytes >= 1024 ? std::to_string(bytes_to_kb(bytes)) + "K" : std::to_string(bytes) + "B";
}

std::string truncate_name(std::string_view name, size_t max_length)
{
    if (name.size() <= max_length) {
        return std::string(name);
    }
    if (max_length <= 1) {
        return std::string(name.substr(0, max_length));
    }
    std::string result(name.substr(0, max_length - 1));
    result.push_back('.');
    return result;
}

std::string format_heap_free_text(const HeapDebugInfo &heap)
{
    if (heap.total_size == 0) {
        return "F N/A";
    }
    std::ostringstream stream;
    stream << "F " << heap.free_percent << "% " << bytes_to_kb(heap.free_size) << "K";
    return stream.str();
}

std::string format_heap_largest_text(const HeapDebugInfo &heap)
{
    return heap.total_size == 0 ? "L N/A" : "L " + std::to_string(bytes_to_kb(heap.largest_free_block)) + "K";
}

bool is_heap_free_warning(const HeapDebugInfo &heap, uint32_t threshold)
{
    return (heap.total_size > 0) && (heap.free_percent < threshold);
}

bool is_heap_largest_warning(const HeapDebugInfo &heap, uint32_t threshold_kb)
{
    return (heap.total_size > 0) && (bytes_to_kb(heap.largest_free_block) < threshold_kb);
}

void add_metric_chip_update(
    std::vector<gui::BindingValueUpdate> &updates, std::string_view chip_path,
    std::string_view label_path, std::string text, bool warning
)
{
    add_binding_update(updates, std::string(chip_path), "bg", warning ? DEBUG_WARNING_BG : DEBUG_NORMAL_BG);
    add_binding_update(updates, std::string(label_path), "text", std::move(text));
    add_binding_update(
        updates, std::string(label_path), "text_color", warning ? DEBUG_WARNING_TEXT : DEBUG_NORMAL_TEXT
    );
}

int32_t get_supported_thread_core_count()
{
#if defined(ESP_PLATFORM)
    return std::max<int32_t>(portNUM_PROCESSORS, 1);
#else
    return 0;
#endif
}

bool is_idle_task_name(std::string_view name, int32_t core_id)
{
    if (name == "IDLE") {
        return true;
    }
    return name == std::string("IDLE") + std::to_string(core_id);
}

std::string format_thread_task_text(int32_t core_id, const std::optional<ThreadTaskDebugInfo> &task)
{
    return task ? "C" + std::to_string(core_id) + " " + truncate_name(task->name, 10)
           : "C" + std::to_string(core_id) + " --";
}

std::string format_thread_cpu_text(const std::optional<ThreadTaskDebugInfo> &task)
{
    return task ? "CPU " + std::to_string(task->cpu_percent) + "%" : "CPU --";
}

std::string format_thread_hwm_text(
    const std::optional<ThreadTaskDebugInfo> &hwm_task,
    const std::optional<ThreadTaskDebugInfo> &top_task
)
{
    if (!hwm_task) {
        return "H --";
    }
    std::string prefix = "H ";
    if (top_task && top_task->name != hwm_task->name) {
        prefix += truncate_name(hwm_task->name, 5) + " ";
    }
    return prefix + format_bytes_compact(hwm_task->stack_high_water_mark);
}

} // namespace

void ShellApp::apply_debug_config(const DebugConfig &config)
{
    {
        std::lock_guard lock(debug_mutex_);
        debug_config_ = config;
    }
    refresh_debug_overlay_visibility();
}

void ShellApp::apply_debug_state(const DebugRuntimeState &state)
{
    {
        std::lock_guard lock(debug_mutex_);
        debug_state_ = state;
    }
    refresh_debug_overlay_visibility();
}

void ShellApp::apply_memory_debug_snapshot(const MemoryDebugSnapshot &snapshot)
{
    refresh_memory_debug_box(snapshot);
}

void ShellApp::apply_thread_debug_snapshot(const ThreadDebugSnapshot &snapshot)
{
    refresh_thread_debug_box(snapshot);
}

ShellApp::DebugConfig ShellApp::get_debug_config_snapshot() const
{
    std::lock_guard lock(debug_mutex_);
    return debug_config_;
}

void ShellApp::stop_debug_runtime()
{
    if (context_ == nullptr || !overlay_mounted_) {
        return;
    }
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_DEBUG_PANEL_PATH, "hidden", "true");
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to hide Utils debug overlay: %1%", result.error());
    }
}

void ShellApp::refresh_debug_overlay_visibility()
{
    if (context_ == nullptr || !overlay_mounted_) {
        return;
    }

    DebugRuntimeState state;
    {
        std::lock_guard lock(debug_mutex_);
        state = debug_state_;
    }
    const bool show_memory = state.memory == DebugFeatureState::Running;
    const bool show_thread = state.thread == DebugFeatureState::Running;
    const auto core_count = get_supported_thread_core_count();
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(updates, SUPER_DEBUG_PANEL_PATH, "hidden", bool_to_binding(!show_memory && !show_thread));
    add_binding_update(updates, SUPER_DEBUG_MEMORY_PATH, "hidden", bool_to_binding(!show_memory));
    add_binding_update(updates, SUPER_DEBUG_THREAD_PATH, "hidden", bool_to_binding(!show_thread));
    add_binding_update(updates, SUPER_DEBUG_THREAD_CORE1_PATH, "hidden", bool_to_binding(core_count < 2));
    if (!show_memory) {
        add_metric_chip_update(
            updates, SUPER_DEBUG_MEMORY_SRAM_FREE_PATH, SUPER_DEBUG_MEMORY_SRAM_FREE_LABEL_PATH,
            "F N/A", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_MEMORY_SRAM_LARGEST_PATH, SUPER_DEBUG_MEMORY_SRAM_LARGEST_LABEL_PATH,
            "L N/A", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_MEMORY_PSRAM_FREE_PATH, SUPER_DEBUG_MEMORY_PSRAM_FREE_LABEL_PATH,
            "F N/A", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_MEMORY_PSRAM_LARGEST_PATH, SUPER_DEBUG_MEMORY_PSRAM_LARGEST_LABEL_PATH,
            "L N/A", false
        );
    }
    if (!show_thread) {
        add_binding_update(updates, SUPER_DEBUG_THREAD_CORE0_TASK_PATH, "text", "C0 --");
        add_binding_update(updates, SUPER_DEBUG_THREAD_CORE1_TASK_PATH, "text", "C1 --");
        add_metric_chip_update(
            updates, SUPER_DEBUG_THREAD_CORE0_CPU_PATH, SUPER_DEBUG_THREAD_CORE0_CPU_LABEL_PATH,
            "CPU --", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_THREAD_CORE0_HWM_PATH, SUPER_DEBUG_THREAD_CORE0_HWM_LABEL_PATH,
            "H --", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_THREAD_CORE1_CPU_PATH, SUPER_DEBUG_THREAD_CORE1_CPU_LABEL_PATH,
            "CPU --", false
        );
        add_metric_chip_update(
            updates, SUPER_DEBUG_THREAD_CORE1_HWM_PATH, SUPER_DEBUG_THREAD_CORE1_HWM_LABEL_PATH,
            "H --", false
        );
    }
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Utils debug overlay visibility: %1%", result.error());
    }
}

void ShellApp::refresh_memory_debug_box(const MemoryDebugSnapshot &snapshot)
{
    if (context_ == nullptr || !overlay_mounted_) {
        return;
    }
    const auto config = get_debug_config_snapshot();
    std::vector<gui::BindingValueUpdate> updates;
    add_metric_chip_update(
        updates, SUPER_DEBUG_MEMORY_SRAM_FREE_PATH, SUPER_DEBUG_MEMORY_SRAM_FREE_LABEL_PATH,
        format_heap_free_text(snapshot.internal),
        is_heap_free_warning(snapshot.internal, config.memory_internal_free_percent_threshold)
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_MEMORY_SRAM_LARGEST_PATH, SUPER_DEBUG_MEMORY_SRAM_LARGEST_LABEL_PATH,
        format_heap_largest_text(snapshot.internal),
        is_heap_largest_warning(snapshot.internal, config.memory_internal_largest_free_block_threshold_kb)
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_MEMORY_PSRAM_FREE_PATH, SUPER_DEBUG_MEMORY_PSRAM_FREE_LABEL_PATH,
        format_heap_free_text(snapshot.external),
        is_heap_free_warning(snapshot.external, config.memory_external_free_percent_threshold)
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_MEMORY_PSRAM_LARGEST_PATH, SUPER_DEBUG_MEMORY_PSRAM_LARGEST_LABEL_PATH,
        format_heap_largest_text(snapshot.external),
        is_heap_largest_warning(snapshot.external, config.memory_external_largest_free_block_threshold_kb)
    );
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Utils memory debug box: %1%", result.error());
    }
}

void ShellApp::refresh_thread_debug_box(const ThreadDebugSnapshot &snapshot)
{
    if (context_ == nullptr || !overlay_mounted_) {
        return;
    }
    struct CoreDebugState {
        std::optional<ThreadTaskDebugInfo> top_task;
        std::optional<ThreadTaskDebugInfo> hwm_task;
        bool cpu_warning = false;
        bool hwm_warning = false;
    };
    const auto config = get_debug_config_snapshot();
    const auto core_count = std::clamp<int32_t>(get_supported_thread_core_count(), 1, 2);
    std::array<CoreDebugState, 2> cores;
    for (const auto &task : snapshot.tasks) {
        if (task.core_id < 0 || task.core_id >= core_count) {
            continue;
        }
        auto &core = cores[static_cast<size_t>(task.core_id)];
        if (!core.top_task || task.cpu_percent > core.top_task->cpu_percent) {
            core.top_task = task;
        }
        if (!core.hwm_task || task.stack_high_water_mark < core.hwm_task->stack_high_water_mark) {
            core.hwm_task = task;
        }
        core.hwm_warning |= task.stack_high_water_mark <= config.thread_stack_high_water_mark_threshold_bytes;
        core.cpu_warning |= is_idle_task_name(task.name, task.core_id) &&
                            task.cpu_percent < config.thread_idle_cpu_percent_threshold;
    }
    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates, SUPER_DEBUG_THREAD_CORE0_TASK_PATH, "text",
        format_thread_task_text(0, cores[0].top_task)
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_THREAD_CORE0_CPU_PATH, SUPER_DEBUG_THREAD_CORE0_CPU_LABEL_PATH,
        format_thread_cpu_text(cores[0].top_task), cores[0].cpu_warning
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_THREAD_CORE0_HWM_PATH, SUPER_DEBUG_THREAD_CORE0_HWM_LABEL_PATH,
        format_thread_hwm_text(cores[0].hwm_task, cores[0].top_task), cores[0].hwm_warning
    );
    add_binding_update(updates, SUPER_DEBUG_THREAD_CORE1_PATH, "hidden", bool_to_binding(core_count < 2));
    add_binding_update(
        updates, SUPER_DEBUG_THREAD_CORE1_TASK_PATH, "text",
        format_thread_task_text(1, cores[1].top_task)
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_THREAD_CORE1_CPU_PATH, SUPER_DEBUG_THREAD_CORE1_CPU_LABEL_PATH,
        format_thread_cpu_text(cores[1].top_task), cores[1].cpu_warning
    );
    add_metric_chip_update(
        updates, SUPER_DEBUG_THREAD_CORE1_HWM_PATH, SUPER_DEBUG_THREAD_CORE1_HWM_LABEL_PATH,
        format_thread_hwm_text(cores[1].hwm_task, cores[1].top_task), cores[1].hwm_warning
    );
    auto result = context_->gui().set_binding_values(updates);
    if (!result) {
        BROOKESIA_LOGW("Failed to refresh Utils thread debug box: %1%", result.error());
    }
}

} // namespace esp_brookesia::system::super
