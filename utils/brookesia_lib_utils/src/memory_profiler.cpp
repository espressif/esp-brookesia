/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "esp_heap_caps.h"
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_MEMORY_PROFILER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/memory_profiler.hpp"

namespace esp_brookesia::lib_utils {

constexpr uint32_t MEMORY_PROFILER_STOP_TIMEOUT_MS = 100;

MemoryProfiler::Statistics::Statistics(const MemoryInfo &cur_memory, const Statistics &last_stats)
    : sample_count(last_stats.sample_count + 1)
    , min_total_free(
          last_stats.min_total_free == 0 ? cur_memory.total_free :
          std::min(cur_memory.total_free, last_stats.min_total_free)
      )
    , min_internal_free(
          last_stats.min_internal_free == 0 ? cur_memory.internal.free_size :
          std::min(cur_memory.internal.free_size, last_stats.min_internal_free)
      )
    , min_external_free(
          last_stats.min_external_free == 0 ? cur_memory.external.free_size :
          std::min(cur_memory.external.free_size, last_stats.min_external_free)
      )
    , min_total_free_percent(
          last_stats.min_total_free_percent == 0 ? cur_memory.total_free_percent :
          std::min(cur_memory.total_free_percent, last_stats.min_total_free_percent)
      )
    , min_internal_free_percent(
          last_stats.min_internal_free_percent == 0 ? cur_memory.internal.free_percent :
          std::min(cur_memory.internal.free_percent, last_stats.min_internal_free_percent)
      )
    , min_external_free_percent(
          last_stats.min_external_free_percent == 0 ? cur_memory.external.free_percent :
          std::min(cur_memory.external.free_percent, last_stats.min_external_free_percent)
      )
    , min_total_largest_free_block(
          last_stats.min_total_largest_free_block == 0 ? cur_memory.total_largest_free_block :
          std::min(cur_memory.total_largest_free_block, last_stats.min_total_largest_free_block)
      )
    , min_internal_largest_free_block(
          last_stats.min_internal_largest_free_block == 0 ? cur_memory.internal.largest_free_block :
          std::min(cur_memory.internal.largest_free_block, last_stats.min_internal_largest_free_block)
      )
    , min_external_largest_free_block(
          last_stats.min_external_largest_free_block == 0 ? cur_memory.external.largest_free_block :
          std::min(cur_memory.external.largest_free_block, last_stats.min_external_largest_free_block)
      )
{
}

MemoryProfiler::~MemoryProfiler()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_profiling()) {
        stop_profiling();
        reset_profiling();
    }
}

bool MemoryProfiler::configure_profiling(const ProfilingConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    boost::lock_guard lock(mutex_);

    config_ = config;

    return true;
}

bool MemoryProfiler::start_profiling(std::shared_ptr<TaskScheduler> scheduler, uint32_t period_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not valid");

    if (is_profiling()) {
        BROOKESIA_LOGD("Already profiling");
        return true;
    }

    if (!scheduler->is_running()) {
        BROOKESIA_LOGW("Scheduler is not running, starting it...");
        BROOKESIA_CHECK_FALSE_RETURN(scheduler->start(), false, "Failed to start scheduler");
    }

    boost::unique_lock lock(mutex_);

    task_scheduler_ = scheduler;

    lib_utils::FunctionGuard stop_guard([this, &lock]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        lock.unlock();
        stop_profiling();
    });

    if (period_ms == 0) {
        period_ms = config_.sample_interval_ms;
    }

    // Schedule periodic profiling task
    auto profiling_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        boost::lock_guard lock(mutex_);

        auto snapshot = take_snapshot(latest_snapshot_.get());
        BROOKESIA_CHECK_NULL_RETURN(snapshot, false, "Failed to take snapshot");

        latest_snapshot_ = snapshot;

        check_thresholds_and_trigger_callbacks(*snapshot);

        profiling_signal_(*snapshot);

        if (config_.enable_auto_logging) {
            print_snapshot(*snapshot);
        }

        return true;
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        scheduler->post_periodic(profiling_task, period_ms, &profiling_task_id_), false,
        "Failed to schedule profiling task"
    );

    stop_guard.release();

    BROOKESIA_LOGI(
        "Started profiling with config:\n%1%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config_, DESCRIBE_FORMAT_VERBOSE)
    );

    return true;
}

void MemoryProfiler::stop_profiling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_profiling()) {
        BROOKESIA_LOGD("Not profiling");
        return;
    }

    boost::lock_guard lock(mutex_);

    task_scheduler_->cancel(profiling_task_id_);
    BROOKESIA_CHECK_FALSE_EXECUTE(
    task_scheduler_->wait(profiling_task_id_, MEMORY_PROFILER_STOP_TIMEOUT_MS), {}, {
        BROOKESIA_LOGE("Wait for profiling task timeout after %1% ms", MEMORY_PROFILER_STOP_TIMEOUT_MS);
    });
    task_scheduler_.reset();

    BROOKESIA_LOGI("Stopped profiling");
}

