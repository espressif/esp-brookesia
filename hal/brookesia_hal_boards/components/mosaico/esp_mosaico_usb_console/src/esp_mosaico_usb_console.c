/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_private/startup_internal.h"
#include "esp_system.h"
#include "esp_task.h"
#include "esp_timer.h"
#include "hal/wdt_hal.h"
#include "sdkconfig.h"
#include "soc/lp_system_reg.h"
#include "soc/rtc.h"
#include "soc/soc.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "vfs_tinyusb.h"
#include "tinyusb_default_config.h"
#include "esp_mosaico_usb_console.h"

#define USB_INIT_TASK_STACK_SIZE        (4096)
#define USB_INIT_TASK_PRIORITY          (ESP_TASK_MAIN_PRIO + 1)
#define USB_RESTART_TASK_STACK_SIZE     (4096)
#define USB_RESTART_TASK_PRIORITY       (ESP_TASK_MAIN_PRIO + 1)
#define USB_RESTART_DELAY_US            (50 * 1000)
#define USB_SYSTEM_RESET_DELAY_MS       (10)
#define USB_FLUSH_TIMEOUT_MS            (20)
#define USB_REENUMERATION_DELAY_MS      (20)
#define STRINGIFY_VALUE(value)          #value
#define STRINGIFY(value)                STRINGIFY_VALUE(value)
#define UART_CONSOLE_PATH               "/dev/uart/" STRINGIFY(CONFIG_ESP_CONSOLE_UART_NUM)
#define CONSOLE_STREAM_COUNT            (3)

typedef enum {
    USB_RESTART_NONE,
    USB_RESTART_NORMAL,
    USB_RESTART_BOOTLOADER,
} UsbRestartType;

static const char *TAG = "MosaicoUSB";
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static bool initializing;
static bool initialized;
static bool reset_armed;
static esp_timer_handle_t restart_timer;
static TaskHandle_t restart_task_handle;
static UsbRestartType pending_restart;
static esp_err_t terminal_error;

typedef struct {
    FILE *shared[CONSOLE_STREAM_COUNT];
    FILE *uart_reserve[CONSOLE_STREAM_COUNT];
    bool switched[CONSOLE_STREAM_COUNT];
    bool invalid[CONSOLE_STREAM_COUNT];
    bool vfs_registered;
} console_streams_t;

static console_streams_t console_streams;

static void auto_init_task(void *arg);
static esp_err_t configure_console_stream_buffering(FILE *stream, size_t index);
static esp_err_t prepare_console_streams(void);
static esp_err_t restore_console_streams(void);
static void replace_failed_stream(size_t index);
static esp_err_t cleanup_after_init_failure(bool driver_installed, bool cdc_initialized);
static void cleanup_before_restart(void);
static void device_event_callback(tinyusb_event_t *event, void *arg);
static void line_state_changed_callback(int interface, cdcacm_event_t *event);
static void restart_task(void *arg);
static void restart_timer_callback(void *arg);
static void system_watchdog_reset(void) __attribute__((noreturn));

esp_err_t esp_mosaico_usb_console_init(void)
{
    bool driver_installed = false;
    bool cdc_initialized = false;
    esp_err_t ret;

    portENTER_CRITICAL(&state_lock);
    if (terminal_error != ESP_OK) {
        const esp_err_t error = terminal_error;
        portEXIT_CRITICAL(&state_lock);
        return error;
    }
    if (initialized) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_OK;
    }
    if (initializing) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    initializing = true;
    portEXIT_CRITICAL(&state_lock);

    ESP_LOGI(TAG, "Initializing USB CDC console");

#if CONFIG_BSP_USB_AUTO_DOWNLOAD
    const BaseType_t task_created = xTaskCreate(
                                        restart_task,
                                        "mosaico_usb_reset",
                                        USB_RESTART_TASK_STACK_SIZE,
                                        NULL,
                                        USB_RESTART_TASK_PRIORITY,
                                        &restart_task_handle
                                    );
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB restart task");
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    const esp_timer_create_args_t timer_config = {
        .callback = restart_timer_callback,
        .name = "mosaico_usb_delay",
    };
    ret = esp_timer_create(&timer_config, &restart_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create USB restart timer: %s", esp_err_to_name(ret));
        goto fail;
    }
