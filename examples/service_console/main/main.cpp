/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <string>
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "cmd_service.hpp"
#include "cmd_debug.hpp"
#include "brookesia/lib_utils.hpp"
#include "private/utils.hpp"
#if CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
#   include "board.hpp"
#endif
#include "general_services.hpp"
#include "ai_agents.hpp"
#include "audio_service.hpp"
#include "expression.hpp"

#define SPIFFS_PARTITION_LABEL "spiffs_data"

constexpr size_t THREAD_IDLE_CPU_USAGE_THRESHOLD = 2;
constexpr size_t THREAD_STACK_USAGE_THRESHOLD = 128;

constexpr size_t MEM_INTERNAL_LARGEST_FREE_THRESHOLD = 10 * 1024;
constexpr size_t MEM_INTERNAL_FREE_PERCENT_THRESHOLD = 15;
constexpr size_t MEM_EXTERNAL_LARGEST_FREE_THRESHOLD = 1024 * 1024;
constexpr size_t MEM_EXTERNAL_FREE_PERCENT_THRESHOLD = 20;

using namespace esp_brookesia;

/*
 * We warn if a secondary serial console is enabled. A secondary serial console is always output-only and
 * hence not very useful for interactive console applications. If you encounter this warning, consider disabling
 * the secondary serial console in menuconfig unless you know what you are doing.
 */
#if SOC_USB_SERIAL_JTAG_SUPPORTED && !CONFIG_ESP_CONSOLE_SECONDARY_NONE
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif

#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_CONSOLE_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    BROOKESIA_CHECK_ESP_ERR_EXIT(err, "Failed to mount FATFS");
}
#endif // CONFIG_STORE_HISTORY

