/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "esp_heap_caps.h"
#include "soc/soc_memory_layout.h"
#include "brookesia/lib_utils/macro_configs.h"
#if !BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/thread_profiler.hpp"

namespace esp_brookesia::lib_utils {

constexpr uint32_t THREAD_PROFILER_STOP_TIMEOUT_MS = 100;

ThreadProfiler::~ThreadProfiler()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_profiling()) {
        stop_profiling();
    }
}

bool ThreadProfiler::configure_profiling(const ProfilingConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    boost::lock_guard lock(mutex_);

    config_ = config;

    return true;
}

ThreadProfiler::ProfilingConfig ThreadProfiler::get_profiling_config() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(mutex_);
    return config_;
}

bool ThreadProfiler::start_profiling(
    std::shared_ptr<TaskScheduler> scheduler, uint32_t sampling_duration_ms, uint32_t profiling_interval_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if !BROOKESIA_UTILS_THREAD_PROFILER_AVAILABLE
    BROOKESIA_LOGE("Thread profiler is not available");
#   if defined(ESP_PLATFORM)
    BROOKESIA_LOGE("Please enable `BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_FREERTOS_CONFIG` in menuconfig");
#   endif
    return false;
#endif

    if (is_profiling()) {
        BROOKESIA_LOGD("Already profiling");
        return true;
    }

    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not valid");
    BROOKESIA_CHECK_FALSE_RETURN(scheduler->is_running(), false, "Scheduler is not running");

    boost::unique_lock lock(mutex_);

    lib_utils::FunctionGuard stop_guard([this, &lock]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        lock.unlock();
        stop_profiling();
    });

    // Set task_scheduler_ before creating the profiling task to ensure it's available
    task_scheduler_ = scheduler;

    if (sampling_duration_ms == 0) {
        sampling_duration_ms = config_.sampling_duration_ms;
    }
    if (profiling_interval_ms == 0) {
        profiling_interval_ms = config_.profiling_interval_ms;
    }
    BROOKESIA_CHECK_FALSE_RETURN(
        sampling_duration_ms < profiling_interval_ms, false,
        "Sampling duration(%1%) must be less than profiling interval(%2%)", sampling_duration_ms, profiling_interval_ms
    );

    // Schedule periodic profiling task
    auto profiling_task = [this, sampling_duration_ms]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        std::shared_ptr<SampleResult> start_result;
        {
            boost::lock_guard lock(mutex_);
            // Take first sample
            start_result = sample_tasks();
            BROOKESIA_CHECK_NULL_RETURN(start_result, false, "Failed to sample tasks");
        }

        // SEXIT delayed task to take second sample after sampling_duration_ms
        // Note: start_result is captured by value (shared_ptr), so it's safe to use in delayed task
        auto sampling_task = [this, start_result]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            // Check if profiling is still active
            if (!is_profiling()) {
                return;
            }

            boost::lock_guard lock(mutex_);

            // Take second sample
            auto end_result = sample_tasks();
            BROOKESIA_CHECK_NULL_EXIT(end_result, "Failed to sample tasks for second sample");

            // Calculate snapshot from the two samples
            auto snapshot = take_snapshot(*start_result, *end_result);
            BROOKESIA_CHECK_NULL_EXIT(snapshot, "Failed to take snapshot");

            sort_tasks(snapshot->tasks, config_.primary_sort, config_.secondary_sort);

            latest_snapshot_ = snapshot;
            prev_result_ = end_result;

            // Check and trigger thresholds
            check_thresholds_and_trigger_callbacks(*snapshot);

            // Trigger profiling signal
            profiling_signal_(*snapshot);

            // Auto logging
            if (config_.enable_auto_logging) {
                print_snapshot(*snapshot, config_.primary_sort, config_.secondary_sort);
            }
        };
        BROOKESIA_CHECK_FALSE_RETURN(
            task_scheduler_->post_delayed(sampling_task, sampling_duration_ms, &sampling_task_id_), false,
            "Failed to schedule delayed sampling task"
        );

        return true;
    };
    BROOKESIA_CHECK_FALSE_RETURN(
        scheduler->post_periodic(profiling_task, profiling_interval_ms, &profiling_task_id_), false, "Failed to schedule profiling task"
    );
    stop_guard.release();

    BROOKESIA_LOGI(
        "Started profiling with config:\n%1%",
        BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config_, DESCRIBE_FORMAT_VERBOSE)
    );

    return true;
}

