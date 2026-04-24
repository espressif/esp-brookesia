/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_vfs_fat.h"
#include "esp_console.h"
#include "console.hpp"
#include "cmd_service.hpp"
#include "cmd_debug.hpp"
#include "private/utils.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

/*
 * We warn if a secondary serial console is enabled. A secondary serial console is always output-only and
 * hence not very useful for interactive console applications. If you encounter this warning, consider disabling
 * the secondary serial console in menuconfig unless you know what you are doing.
 */
#if SOC_USB_SERIAL_JTAG_SUPPORTED && !CONFIG_ESP_CONSOLE_SECONDARY_NONE
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif

#define PROMPT_STR CONFIG_IDF_TARGET

#if CONFIG_CONSOLE_STORE_HISTORY
#   define HISTORY_MOUNT_ROOT "/data"
#   define HISTORY_PATH HISTORY_MOUNT_ROOT "/history.txt"
#   define HISTORY_PARTITION_LABEL "storage"
#endif

using namespace esp_brookesia;

bool Console::start()
{
    return start(Config{});
}

bool Console::start(const Config &config)
{
    if (is_running()) {
        BROOKESIA_LOGW("Console is already running");
        return true;
    }

    config_ = config;

    auto thread_config = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
#if CONFIG_CONSOLE_STORE_HISTORY
    auto mount_history_task = []() {
        static wl_handle_t wl_handle;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 4,
        };
#pragma GCC diagnostic pop
        esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
                            HISTORY_MOUNT_ROOT, HISTORY_PARTITION_LABEL, &mount_config, &wl_handle
                        );
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(err, {}, {
            BROOKESIA_LOGE("Failed to mount FATFS for history");
        });
        BROOKESIA_LOGI("FATFS for history mounted successfully");
    };
    // Since mounting FATFS in `esp_vfs_fat_spiflash_mount_rw_wl()` operates on Flash,
    // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
    if (thread_config.stack_in_ext) {
        BROOKESIA_LOGD("Mounting history in new thread");
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread(mount_history_task).join();
    } else {
        BROOKESIA_LOGD("Mounting history in current thread");
        mount_history_task();
    }
#endif // CONFIG_CONSOLE_STORE_HISTORY

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.task_stack_size = config_.task_stack_size;
    repl_config.task_priority = config_.task_priority;
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = config_.max_cmdline_length;

#if CONFIG_CONSOLE_STORE_HISTORY
    repl_config.history_save_path = HISTORY_PATH;
    BROOKESIA_LOGI("Command history enabled");
#else
    BROOKESIA_LOGI("Command history disabled");
#endif

    // Register commands
    register_commands();

    esp_err_t result = ESP_OK;
    auto create_repl_func = [&]() {
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
        esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
        result = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
        esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
        result = esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl);
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
        esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
        result = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
#else
#   error Unsupported console type
#endif
    };
    // Since creating REPL operates on Flash,
    // a separate thread with its stack located in SRAM needs to be created to prevent a crash.
    if (thread_config.stack_in_ext) {
        BROOKESIA_LOGD("Creating REPL in new thread");
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread(create_repl_func).join();
    } else {
        BROOKESIA_LOGD("Creating REPL in current thread");
        create_repl_func();
    }
    BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to create UART REPL");

    auto start_result = esp_console_start_repl(repl);
    BROOKESIA_CHECK_ESP_ERR_RETURN(start_result, false, "Failed to start REPL");

    // Print welcome banner
    print_banner();

    is_running_.store(true);

    BROOKESIA_LOGI("Console started successfully");

    return true;
}

void Console::print_banner()
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          ESP Brookesia Service Manager Console               ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("┌─ Service Commands ───────────────────────────────────────────┐\n");
    printf("│  svc_list                         List all services          │\n");
    printf("│  svc_funcs <service>              List service functions     │\n");
    printf("│  svc_events <service>             List service events        │\n");
    printf("│  svc_call <srv> <func> [json]     Call service function      │\n");
    printf("│  svc_subscribe <srv> <event>      Subscribe to event         │\n");
    printf("│  svc_unsubscribe <srv> <event>    Unsubscribe from event     │\n");
    printf("│  svc_stop <service>               Stop service binding       │\n");
    printf("└──────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("┌─ Debug Commands ─────────────────────────────────────────────┐\n");
    printf("│  debug_mem                        Memory profiler info       │\n");
    printf("│  debug_thread [-p] [-s] [-d ms]   Thread profiler info       │\n");
    printf("│  debug_time_report                Time profiler report       │\n");
    printf("│  debug_time_clear                 Clear time profiler data   │\n");
    printf("└──────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("┌─ RPC Commands ───────────────────────────────────────────────┐\n");
    printf("│  svc_rpc_server <action>          Manage RPC server          │\n");
    printf("│  svc_rpc_call <host> <srv> ...    Remote function call       │\n");
    printf("│  svc_rpc_subscribe <host> ...     Remote event subscribe     │\n");
    printf("│  svc_rpc_unsubscribe <host> ...   Remote event unsubscribe   │\n");
    printf("└──────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("Type 'help' for detailed command usage.\n");
    printf("\n");
}

void Console::register_commands()
{
    esp_console_register_help_command();
    register_service_commands();
    register_debug_commands();
}