void MemoryProfiler::reset_profiling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(mutex_);

    // Clear profiling data
    latest_snapshot_.reset();

    // Disconnect signals
    profiling_signal_.disconnect_all_slots();

    // Clear threshold listeners
    threshold_listeners_.clear();

    BROOKESIA_LOGD("Reset profiling data");
}

MemoryProfiler::SignalConnection MemoryProfiler::connect_profiling_signal(ProfilingSignalSlot slot)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return profiling_signal_.connect(slot);
}

MemoryProfiler::SignalConnection MemoryProfiler::connect_threshold_signal(
    ThresholdType type, uint32_t threshold_value, ThresholdSignalSlot slot
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: type(%1%), threshold_value(%2%)", BROOKESIA_DESCRIBE_TO_STR(type), threshold_value);

    boost::lock_guard lock(mutex_);

    // Find or create listener for this threshold type
    auto it = std::find_if(threshold_listeners_.begin(), threshold_listeners_.end(),
    [type, threshold_value](const ThresholdListener & listener) {
        return listener.type == type && listener.threshold_value == threshold_value;
    });

    if (it == threshold_listeners_.end()) {
        threshold_listeners_.emplace_back();
        it = threshold_listeners_.end() - 1;
        it->type = type;
        it->threshold_value = threshold_value;
    }

    return it->signal.connect(slot);
}

std::shared_ptr<MemoryProfiler::ProfileSnapshot> MemoryProfiler::take_snapshot(ProfileSnapshot *last_snapshot)
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::shared_ptr<ProfileSnapshot> snapshot;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        snapshot = std::make_shared<ProfileSnapshot>(), nullptr, "Failed to create snapshot"
    );
    snapshot->timestamp = std::chrono::system_clock::now();
    sample_memory(snapshot->memory);
    snapshot->stats = Statistics(snapshot->memory, last_snapshot ? last_snapshot->stats : Statistics());

    return snapshot;
}

void MemoryProfiler::print_snapshot(const ProfileSnapshot &snapshot)
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::ostringstream oss;
    oss << "\n==================== Memory Profiler Snapshot ====================\n";

    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(snapshot.timestamp);
    std::tm tm_buf;
    localtime_r(&time_t, &tm_buf);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
    oss << "Timestamp: " << time_str << "\n";

    // Memory information table
    // Column widths: 19, 13, 13, 14, 8
    const char *header_separator = "+-------------------+-------------+-------------+--------------+--------+";
    const char *row_separator = "+-------------------+-------------+-------------+--------------+--------+";

    oss << header_separator << "\n";
    oss << "| " << std::left << std::setw(17) << "Heap Type"
        << " | " << std::right << std::setw(11) << "Total (KB)"
        << " | " << std::right << std::setw(11) << "Free (KB)"
        << " | " << std::right << std::setw(12) << "Largest (KB)"
        << " | " << std::right << std::setw(5) << "Used %"
        << " |\n";
    oss << header_separator << "\n";

    // Internal SRAM
    oss << "| " << std::left << std::setw(17) << "Internal (SRAM)"
        << " | " << std::right << std::fixed << std::setprecision(3) << std::setw(11) << (snapshot.memory.internal.total_size / 1024)
        << " | " << std::setw(11) << (snapshot.memory.internal.free_size / 1024)
        << " | " << std::setw(12) << static_cast<size_t>(snapshot.memory.internal.largest_free_block / 1024)
        << " | " << std::setw(5) << snapshot.memory.internal.used_percent << "%"
        << " |\n";
    oss << row_separator << "\n";

#if CONFIG_SPIRAM
    // External PSRAM
    oss << "| " << std::left << std::setw(17) << "External (PSRAM)"
        << " | " << std::right << std::setw(11) << (snapshot.memory.external.total_size / 1024)
        << " | " << std::setw(11) << (snapshot.memory.external.free_size / 1024)
        << " | " << std::setw(12) << static_cast<size_t>(snapshot.memory.external.largest_free_block / 1024)
        << " | " << std::setw(5) << snapshot.memory.external.used_percent << "%"
        << " |\n";
    oss << row_separator << "\n";