#endif

    const tinyusb_config_t tinyusb_config = TINYUSB_DEFAULT_CONFIG(device_event_callback);
    ret = tinyusb_driver_install(&tinyusb_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        goto fail;
    }
    driver_installed = true;

    const tinyusb_config_cdcacm_t cdc_config = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
#if CONFIG_BSP_USB_AUTO_DOWNLOAD
        .callback_line_state_changed = line_state_changed_callback,
#else
        .callback_line_state_changed = NULL,
#endif
        .callback_line_coding_changed = NULL,
    };
    ret = tinyusb_cdcacm_init(&cdc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB CDC ACM: %s", esp_err_to_name(ret));
        goto fail;
    }
    cdc_initialized = true;

    ret = prepare_console_streams();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to redirect console to USB CDC: %s", esp_err_to_name(ret));
        goto fail;
    }

    portENTER_CRITICAL(&state_lock);
    reset_armed = false;
    pending_restart = USB_RESTART_NONE;
    initialized = true;
    initializing = false;
    portEXIT_CRITICAL(&state_lock);

    ESP_LOGI(TAG, "USB CDC console ready; reset reason=%d", (int)esp_reset_reason());
    return ESP_OK;

fail: {
        const esp_err_t cleanup_ret = cleanup_after_init_failure(driver_installed, cdc_initialized);
        if (cleanup_ret != ESP_OK) {
            portENTER_CRITICAL(&state_lock);
            terminal_error = cleanup_ret;
            portEXIT_CRITICAL(&state_lock);
            ESP_LOGE(TAG, "USB console rollback is incomplete; manual restart required: %s",
                     esp_err_to_name(cleanup_ret));
        }
    }
    portENTER_CRITICAL(&state_lock);
    initializing = false;
    portEXIT_CRITICAL(&state_lock);
    return ret;
}

static void auto_init_task(void *arg)
{
    (void)arg;

    const esp_err_t ret = esp_mosaico_usb_console_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "USB CDC initialization failed: %s", esp_err_to_name(ret));
    }
    vTaskDelete(NULL);
}