void ThreadProfiler::stop_profiling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_profiling()) {
        BROOKESIA_LOGD("Not profiling");
        return;
    }

    boost::lock_guard lock(mutex_);

    task_scheduler_->cancel(sampling_task_id_);
    task_scheduler_->cancel(profiling_task_id_);
    BROOKESIA_CHECK_FALSE_EXECUTE(task_scheduler_->wait(sampling_task_id_, THREAD_PROFILER_STOP_TIMEOUT_MS), {}, {
        BROOKESIA_LOGE("Wait for sampling task timeout after %1% ms", THREAD_PROFILER_STOP_TIMEOUT_MS);
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(task_scheduler_->wait(profiling_task_id_, THREAD_PROFILER_STOP_TIMEOUT_MS), {}, {
        BROOKESIA_LOGE("Wait for profiling task timeout after %1% ms", THREAD_PROFILER_STOP_TIMEOUT_MS);
    });
    task_scheduler_.reset();

    BROOKESIA_LOGI("Stopped profiling");
}

void ThreadProfiler::reset_profiling()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(mutex_);

    // Clear data
    latest_snapshot_.reset();
    prev_result_.reset();

    // Disconnect signals
    profiling_signal_.disconnect_all_slots();
    threshold_signals_.clear();

    // Clear threshold listeners
    threshold_listeners_.clear();

    BROOKESIA_LOGD("Reset profiling data");
}

ThreadProfiler::SignalConnection ThreadProfiler::connect_profiling_signal(ProfilingSignalSlot callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(mutex_);
    return profiling_signal_.connect(callback);
}

ThreadProfiler::SignalConnection ThreadProfiler::connect_threshold_signal(
    ThresholdType type, uint32_t threshold_value, ThresholdSignalSlot callback
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(mutex_);

    // Store threshold listener configuration
    threshold_listeners_.push_back({type, threshold_value});

    // Connect callback to the appropriate signal
    return threshold_signals_[type].connect(callback);
}

std::shared_ptr<ThreadProfiler::SampleResult> ThreadProfiler::sample_tasks()
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::shared_ptr<SampleResult> result;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        result = std::make_shared<SampleResult>(), nullptr, "Failed to create sample result"
    );

    // Allocate array with extra space
    UBaseType_t array_size = uxTaskGetNumberOfTasks();
    result->task_status.resize(array_size);
    result->timestamp = std::chrono::system_clock::now();

    // Get task system state
    uxTaskGetSystemState(result->task_status.data(), array_size, &result->runtime);

    return result;
}