static void initialize_spiffs(void)
{
    /* Initialize SPIFFS */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            BROOKESIA_LOGE("Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            BROOKESIA_LOGE("Failed to find SPIFFS partition");
        } else {
            BROOKESIA_LOGE("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    } else {
        BROOKESIA_LOGI("SPIFFS mounted successfully");
    }
}

extern "C" void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.task_stack_size = 10 * 1024;
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    /* Initialize system components */
    initialize_spiffs();
#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    BROOKESIA_LOGI("Command history enabled");
#else
    BROOKESIA_LOGI("Command history disabled");
#endif

#if CONFIG_EXAMPLE_ENABLE_BOARD_MANAGER
    board_manager_init();
#endif

    /* Initialize all services */
    general_services_init();
    audio_service_init();
    expression_emote_init();
    // Agent service should be initialized after expression service
    ai_agents_init();

    /* Print welcome banner */
    printf("\n");
    printf("==============================================\n");
    printf("  ESP Brookesia Service Manager Console\n");
    printf("==============================================\n");
    printf("\n");
    printf("Service Commands:\n");
    printf("  svc_list                        - List all registered services\n");
    printf("  svc_funcs <service>            - List all functions in a service\n");
    printf("  svc_events <service>           - List all events in a service\n");
    printf("  svc_call <srv> <func> [params] - Call a service function with JSON parameters\n");
    printf("  svc_stop <service>            - Stop and release a service binding\n");
    printf("  svc_subscribe <srv> <event>   - Subscribe to a service event\n");
    printf("  svc_unsubscribe <srv> <event>  - Unsubscribe from service events\n");
    printf("\n");
    printf("RPC Commands:\n");
    printf("  svc_rpc_server <action> [-p <port>] [-s <services>]\n");
    printf("                              - Manage RPC server: start, stop, connect/disconnect services\n");
    printf("  svc_rpc_call <host> <srv> <func> [params] [-p <port>] [-t <timeout>]\n");
    printf("                              - Call a remote service function via RPC\n");
    printf("  svc_rpc_subscribe <host> <srv> <event> [-p <port>] [-t <timeout>]\n");
    printf("                              - Subscribe to a remote service event via RPC\n");
    printf("  svc_rpc_unsubscribe <host> <srv> <event> [-p <port>] [-t <timeout>]\n");
    printf("                              - Unsubscribe from a remote service event via RPC\n");
    printf("\n");
    printf("Debug Commands:\n");
    printf("  debug_mem                      - Print memory profiler information\n");
    printf("  debug_thread [-p <sort>] [-s <sort>] [-d <ms>]\n");
    printf("                              - Print thread profiler information\n");
    printf("                                -p: Primary sort (none|core, default: core)\n");
    printf("                                -s: Secondary sort (cpu|priority|stack|name, default: cpu)\n");
    printf("                                -d: Sampling duration in ms (default: 1000)\n");
    printf("  debug_time_report              - Print time profiler report\n");
    printf("  debug_time_clear               - Clear all time profiler data\n");
    printf("\n");

    /* Register commands */
    esp_console_register_help_command();

    /* Register service manager commands */
    register_service_commands();

    /* Register debug commands */
    register_debug_commands();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    /* Start a task scheduler for profiling */
    auto scheduler = std::make_shared<lib_utils::TaskScheduler>();
    scheduler->start();

    /* Configure and start thread profiler */
    auto &thread_profiler = lib_utils::ThreadProfiler::get_instance();
    thread_profiler.configure_profiling({
        .enable_auto_logging = false,
    });
    thread_profiler.start_profiling(scheduler);
    // Monitor idle tasks CPU usage
    using thread_profiler_connection_type = lib_utils::ThreadProfiler::SignalConnection;
    static std::vector<thread_profiler_connection_type> thread_profiler_connections;
    {
        auto profiling_slot = [](const lib_utils::ThreadProfiler::ProfileSnapshot & snapshot) {
            std::vector<std::string> idle_task_names = {
                "IDLE0",
#if CONFIG_SOC_CPU_CORES_NUM > 1
                "IDLE1",
#endif
            };
            bool need_print_snapshot = false;
            for (auto &name : idle_task_names) {
                lib_utils::ThreadProfiler::TaskInfo task_info;
                BROOKESIA_CHECK_FALSE_EXIT(
                    lib_utils::ThreadProfiler::get_task_by_name(snapshot, name, task_info),
                    "Failed to get idle task `%1%`", name
                );
                if (task_info.cpu_percent < THREAD_IDLE_CPU_USAGE_THRESHOLD) {
                    need_print_snapshot = true;
                    BROOKESIA_LOGW(
                        "The CPU usage of the idle task `%1%` is less than %2%%:", name, THREAD_IDLE_CPU_USAGE_THRESHOLD
                    );
                }
            }
            if (need_print_snapshot) {
                lib_utils::ThreadProfiler::print_snapshot(snapshot);
            }

        };
        auto connection = thread_profiler.connect_profiling_signal(profiling_slot);
        thread_profiler_connections.push_back(std::move(connection));
    }
    // Monitor tasks with high stack usage
    {
        auto threshold_slot = [](const std::vector<lib_utils::ThreadProfiler::TaskInfo> &tasks) {
            BROOKESIA_LOGW("The following tasks have high stack usage:");
            lib_utils::ThreadProfiler::print_snapshot({.tasks = tasks});
        };
        auto connection = thread_profiler.connect_threshold_signal(
                              lib_utils::ThreadProfiler::ThresholdType::StackUsage, THREAD_STACK_USAGE_THRESHOLD,
                              threshold_slot
                          );
        thread_profiler_connections.push_back(std::move(connection));
    }

    /* Configure and start memory profiler */
    auto &memory_profiler = lib_utils::MemoryProfiler::get_instance();
    memory_profiler.configure_profiling({
        .enable_auto_logging = false,
    });
    memory_profiler.start_profiling(scheduler);
    // Monitor largest free internal memory
    using memory_profiler_connection_type = lib_utils::MemoryProfiler::SignalConnection;
    static std::vector<memory_profiler_connection_type> memory_profiler_connections;
    {
        auto threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGW(
                "Largest free internal memory is too low: %1% KB (total: %2% KB, free: %3% KB)",
                snapshot.memory.internal.largest_free_block / 1024,
                snapshot.memory.internal.total_size / 1024, snapshot.memory.internal.free_size / 1024
            );
        };
        auto connection = memory_profiler.connect_threshold_signal(
                              lib_utils::MemoryProfiler::ThresholdType::InternalLargestFreeBlock,
                              MEM_INTERNAL_LARGEST_FREE_THRESHOLD, threshold_slot
                          );
        memory_profiler_connections.push_back(std::move(connection));
    }
    // Monitor free internal memory
    {
        auto threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGW(
                "Internal free memory is too low: %1%%% (%2% KB)",
                snapshot.memory.internal.free_percent, snapshot.memory.internal.free_size / 1024
            );
        };
        auto connection = memory_profiler.connect_threshold_signal(
                              lib_utils::MemoryProfiler::ThresholdType::InternalFreePercent,
                              MEM_INTERNAL_FREE_PERCENT_THRESHOLD, threshold_slot
                          );
        memory_profiler_connections.push_back(std::move(connection));
    }
#if CONFIG_SPIRAM
    // Monitor largest free external memory
    {
        auto threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGW(
                "Largest free external memory is too low: %1% KB (total: %2% KB, free: %3% KB)",
                snapshot.memory.external.largest_free_block / 1024,
                snapshot.memory.external.total_size / 1024, snapshot.memory.external.free_size / 1024
            );
        };
        auto connection = memory_profiler.connect_threshold_signal(
                              lib_utils::MemoryProfiler::ThresholdType::ExternalLargestFreeBlock,
                              MEM_EXTERNAL_LARGEST_FREE_THRESHOLD, threshold_slot
                          );
        memory_profiler_connections.push_back(std::move(connection));
    }
    // Monitor free external memory
    {
        auto threshold_slot = [](const lib_utils::MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGW(
                "External free memory is too low: %1%%% (%2% KB)",
                snapshot.memory.external.free_percent, snapshot.memory.external.free_size / 1024
            );
        };
        auto connection = memory_profiler.connect_threshold_signal(
                              lib_utils::MemoryProfiler::ThresholdType::ExternalFreePercent,
                              MEM_EXTERNAL_FREE_PERCENT_THRESHOLD, threshold_slot
                          );
        memory_profiler_connections.push_back(std::move(connection));
    }
#endif // CONFIG_SPIRAM
}
