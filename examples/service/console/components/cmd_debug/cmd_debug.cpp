/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "brookesia/lib_utils/memory_profiler.hpp"
#include "brookesia/lib_utils/thread_profiler.hpp"
#include "brookesia/lib_utils/time_profiler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cmd_debug.hpp"

using namespace esp_brookesia::lib_utils;

static const char *TAG = "debug_cmd";

// ============================================================================
// Memory profiler command
// ============================================================================

static struct {
    struct arg_end *end;
} mem_args;

static int do_debug_mem_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mem_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mem_args.end, argv[0]);
        return 1;
    }

    printf("\n=== Memory Profiler ===\n\n");

    // Take a snapshot
    auto snapshot = MemoryProfiler::take_snapshot();
    if (!snapshot) {
        printf("Error: Failed to take memory snapshot\n\n");
        return 1;
    }

    // Print the snapshot
    MemoryProfiler::print_snapshot(*snapshot);

    printf("\n");
    return 0;
}

// ============================================================================
// Thread profiler command
// ============================================================================

static struct {
    struct arg_str *sort_primary;
    struct arg_str *sort_secondary;
    struct arg_int *duration;
    struct arg_end *end;
} thread_args;

static int do_debug_thread_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&thread_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, thread_args.end, argv[0]);
        return 1;
    }

    printf("\n=== Thread Profiler ===\n\n");

    // Parse sort options
    ThreadProfiler::PrimarySortBy primary_sort = ThreadProfiler::PrimarySortBy::CoreId;
    ThreadProfiler::SecondarySortBy secondary_sort = ThreadProfiler::SecondarySortBy::CpuPercent;

    if (thread_args.sort_primary->count > 0) {
        const char *primary_str = thread_args.sort_primary->sval[0];
        if (strcmp(primary_str, "none") == 0 || strcmp(primary_str, "None") == 0) {
            primary_sort = ThreadProfiler::PrimarySortBy::None;
        } else if (strcmp(primary_str, "core") == 0 || strcmp(primary_str, "CoreId") == 0) {
            primary_sort = ThreadProfiler::PrimarySortBy::CoreId;
        } else {
            printf("Warning: Unknown primary sort '%s', using default (CoreId)\n", primary_str);
        }
    }

    if (thread_args.sort_secondary->count > 0) {
        const char *secondary_str = thread_args.sort_secondary->sval[0];
        if (strcmp(secondary_str, "cpu") == 0 || strcmp(secondary_str, "CpuPercent") == 0) {
            printf("Using secondary sort: CpuPercent\n");
            secondary_sort = ThreadProfiler::SecondarySortBy::CpuPercent;
        } else if (strcmp(secondary_str, "priority") == 0 || strcmp(secondary_str, "Priority") == 0) {
            printf("Using secondary sort: Priority\n");
            secondary_sort = ThreadProfiler::SecondarySortBy::Priority;
        } else if (strcmp(secondary_str, "stack") == 0 || strcmp(secondary_str, "StackUsage") == 0) {
            printf("Using secondary sort: StackUsage\n");
            secondary_sort = ThreadProfiler::SecondarySortBy::StackUsage;
        } else if (strcmp(secondary_str, "name") == 0 || strcmp(secondary_str, "Name") == 0) {
            printf("Using secondary sort: Name\n");
            secondary_sort = ThreadProfiler::SecondarySortBy::Name;
        } else {
            printf("Warning: Unknown secondary sort '%s', using default (CpuPercent)\n", secondary_str);
        }
    }

    // Parse sampling duration (default: 1000ms)
    uint32_t duration_ms = 1000;
    if (thread_args.duration->count > 0) {
        duration_ms = static_cast<uint32_t>(thread_args.duration->ival[0]);
        if (duration_ms == 0) {
            printf("Warning: Duration must be > 0, using default (1000ms)\n");
            duration_ms = 1000;
        }
    }

    // Sample tasks twice with a delay to calculate CPU usage
    printf("Sampling tasks (duration: %lu ms)...\n", duration_ms);
    auto start_result = ThreadProfiler::sample_tasks();
    if (!start_result) {
        printf("Error: Failed to sample tasks (start)\n\n");
        return 1;
    }

    // Wait for the specified duration to get meaningful CPU usage data
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    auto end_result = ThreadProfiler::sample_tasks();
    if (!end_result) {
        printf("Error: Failed to sample tasks (end)\n\n");
        return 1;
    }

    // Take snapshot
    auto snapshot = ThreadProfiler::take_snapshot(*start_result, *end_result);
    if (!snapshot) {
        printf("Error: Failed to take thread snapshot\n\n");
        return 1;
    }

    // Sort tasks according to configuration
    ThreadProfiler::sort_tasks(snapshot->tasks, primary_sort, secondary_sort);

    // Print the snapshot
    ThreadProfiler::print_snapshot(*snapshot, primary_sort, secondary_sort);

    printf("\n");
    return 0;
}

// ============================================================================
// Time profiler commands
// ============================================================================

static struct {
    struct arg_end *end;
} time_report_args;

static int do_debug_time_report_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&time_report_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, time_report_args.end, argv[0]);
        return 1;
    }

    printf("\n=== Time Profiler Report ===\n\n");

    // Generate and output profiling report
    TimeProfiler::get_instance().report();

    printf("\n");
    return 0;
}

static struct {
    struct arg_end *end;
} time_clear_args;

static int do_debug_time_clear_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&time_clear_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, time_clear_args.end, argv[0]);
        return 1;
    }

    printf("\n=== Time Profiler Clear ===\n\n");

    // Clear all profiling data
    TimeProfiler::get_instance().clear();

    printf("All time profiling data has been cleared.\n\n");
    return 0;
}

// ============================================================================
// Command registration
// ============================================================================

void register_debug_commands(void)
{
    // Command: debug_mem
    mem_args.end = arg_end(1);

    const esp_console_cmd_t mem_cmd = {
        .command = "debug_mem",
        .help = "Print memory profiler information",
        .hint = NULL,
        .func = &do_debug_mem_cmd,
        .argtable = &mem_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&mem_cmd));

    // Command: debug_thread
    thread_args.sort_primary = arg_str0("p", "primary", "<none|core>", "Primary sort: none or core (default: core)");
    thread_args.sort_secondary = arg_str0("s", "secondary", "<cpu|priority|stack|name>", "Secondary sort: cpu, priority, stack, or name (default: cpu)");
    thread_args.duration = arg_int0("d", "duration", "<ms>", "Sampling duration in milliseconds (default: 1000)");
    thread_args.end = arg_end(4);

    const esp_console_cmd_t thread_cmd = {
        .command = "debug_thread",
        .help = "Print thread profiler information",
        .hint = NULL,
        .func = &do_debug_thread_cmd,
        .argtable = &thread_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&thread_cmd));

    // Command: debug_time_report
    time_report_args.end = arg_end(1);

    const esp_console_cmd_t time_report_cmd = {
        .command = "debug_time_report",
        .help = "Print time profiler report",
        .hint = NULL,
        .func = &do_debug_time_report_cmd,
        .argtable = &time_report_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&time_report_cmd));

    // Command: debug_time_clear
    time_clear_args.end = arg_end(1);

    const esp_console_cmd_t time_clear_cmd = {
        .command = "debug_time_clear",
        .help = "Clear all time profiler data",
        .hint = NULL,
        .func = &do_debug_time_clear_cmd,
        .argtable = &time_clear_args,
        .func_w_context = NULL,
        .context = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&time_clear_cmd));

    ESP_LOGI(TAG, "Debug commands registered");
}