std::shared_ptr<ThreadProfiler::ProfileSnapshot> ThreadProfiler::take_snapshot(
    const SampleResult &start_result, const SampleResult &end_result)
{
    BROOKESIA_LOG_TRACE_GUARD();

#if !BROOKESIA_UTILS_THREAD_PROFILER_AVAILABLE
    BROOKESIA_LOGE("Thread profiler is not available");
#   if defined(ESP_PLATFORM)
    BROOKESIA_LOGE("Please enable `BROOKESIA_UTILS_THREAD_PROFILER_ENABLE_FREERTOS_CONFIG` in menuconfig");
#   endif
    return nullptr;
#else
    std::shared_ptr<ProfileSnapshot> snapshot;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        snapshot = std::make_shared<ProfileSnapshot>(), nullptr, "Failed to create snapshot"
    );

    // Calculate total elapsed time
    uint32_t total_elapsed_time = end_result.runtime - start_result.runtime;
    BROOKESIA_CHECK_FALSE_RETURN(
        total_elapsed_time > 0, nullptr, "Total elapsed time is zero. Try increasing sampling_duration_ms"
    );

    snapshot->timestamp = std::chrono::system_clock::now();
    snapshot->total_runtime = end_result.runtime;
    snapshot->stats.sample_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_result.timestamp - start_result.timestamp).count();

    std::vector<TaskStatus_t> start_copy = start_result.task_status;
    std::vector<TaskStatus_t> end_copy = end_result.task_status;

    // Process matched tasks (exist in both sample results)
    for (size_t i = 0; i < start_copy.size(); i++) {
        if (start_copy[i].xHandle == nullptr) {
            continue; // Already processed
        }

        bool found = false;
        for (size_t j = 0; j < end_copy.size(); j++) {
            if (end_copy[j].xHandle == nullptr) {
                continue;
            }

            if (start_copy[i].xHandle == end_copy[j].xHandle) {
                // Calculate CPU usage for this task
                uint32_t task_elapsed_time = end_copy[j].ulRunTimeCounter - start_copy[i].ulRunTimeCounter;
                uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);

                TaskInfo info;
                info.name = start_copy[i].pcTaskName;
                info.handle = start_copy[i].xHandle;
                info.state = convert_task_state(start_copy[i].eCurrentState);
                info.priority = start_copy[i].uxCurrentPriority;
                // Convert tskNO_AFFINITY to -1 for better display
                info.core_id = (start_copy[i].xCoreID == tskNO_AFFINITY) ? -1 : start_copy[i].xCoreID;
                info.stack_high_water_mark = start_copy[i].usStackHighWaterMark;
                info.is_stack_external = !esp_ptr_internal(pxTaskGetStackStart(start_copy[i].xHandle));
                info.runtime_counter = end_copy[j].ulRunTimeCounter;
                info.elapsed_time = task_elapsed_time;
                info.cpu_percent = percentage_time;
                info.status = TaskStatus::Normal;

                snapshot->tasks.push_back(info);

                // Update statistics
                switch (info.state) {
                case TaskState::Running:
                    snapshot->stats.running_tasks++;
                    break;
                case TaskState::Blocked:
                    snapshot->stats.blocked_tasks++;
                    break;
                case TaskState::Suspended:
                    snapshot->stats.suspended_tasks++;
                    break;
                default:
                    break;
                }

                // Mark as processed
                start_copy[i].xHandle = nullptr;
                end_copy[j].xHandle = nullptr;
                found = true;
                break;
            }
        }

        // Handle deleted tasks (exist in start but not in end)
        if (!found && start_copy[i].xHandle != nullptr) {
            TaskInfo info;
            info.name = start_copy[i].pcTaskName;
            info.handle = start_copy[i].xHandle;
            info.state = convert_task_state(start_copy[i].eCurrentState);
            info.priority = start_copy[i].uxCurrentPriority;
            // Convert tskNO_AFFINITY to -1 for better display
            info.core_id = (start_copy[i].xCoreID == tskNO_AFFINITY) ? -1 : start_copy[i].xCoreID;
            info.stack_high_water_mark = start_copy[i].usStackHighWaterMark;
            info.elapsed_time = 0;
            info.cpu_percent = 0;
            info.status = TaskStatus::Deleted;

            snapshot->tasks.push_back(info);
        }
    }

    // Handle newly created tasks (exist in end but not in start)
    for (size_t i = 0; i < end_copy.size(); i++) {
        if (end_copy[i].xHandle != nullptr) {
            TaskInfo info;
            info.name = end_copy[i].pcTaskName;
            info.handle = end_copy[i].xHandle;
            info.state = convert_task_state(end_copy[i].eCurrentState);
            info.priority = end_copy[i].uxCurrentPriority;
            // Convert tskNO_AFFINITY to -1 for better display
            info.core_id = (end_copy[i].xCoreID == tskNO_AFFINITY) ? -1 : end_copy[i].xCoreID;
            info.stack_high_water_mark = end_copy[i].usStackHighWaterMark;
            info.is_stack_external = !esp_ptr_internal(pxTaskGetStackStart(end_copy[i].xHandle));
            info.elapsed_time = 0;
            info.cpu_percent = 0;
            info.status = TaskStatus::Created;

            snapshot->tasks.push_back(info);
        }
    }

    // Calculate total CPU percentage
    for (const auto &task : snapshot->tasks) {
        snapshot->stats.total_cpu_percent += task.cpu_percent;
    }
    snapshot->stats.total_tasks = snapshot->tasks.size();

    return snapshot;