#endif

    // Total
    oss << "| " << std::left << std::setw(17) << "Total"
        << " | " << std::right << std::setw(11) << (snapshot.memory.total_size / 1024)
        << " | " << std::setw(11) << (snapshot.memory.total_free / 1024)
        << " | " << std::setw(12) << static_cast<size_t>(snapshot.memory.total_largest_free_block / 1024)
        << " | " << std::setw(5) << (100 - snapshot.memory.total_free_percent) << "%"
        << " |\n";
    oss << row_separator << "\n";

    // Statistics table
    oss << "========================== Statistics ============================\n";
    const char *stats_header_separator = "+---------------------------+--------------------+";
    const char *stats_row_separator = "+---------------------------+--------------------+";

    oss << stats_header_separator << "\n";
    oss << "| " << std::left << std::setw(25) << "Field"
        << " | " << std::right << std::setw(18) << "Value"
        << " |\n";
    oss << stats_header_separator << "\n";

    // Helper lambda to format value with unit
    auto format_value = [](double value, const char *unit) -> std::string {
        std::ostringstream val_oss;
        val_oss << std::fixed << std::setprecision(3) << value << " " << unit;
        return val_oss.str();
    };

    auto format_int_value = [](size_t value) -> std::string {
        std::ostringstream val_oss;
        val_oss << value;
        return val_oss.str();
    };

    auto format_pct_value = [](size_t value) -> std::string {
        std::ostringstream val_oss;
        val_oss << value << "%";
        return val_oss.str();
    };

    // Sample Count
    oss << "| " << std::left << std::setw(25) << "Sample Count"
        << " | " << std::right << std::setw(18) << format_int_value(snapshot.stats.sample_count)
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min Internal Free
    oss << "| " << std::left << std::setw(25) << "Min Inter Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_internal_free / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min Internal Free Pct
    oss << "| " << std::left << std::setw(25) << "Min Inter Free Pct"
        << " | " << std::right << std::setw(18) << format_pct_value(snapshot.stats.min_internal_free_percent)
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min Internal Largest Free
    oss << "| " << std::left << std::setw(25) << "Min Inter Largest Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_internal_largest_free_block / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";

#if CONFIG_SPIRAM
    // Min External Free
    oss << "| " << std::left << std::setw(25) << "Min Exter Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_external_free / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min External Free Pct
    oss << "| " << std::left << std::setw(25) << "Min Exter Free Pct"
        << " | " << std::right << std::setw(18) << format_pct_value(snapshot.stats.min_external_free_percent)
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min External Largest Free
    oss << "| " << std::left << std::setw(25) << "Min Exter Largest Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_external_largest_free_block / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";
#endif

    // Min Total Free
    oss << "| " << std::left << std::setw(25) << "Min Total Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_total_free / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min Total Free Pct
    oss << "| " << std::left << std::setw(25) << "Min Total Free Pct"
        << " | " << std::right << std::setw(18) << format_pct_value(snapshot.stats.min_total_free_percent)
        << " |\n";
    oss << stats_row_separator << "\n";

    // Min Total Largest Free
    oss << "| " << std::left << std::setw(25) << "Min Total Largest Free"
        << " | " << std::right << std::setw(18) << format_value(snapshot.stats.min_total_largest_free_block / 1024, "KB")
        << " |\n";
    oss << stats_row_separator << "\n";

    oss << "==================================================================\n";

    BROOKESIA_LOGI("%1%", oss.str());
}

void MemoryProfiler::check_thresholds_and_trigger_callbacks(const ProfileSnapshot &snapshot) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    for (const auto &listener : threshold_listeners_) {
        if (!check_threshold(snapshot, listener.type, listener.threshold_value)) {
            continue;
        }
        listener.signal(snapshot);
    }
}

void MemoryProfiler::sample_memory(MemoryInfo &mem_info)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // Sample internal SRAM
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    mem_info.internal = HeapInfo(internal_total, internal_free, internal_largest);

    // Sample external PSRAM
    size_t external_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t external_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t external_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    mem_info.external = HeapInfo(external_total, external_free, external_largest);

    mem_info.total_size = internal_total + external_total;
    mem_info.total_free = internal_free + external_free;
    mem_info.total_free_percent = mem_info.total_free * 100 / mem_info.total_size;
    mem_info.total_largest_free_block = std::max(internal_largest, external_largest);
}

bool MemoryProfiler::check_threshold(const ProfileSnapshot &snapshot, ThresholdType type, uint32_t threshold_value)
{
    auto &stats = snapshot.stats;
    switch (type) {
    case ThresholdType::TotalFree:
        return stats.min_total_free <= threshold_value;
    case ThresholdType::InternalFree:
        return stats.min_internal_free <= threshold_value;
    case ThresholdType::ExternalFree:
        return stats.min_external_free <= threshold_value;
    case ThresholdType::TotalFreePercent:
        return stats.min_total_free_percent <= threshold_value;
    case ThresholdType::InternalFreePercent:
        return stats.min_internal_free_percent <= threshold_value;
    case ThresholdType::ExternalFreePercent:
        return stats.min_external_free_percent <= threshold_value;
    case ThresholdType::TotalLargestFreeBlock:
        return stats.min_total_largest_free_block <= threshold_value;
    case ThresholdType::InternalLargestFreeBlock:
        return stats.min_internal_largest_free_block <= threshold_value;
    case ThresholdType::ExternalLargestFreeBlock:
        return stats.min_external_largest_free_block <= threshold_value;
    default:
        return false;
    }
}

} // namespace esp_brookesia::lib_utils