static esp_err_t configure_console_stream_buffering(FILE *stream, size_t index)
{
    /* Reopening output streams may reset IDF's line buffering to full buffering. */
    if ((index != 0) && (setvbuf(stream, NULL, _IOLBF, BUFSIZ) != 0)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t prepare_console_streams(void)
{
    const char *modes[CONSOLE_STREAM_COUNT] = {"r", "w", "w"};
    FILE *usb_probe[CONSOLE_STREAM_COUNT] = {NULL};
    console_streams.shared[0] = stdin;
    console_streams.shared[1] = stdout;
    console_streams.shared[2] = stderr;

    esp_err_t ret = esp_vfs_tusb_cdc_register(TINYUSB_CDC_ACM_0, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    console_streams.vfs_registered = true;

    /* Allocate every prospective stream before touching shared stdio. Keep
     * UART streams reserved so a failed freopen never needs another fopen. */
    for (size_t i = 0; i < CONSOLE_STREAM_COUNT; i++) {
        console_streams.uart_reserve[i] = fopen(UART_CONSOLE_PATH, modes[i]);
        if (console_streams.uart_reserve[i] == NULL) {
            ret = ESP_ERR_NO_MEM;
            break;
        }
        ret = configure_console_stream_buffering(console_streams.uart_reserve[i], i);
        if (ret != ESP_OK) {
            break;
        }
        usb_probe[i] = fopen(VFS_TUSB_PATH_DEFAULT, modes[i]);
        if (usb_probe[i] == NULL) {
            ret = ESP_FAIL;
            break;
        }
    }
    for (size_t i = 0; i < CONSOLE_STREAM_COUNT; i++) {
        if (usb_probe[i] != NULL) {
            if ((fclose(usb_probe[i]) != 0) && (ret == ESP_OK)) {
                ret = ESP_FAIL;
            }
        }
    }
    if (ret != ESP_OK) {
        return ret;
    }

    /* IDF tasks share FILE objects. Pointer assignment alone would redirect
     * only this task, so successful switching must preserve those objects. */
    for (size_t i = 0; i < CONSOLE_STREAM_COUNT; i++) {
        if (freopen(VFS_TUSB_PATH_DEFAULT, modes[i], console_streams.shared[i]) == NULL) {
            replace_failed_stream(i);
            return ESP_FAIL;
        }
        console_streams.switched[i] = true;
        ret = configure_console_stream_buffering(console_streams.shared[i], i);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static void replace_failed_stream(size_t index)
{
    /* freopen closes the original stream on failure. Never pass that FILE
     * to another stdio operation. Existing tasks may still hold its pointer;
     * restoring them atomically is not possible through public stdio APIs. */
    console_streams.invalid[index] = true;
    console_streams.switched[index] = false;
    FILE *fallback = console_streams.uart_reserve[index];
    console_streams.uart_reserve[index] = NULL;
    if (index == 0) {
        stdin = fallback;
    } else if (index == 1) {
        stdout = fallback;
    } else {
        stderr = fallback;
    }
}

static esp_err_t restore_console_streams(void)
{
    const char *modes[CONSOLE_STREAM_COUNT] = {"r", "w", "w"};
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < CONSOLE_STREAM_COUNT; i++) {
        if (console_streams.switched[i]) {
            if (freopen(UART_CONSOLE_PATH, modes[i], console_streams.shared[i]) == NULL) {
                replace_failed_stream(i);
            } else {
                console_streams.switched[i] = false;
                const esp_err_t buffer_ret = configure_console_stream_buffering(console_streams.shared[i], i);
                if ((buffer_ret != ESP_OK) && (ret == ESP_OK)) {
                    ret = buffer_ret;
                }
            }
        }
        if (console_streams.invalid[i]) {
            ret = ESP_ERR_INVALID_STATE;
        }
        if (console_streams.uart_reserve[i] != NULL) {
            /* fclose consumes a stream even if flushing reports an error. */
            if ((fclose(console_streams.uart_reserve[i]) != 0) && (ret == ESP_OK)) {
                ret = ESP_FAIL;
            }
            console_streams.uart_reserve[i] = NULL;
        }
    }
    if ((ret == ESP_OK) && console_streams.vfs_registered) {
        ret = esp_vfs_tusb_cdc_unregister(NULL);
        if (ret == ESP_OK) {
            console_streams.vfs_registered = false;
        }
    }
    return ret;
}

static esp_err_t cleanup_after_init_failure(bool driver_installed, bool cdc_initialized)
{
    esp_err_t ret = restore_console_streams();
    /* An incomplete stdio rollback must keep its VFS/CDC dependencies alive. */
    if ((ret == ESP_OK) && cdc_initialized) {
        ret = tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0);
    }
    if ((ret == ESP_OK) && driver_installed) {
        ret = tinyusb_driver_uninstall();
    }
#if CONFIG_BSP_USB_AUTO_DOWNLOAD
    if (restart_timer != NULL) {
        (void)esp_timer_delete(restart_timer);
        restart_timer = NULL;
    }
    if (restart_task_handle != NULL) {
        vTaskDelete(restart_task_handle);
        restart_task_handle = NULL;
    }
#endif
    return ret;
}

static void cleanup_before_restart(void)
{
    (void)tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(USB_FLUSH_TIMEOUT_MS));
    if (restore_console_streams() == ESP_OK) {
        (void)tinyusb_cdcacm_unregister_callback(TINYUSB_CDC_ACM_0, CDC_EVENT_LINE_STATE_CHANGED);
        if (tinyusb_cdcacm_deinit(TINYUSB_CDC_ACM_0) == ESP_OK) {
            (void)tinyusb_driver_uninstall();
        }
    }

    // Give the host a visible detach interval before the ROM downloader enumerates.
    vTaskDelay(pdMS_TO_TICKS(USB_REENUMERATION_DELAY_MS));
}

static void device_event_callback(tinyusb_event_t *event, void *arg)
{
    (void)arg;

    if ((event == NULL) || ((event->id != TINYUSB_EVENT_ATTACHED) &&
                            (event->id != TINYUSB_EVENT_DETACHED))) {
        return;
    }

    portENTER_CRITICAL(&state_lock);
    reset_armed = false;
    portEXIT_CRITICAL(&state_lock);
}

static void line_state_changed_callback(int interface, cdcacm_event_t *event)
{
    bool should_restart = false;

    if ((interface != TINYUSB_CDC_ACM_0) || (event == NULL) ||
            (event->type != CDC_EVENT_LINE_STATE_CHANGED)) {
        return;
    }

    const bool dtr = event->line_state_changed_data.dtr;
    const bool rts = event->line_state_changed_data.rts;

    portENTER_CRITICAL(&state_lock);
    if (initialized) {
        // Both esptool and idf-monitor use (DTR=0, RTS=1) as the reset
        // preamble. idf-monitor also toggles both lines while merely opening
        // the port, so an RTS falling edge alone is not a reset request.
        if (!dtr && rts) {
            reset_armed = true;
        } else if (reset_armed && !rts) {
            pending_restart = dtr ? USB_RESTART_BOOTLOADER : USB_RESTART_NORMAL;
            reset_armed = false;
            should_restart = true;
        }
    }
    portEXIT_CRITICAL(&state_lock);

    if (should_restart && (restart_timer != NULL)) {
        (void)esp_timer_stop(restart_timer);
        if (esp_timer_start_once(restart_timer, USB_RESTART_DELAY_US) != ESP_OK) {
            portENTER_CRITICAL(&state_lock);
            pending_restart = USB_RESTART_NONE;
            portEXIT_CRITICAL(&state_lock);
        }
    }
}

static void restart_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t notification_value = USB_RESTART_NONE;
        if (xTaskNotifyWait(0, UINT32_MAX, &notification_value, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const UsbRestartType restart_type = (UsbRestartType)notification_value;
        if ((restart_type != USB_RESTART_NORMAL) && (restart_type != USB_RESTART_BOOTLOADER)) {
            continue;
        }

        portENTER_CRITICAL(&state_lock);
        initialized = false;
        portEXIT_CRITICAL(&state_lock);

        ESP_LOGW(TAG, "Host requested USB %s via DTR/RTS",
                 restart_type == USB_RESTART_BOOTLOADER ? "download" : "restart");
        cleanup_before_restart();

        // ESP32-S31 has no public restart-to-download API in the selected IDF.
        // Keep the preview-target register access isolated to this board component.
        if (restart_type == USB_RESTART_BOOTLOADER) {
            REG_SET_BIT(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);
        } else {
            REG_CLR_BIT(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);
        }
        system_watchdog_reset();
    }
}

static void restart_timer_callback(void *arg)
{
    (void)arg;

    portENTER_CRITICAL(&state_lock);
    const UsbRestartType restart_type = pending_restart;
    pending_restart = USB_RESTART_NONE;
    portEXIT_CRITICAL(&state_lock);

    if ((restart_type == USB_RESTART_NONE) || (restart_task_handle == NULL)) {
        return;
    }

    (void)xTaskNotify(restart_task_handle, (uint32_t)restart_type, eSetValueWithOverwrite);
}

static void system_watchdog_reset(void)
{
    wdt_hal_context_t rtc_wdt;
    uint32_t timeout_ticks = (uint32_t)(
                                 (uint64_t)USB_SYSTEM_RESET_DELAY_MS * rtc_clk_slow_freq_get_hz() / 1000U
                             );
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    // esp_restart() only resets the CPUs on the preview ESP32-S31 target.
    // A system watchdog reset also resets the USB controller and UTMI PHY,
    // while preserving the LP-domain force-download flag.
    wdt_hal_init(&rtc_wdt, WDT_RWDT, 0, false);
    wdt_hal_write_protect_disable(&rtc_wdt);
    wdt_hal_config_stage(
        &rtc_wdt,
        WDT_STAGE0,
        timeout_ticks,
        WDT_STAGE_ACTION_RESET_SYSTEM
    );
    wdt_hal_enable(&rtc_wdt);
    wdt_hal_write_protect_enable(&rtc_wdt);

    while (true) {
    }
}

#if CONFIG_BSP_USB_CONSOLE_AUTO_INIT
ESP_SYSTEM_INIT_FN(esp_mosaico_usb_console_auto_init, SECONDARY, BIT(0), 260)
{
    const BaseType_t task_created = xTaskCreate(
                                        auto_init_task,
                                        "mosaico_usb_init",
                                        USB_INIT_TASK_STACK_SIZE,
                                        NULL,
                                        USB_INIT_TASK_PRIORITY,
                                        NULL
                                    );
    if (task_created != pdPASS) {
        ESP_EARLY_LOGW(TAG, "Failed to schedule USB CDC initialization; keeping the configured IDF console");
    }

    // USB is optional; a failure must not abort the ESP-IDF startup sequence.
    return ESP_OK;
}
#endif