#endif
}

void ThreadProfiler::sort_tasks(std::vector<TaskInfo> &tasks, PrimarySortBy primary_sort, SecondarySortBy secondary_sort)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // Helper lambda for secondary sort comparison
    auto secondary_compare = [&](const TaskInfo & a, const TaskInfo & b) -> bool {
        switch (secondary_sort)
        {
        case SecondarySortBy::CpuPercent:
            return a.cpu_percent > b.cpu_percent;  // Descending

        case SecondarySortBy::Priority:
            return a.priority > b.priority;  // Descending

        case SecondarySortBy::StackUsage:
            return a.stack_high_water_mark < b.stack_high_water_mark;  // Ascending (lower = more used)

        case SecondarySortBy::Name:
            return a.name < b.name;  // Alphabetically

        default:
            return false;
        }
    };

    // Two-level sorting
    std::stable_sort(tasks.begin(), tasks.end(), [&](const TaskInfo & a, const TaskInfo & b) {
        // Primary sort (if enabled)
        if (primary_sort != PrimarySortBy::None) {
            switch (primary_sort) {
            case PrimarySortBy::CoreId:
                if (a.core_id != b.core_id) {
                    return a.core_id < b.core_id;
                }
                break;

            default:
                break;
            }
        }

        // Secondary sort (always applied)
        return secondary_compare(a, b);
    });
}

void ThreadProfiler::print_snapshot(
    const ProfileSnapshot &snapshot, PrimarySortBy primary_sort, SecondarySortBy secondary_sort
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::ostringstream oss;

    // Header
    oss << "\n==================== Thread Profiler Snapshot ====================\n";
    oss << "Tasks: " << snapshot.stats.total_tasks
        << " (Running: " << snapshot.stats.running_tasks
        << ", Blocked: " << snapshot.stats.blocked_tasks
        << ", Suspended: " << snapshot.stats.suspended_tasks << ")\n"
        << "Total CPU: " << snapshot.stats.total_cpu_percent << "%"
        << ", Sampling Duration: " << snapshot.stats.sample_duration_ms << "ms\n";

    // Define column widths (width of dashes in separator, not including +)
    // Format: | content |, so content width = column_width - 2 (for "| " and " |")
    const int name_width = 20;      // "Name" (4) + padding = 20 dashes
    const int coreid_width = 8;     // "CoreId" (6) + padding = 8 dashes
    const int cpu_width = 7;        // "CPU%" (4) + padding = 7 dashes
    const int priority_width = 10;  // "Priority" (8) + padding = 10 dashes
    const int hwm_width = 7;        // "HWM" (3) + padding = 7 dashes
    const int stack_width = 7;      // "Stack" (5) + padding = 7 dashes
    const int runtime_width = 10;   // "Run Time" (8) + padding = 10 dashes
    const int state_width = 11;     // "Suspended" (9) + padding = 11 dashes

    // Helper function to build separator line
    auto build_separator = [&]() -> std::string {
        std::ostringstream sep;
        sep << "+";
        sep << std::string(name_width, '-') << "+";

        // Primary sort column (if CoreId)
        if (primary_sort == PrimarySortBy::CoreId)
        {
            sep << std::string(coreid_width, '-') << "+";
        }

        // Secondary sort column
        switch (secondary_sort)
        {
        case SecondarySortBy::CpuPercent:
            sep << std::string(cpu_width, '-') << "+";
            break;
        case SecondarySortBy::Priority:
            sep << std::string(priority_width, '-') << "+";
            break;
        case SecondarySortBy::StackUsage:
            sep << std::string(hwm_width, '-') << "+";
            break;
        case SecondarySortBy::Name:
            break;
        }

        // Other columns (not used as primary/secondary sort)
        if (primary_sort != PrimarySortBy::CoreId)
        {
            sep << std::string(coreid_width, '-') << "+";
        }
        if (secondary_sort != SecondarySortBy::CpuPercent)
        {
            sep << std::string(cpu_width, '-') << "+";
        }
        if (secondary_sort != SecondarySortBy::Priority)
        {
            sep << std::string(priority_width, '-') << "+";
        }
        if (secondary_sort != SecondarySortBy::StackUsage)
        {
            sep << std::string(hwm_width, '-') << "+";
        }

        // Fixed columns
        sep << std::string(stack_width, '-') << "+";
        sep << std::string(runtime_width, '-') << "+";
        sep << std::string(state_width, '-') << "+";
        return sep.str();
    };

    // Helper function to print a column value
    auto print_column = [&](const std::string & value, int width, bool right_align = false) {
        if (right_align) {
            oss << " | " << std::right << std::setw(width - 2) << value;
        } else {
            oss << " | " << std::left << std::setw(width - 2) << value;
        }
    };

    // Helper function to print primary sort column
    auto print_primary_sort_column = [&](const std::string & value) {
        if (primary_sort == PrimarySortBy::CoreId) {
            print_column(value, coreid_width, true);
        }
    };

    // Helper function to print secondary sort column
    auto print_secondary_sort_column = [&](const std::string & value) {
        switch (secondary_sort) {
        case SecondarySortBy::CpuPercent:
            print_column(value, cpu_width, true);
            break;
        case SecondarySortBy::Priority:
            print_column(value, priority_width, true);
            break;
        case SecondarySortBy::StackUsage:
            print_column(value, hwm_width, true);
            break;
        case SecondarySortBy::Name:
            break;
        }
    };

    // Helper function to print other columns (not used as primary/secondary sort)
    auto print_other_columns = [&](const TaskInfo & task, bool is_special_status) {
        if (primary_sort != PrimarySortBy::CoreId) {
            print_column(is_special_status ? "-" : std::to_string(task.core_id), coreid_width, true);
        }
        if (secondary_sort != SecondarySortBy::CpuPercent) {
            print_column(is_special_status ? "-" : (std::to_string(task.cpu_percent) + "%"), cpu_width, true);
        }
        if (secondary_sort != SecondarySortBy::Priority) {
            print_column(is_special_status ? "-" : std::to_string(task.priority), priority_width, true);
        }
        if (secondary_sort != SecondarySortBy::StackUsage) {
            print_column(is_special_status ? "-" : std::to_string(task.stack_high_water_mark), hwm_width, true);
        }
    };

    // Helper function to print a task row
    auto print_task_row = [&](const TaskInfo & task, bool is_special_status) {
        oss << "| " << std::left << std::setw(name_width - 2) << task.name;

        // Primary sort column
        if (is_special_status) {
            print_primary_sort_column("-");
        } else {
            if (primary_sort == PrimarySortBy::CoreId) {
                print_column(std::to_string(task.core_id), coreid_width, true);
            }
        }

        // Secondary sort column
        if (is_special_status) {
            print_secondary_sort_column("-");
        } else {
            switch (secondary_sort) {
            case SecondarySortBy::CpuPercent:
                print_column(std::to_string(task.cpu_percent) + "%", cpu_width, true);
                break;
            case SecondarySortBy::Priority:
                print_column(std::to_string(task.priority), priority_width, true);
                break;
            case SecondarySortBy::StackUsage:
                print_column(std::to_string(task.stack_high_water_mark), hwm_width, true);
                break;
            case SecondarySortBy::Name:
                break;
            }
        }

        // Other columns
        print_other_columns(task, is_special_status);

        // Fixed columns
        if (is_special_status) {
            print_column("-", stack_width);
            print_column("-", runtime_width, true);
            const char *status_str = (task.status == TaskStatus::Deleted) ? "Deleted" : "Created";
            print_column(status_str, state_width);
        } else {
            const char *stack_location = task.is_stack_external ? "Extr" : "Intr";
            print_column(stack_location, stack_width);
            print_column(std::to_string(task.elapsed_time), runtime_width, true);
            print_column(get_state_string(task.state), state_width);
        }

        oss << " |\n";
    };

    std::string separator = build_separator();

    // Print header
    oss << separator << "\n";
    oss << "| " << std::left << std::setw(name_width - 2) << "Name";

    // Primary sort column header
    if (primary_sort == PrimarySortBy::CoreId) {
        print_column("CoreId", coreid_width, true);
    }

    // Secondary sort column header
    switch (secondary_sort) {
    case SecondarySortBy::CpuPercent:
        print_column("CPU%", cpu_width, true);
        break;
    case SecondarySortBy::Priority:
        print_column("Priority", priority_width, true);
        break;
    case SecondarySortBy::StackUsage:
        print_column("HWM", hwm_width, true);
        break;
    case SecondarySortBy::Name:
        break;
    }

    // Other column headers
    if (primary_sort != PrimarySortBy::CoreId) {
        print_column("CoreId", coreid_width, true);
    }
    if (secondary_sort != SecondarySortBy::CpuPercent) {
        print_column("CPU%", cpu_width, true);
    }
    if (secondary_sort != SecondarySortBy::Priority) {
        print_column("Priority", priority_width, true);
    }
    if (secondary_sort != SecondarySortBy::StackUsage) {
        print_column("HWM", hwm_width, true);
    }

    // Fixed column headers
    print_column("Stack", stack_width);
    print_column("Run Time", runtime_width, true);
    print_column("State", state_width);

    oss << " |\n";
    oss << separator << "\n";

    // Build data rows
    for (const auto &task : snapshot.tasks) {
        bool is_special_status = (task.status == TaskStatus::Deleted || task.status == TaskStatus::Created);
        print_task_row(task, is_special_status);
        oss << separator << "\n";
    }

    oss << "==================================================================\n";

    BROOKESIA_LOGI("%1%", oss.str());
}

bool ThreadProfiler::get_task_by_name(const ProfileSnapshot &snapshot, const std::string &name, TaskInfo &task)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto it = std::find_if(snapshot.tasks.begin(), snapshot.tasks.end(), [&name](const TaskInfo & task) {
        return task.name == name;
    });
    if (it != snapshot.tasks.end()) {
        task = *it;
        return true;
    }

    return false;
}

std::vector<ThreadProfiler::TaskInfo> ThreadProfiler::get_tasks_above_threshold(
    const ProfileSnapshot &snapshot, ThresholdType type, uint32_t threshold_value
)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto &tasks = snapshot.tasks;
    std::vector<TaskInfo> result;

    switch (type) {
    case ThresholdType::CpuPercent:
        // Return tasks with CPU >= threshold_value
        std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(result),
        [threshold_value](const TaskInfo & task) {
            return task.cpu_percent >= threshold_value;
        }
                    );
        break;

    case ThresholdType::Priority:
        // Return tasks with priority >= threshold_value
        std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(result),
        [threshold_value](const TaskInfo & task) {
            return task.priority >= threshold_value;
        }
                    );
        break;

    case ThresholdType::StackUsage:
        // Return tasks with stack HWM <= threshold_value (lower = more critical)
        std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(result),
        [threshold_value](const TaskInfo & task) {
            return task.stack_high_water_mark <= threshold_value;
        }
                    );
        break;

    default:
        break;
    }

    return result;

}

void ThreadProfiler::check_thresholds_and_trigger_callbacks(const ProfileSnapshot &snapshot)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Check each registered threshold
    for (const auto &listener : threshold_listeners_) {
        auto matched_tasks = get_tasks_above_threshold(snapshot, listener.type, listener.threshold_value);
        if (!matched_tasks.empty()) {
            threshold_signals_[listener.type](matched_tasks);
        }
    }
}

ThreadProfiler::TaskState ThreadProfiler::convert_task_state(eTaskState state)
{
    switch (state) {
    case eRunning:
        return TaskState::Running;
    case eReady:
        return TaskState::Ready;
    case eBlocked:
        return TaskState::Blocked;
    case eSuspended:
        return TaskState::Suspended;
    case eDeleted:
        return TaskState::Deleted;
    default:
        return TaskState::Invalid;
    }
}

const char *ThreadProfiler::get_state_string(TaskState state)
{
    switch (state) {
    case TaskState::Running:
        return "Running";
    case TaskState::Ready:
        return "Ready";
    case TaskState::Blocked:
        return "Blocked";
    case TaskState::Suspended:
        return "Suspended";
    case TaskState::Deleted:
        return "Deleted";
    default:
        return "Invalid";
    }
}

} // namespace esp_brookesia::lib_utils
