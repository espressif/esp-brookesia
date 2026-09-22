# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Fault-inject the real Mosaico cleanup callbacks without ESP-IDF or hardware.

Run through test_apps/host/run.py --suite core.
The fake drivers model both consuming and non-consuming bus deletion failures.
They deliberately assert child-before-parent release and reject double release.
"""

import pathlib
import shutil
import subprocess
import tempfile
import unittest


MOSAICO = pathlib.Path(__file__).resolve().parents[4] / 'hal/brookesia_hal_boards/components/mosaico'
CUSTOM = MOSAICO / 'brookesia_hal_custom'

STUB = r'''
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NO_MEM 4
#define ESP_ERR_NOT_SUPPORTED 5
#define ESP_ERR_NOT_FOUND 6
#define ESP_ERR_TIMEOUT 7
#define BIT(n) (1U << (n))
#define BIT64(n) (1ULL << (n))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_RETURN_ON_FALSE(a, e, ...) do { if (!(a)) return (e); } while (0)
#define ESP_RETURN_ON_ERROR(a, ...) do { int e = (a); if (e) return e; } while (0)
#define CONFIG_NAND_FLASH_ENABLE_BDL 1
#define CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES 1
#define SOC_USB_SERIAL_JTAG_SUPPORTED 0
static inline const char *esp_err_to_name(esp_err_t e) { (void)e; return "injected"; }
typedef int gpio_num_t;
#define GPIO_NUM_NC -1
#define GPIO_NUM_33 33
#define GPIO_NUM_34 34
#define GPIO_NUM_48 48
#define GPIO_NUM_53 53
#define GPIO_MODE_OUTPUT 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define GPIO_IS_VALID_OUTPUT_GPIO(n) ((n) >= 0 && (n) < 64)
typedef struct { uint64_t pin_bit_mask; int mode, pull_up_en, pull_down_en, intr_type; } gpio_config_t;
typedef struct { int unused; } gpio_io_config_t;
typedef void *spi_device_handle_t;
typedef struct { int spi_port; } periph_spi_handle_t;
typedef struct { int clock_speed_hz, mode, spics_io_num, queue_size, flags; } spi_device_interface_config_t;
#define SPI_DEVICE_HALFDUPLEX 1
#define SPI_NAND_IO_MODE_SIO 1
typedef struct blockdev *esp_blockdev_handle_t;
struct blockdev_ops {
    esp_err_t (*sync)(esp_blockdev_handle_t);
    esp_err_t (*release)(esp_blockdev_handle_t);
};
struct blockdev { struct blockdev_ops *ops; };
typedef struct { void *device_handle; int gc_factor, io_mode, flags; } spi_nand_flash_config_t;
typedef void *i2c_master_dev_handle_t;
typedef void *i2c_master_bus_handle_t;
typedef struct { int dev_addr_length, device_address, scl_speed_hz; } i2c_device_config_t;
#define I2C_ADDR_BIT_LEN_7 0
typedef void *SemaphoreHandle_t;
#define portMAX_DELAY 0
#define pdMS_TO_TICKS(n) (n)
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
static inline void xSemaphoreTake(SemaphoreHandle_t s, int t) { (void)s; (void)t; }
static inline void xSemaphoreGive(SemaphoreHandle_t s) { (void)s; }
static inline void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
static inline void vTaskDelay(int t) { (void)t; }

static int refs, unrefs, children, removes, adds, bdl_releases, resets;
static int ref_failure, add_failure, probe_failure, bdl_init_failure, remove_failures;
static int unref_failure, unref_consumes, bdl_release_failure, sync_failure, reset_failure;
static int bus_live, bdl_live;
static periph_spi_handle_t bus = { .spi_port = 1 };
static int gpio_config(const gpio_config_t *c) { (void)c; return 0; }
static int gpio_set_level(gpio_num_t p, int l) { (void)p; (void)l; return 0; }
static int gpio_get_io_config(gpio_num_t p, gpio_io_config_t *c) { (void)p; (void)c; return 0; }
static int gpio_reset_pin(gpio_num_t p) { (void)p; resets++; return reset_failure; }
static int brookesia_hal_board_periph_ref_handle(const char *n, void **h)
{
    (void)n; refs++;
    if (ref_failure) return ref_failure;
    assert(!bus_live); bus_live = 1; *h = &bus; return 0;
}
static int brookesia_hal_board_periph_unref_handle(const char *n)
{
    (void)n; assert(bus_live); assert(children == 0); unrefs++;
    if (!unref_failure || unref_consumes) bus_live = 0;
    return unref_failure;
}
static int add_child(void **h)
{
    assert(bus_live); adds++;
    if (add_failure) return add_failure;
    children++; *h = (void *)(uintptr_t)(adds + 1); return 0;
}
static int remove_child(void *h)
{
    assert(h); assert(children > 0); assert(bus_live); removes++;
    if (remove_failures) { remove_failures--; return ESP_ERR_TIMEOUT; }
    children--; return 0;
}
static int spi_bus_add_device(int n, const spi_device_interface_config_t *c, void **h)
{ (void)n; (void)c; return add_child(h); }
static int spi_bus_remove_device(void *h) { return remove_child(h); }
static int i2c_master_bus_add_device(void *b, const i2c_device_config_t *c, void **h)
{ assert(b); (void)c; return add_child(h); }
static int i2c_master_bus_rm_device(void *h) { return remove_child(h); }
static int i2c_master_probe(void *b, int a, int t) { (void)b; (void)a; (void)t; return probe_failure; }
static int i2c_master_transmit_receive(void *h, const void *w, size_t wn, void *r, size_t rn, int t)
{ assert(h); (void)w; (void)wn; (void)t; memset(r, 0, rn); return probe_failure; }
static int fake_sync(esp_blockdev_handle_t h) { (void)h; assert(bdl_live); return sync_failure; }
static int fake_release(esp_blockdev_handle_t h)
{ (void)h; assert(bdl_live); bdl_live = 0; bdl_releases++; return bdl_release_failure; }
static struct blockdev_ops bdl_ops = {fake_sync, fake_release};
static struct blockdev bdl = {&bdl_ops};
static int spi_nand_flash_init_with_layers(spi_nand_flash_config_t *c, esp_blockdev_handle_t *h)
{ (void)c; if (bdl_init_failure) return bdl_init_failure; bdl_live = 1; *h = &bdl; return 0; }
'''

DRIVER_MAIN = r'''
int main(int argc, char **argv)
{
    assert(argc == 2);
    const int scenario = atoi(argv[1]);
    void *handle = NULL;
    CONFIG config = CONFIG_VALUE;
    if (scenario == 0) {
        assert(INIT(&config, &handle) == ESP_OK);
        remove_failures = 1;
        assert(DEINIT(handle) == ESP_ERR_TIMEOUT);
        assert(PENDING() && children == 1 && unrefs == 0);
        int before = removes;
        assert(ERROR() == ESP_ERR_TIMEOUT && ERROR() == ESP_ERR_TIMEOUT);
        assert(removes == before); /* A facade query must never trigger cleanup. */
        assert(CLEANUP() == ESP_OK);
        assert(!PENDING() && unrefs == 1 && children == 0);
    } else if (scenario == 1) {
        PROBE_FAILURE = ESP_FAIL;
        remove_failures = 1;
        assert(INIT(&config, &handle) == ESP_FAIL && handle == NULL);
        assert(PENDING() && children == 1 && unrefs == 0);
        PROBE_FAILURE = ESP_OK;
        assert(INIT(&config, &handle) == ESP_OK);
        assert(refs == 2 && unrefs == 1 && children == 1);
        assert(DEINIT(handle) == ESP_OK && unrefs == 2);
    } else if (scenario == 2 || scenario == 3) {
        assert(INIT(&config, &handle) == ESP_OK);
        unref_failure = ESP_ERR_INVALID_STATE;
        unref_consumes = scenario == 3;
        assert(DEINIT(handle) == ESP_ERR_INVALID_STATE);
        assert(PENDING() && ERROR() == ESP_ERR_INVALID_STATE);
        assert(bus_live == (scenario == 2));
        int before = removes;
        assert(CLEANUP() == ESP_ERR_INVALID_STATE);
        handle = (void *)1;
        assert(INIT(&config, &handle) == ESP_ERR_INVALID_STATE && handle == NULL);
        assert(unrefs == 1 && refs == 1 && removes == before);
    } else if (scenario == 4) {
        assert(INIT(&config, &handle) == ESP_OK);
        bdl_release_failure = ESP_FAIL;
        remove_failures = 1;
        assert(DEINIT(handle) == ESP_ERR_TIMEOUT && bdl_releases == 1);
        assert(CLEANUP() == ESP_FAIL);
        assert(!PENDING() && bdl_releases == 1 && unrefs == 1);
        assert(ERROR() == ESP_FAIL && CLEANUP() == ESP_OK && ERROR() == ESP_FAIL);
        bdl_release_failure = ESP_OK;
        assert(INIT(&config, &handle) == ESP_OK && ERROR() == ESP_OK);
        assert(DEINIT(handle) == ESP_OK);
    } else if (scenario == 5) {
        assert(INIT(&config, &handle) == ESP_OK);
        reset_failure = ESP_FAIL;
        assert(DEINIT(handle) == ESP_FAIL && PENDING());
        reset_failure = ESP_OK;
        assert(CLEANUP() == ESP_OK);
        assert(!PENDING() && unrefs == 1 && bdl_releases == 1);
    }
    return 0;
}
'''

MANAGER_MAIN = r'''
int main(int argc, char **argv)
{
    assert(argc == 2);
    const int scenario = atoi(argv[1]);
    const esp_mosaico_expansion_manager_config_t config = {
        .i2c_name = "i2c", .frequency_hz = 400000, .timeout_ms = 100,
        .slots = {{1, 0, 0x50}, {2, 1, 0x51}},
    };
    if (scenario == 3 || scenario == 4) {
        add_failure = ESP_ERR_NO_MEM;
        unref_failure = ESP_FAIL;
        unref_consumes = scenario == 4;
        assert(esp_mosaico_expansion_manager_init(&config) == ESP_ERR_NO_MEM);
        assert(esp_mosaico_expansion_manager_cleanup_pending());
        assert(!esp_mosaico_expansion_manager_is_initialized() && manager.i2c_bus == NULL);
        assert(esp_mosaico_expansion_manager_init(&config) == ESP_FAIL);
        assert(esp_mosaico_expansion_manager_deinit() == ESP_FAIL);
        assert(esp_mosaico_expansion_manager_recover() == ESP_FAIL);
        assert(refs == 1 && unrefs == 1);
        return 0;
    }
    assert(esp_mosaico_expansion_manager_init(&config) == ESP_OK);
    if (scenario < 2) {
        unref_failure = ESP_FAIL;
        unref_consumes = scenario;
        assert(esp_mosaico_expansion_manager_deinit() == ESP_FAIL);
        assert(esp_mosaico_expansion_restore_address_selects() == ESP_FAIL);
        assert(esp_mosaico_camera_slot_acquire(NULL) == ESP_FAIL);
        assert(esp_mosaico_camera_slot_release(0) == ESP_FAIL);
        assert(esp_mosaico_expansion_manager_cleanup_pending());
        assert(manager.i2c_bus == NULL);
        int before = adds + removes + resets;
        assert(esp_mosaico_expansion_manager_recover() == ESP_FAIL);
        assert(esp_mosaico_expansion_manager_init(&config) == ESP_FAIL);
        assert(esp_mosaico_expansion_manager_deinit() == ESP_FAIL);
        esp_mosaico_expansion_info_t info;
        assert(esp_mosaico_expansion_scan(0, &info) == ESP_ERR_INVALID_STATE);
        assert(unrefs == 1 && refs == 1 && before == adds + removes + resets);
    } else {
        remove_failures = 5;
        assert(esp_mosaico_expansion_manager_deinit() == ESP_ERR_TIMEOUT);
        assert(unrefs == 0 && children == 2);
        assert(esp_mosaico_expansion_manager_recover() == ESP_OK);
        assert(!esp_mosaico_expansion_manager_cleanup_pending());
        assert(esp_mosaico_expansion_manager_deinit() == ESP_OK);
        assert(unrefs == 1 && children == 0);
    }
    return 0;
}
'''

USB_STUB = r'''
#include "stub.h"
#include <stdio.h>
#define CONFIG_BSP_USB_AUTO_DOWNLOAD 0
#define CONFIG_BSP_USB_CONSOLE_AUTO_INIT 0
#define CONFIG_ESP_CONSOLE_UART_NUM 0
#define ESP_TASK_MAIN_PRIO 1
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdTRUE 1
#define eSetValueWithOverwrite 0
#define TINYUSB_CDC_ACM_0 0
#define CDC_EVENT_LINE_STATE_CHANGED 1
#define TINYUSB_EVENT_ATTACHED 1
#define TINYUSB_EVENT_DETACHED 2
#define VFS_TUSB_PATH_DEFAULT "/dev/tusb_cdc"
#define TINYUSB_DEFAULT_CONFIG(cb) {.callback = (cb)}
#define LP_SYSTEM_REG_SYS_CTRL_REG 0
#define LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT 1
#define REG_SET_BIT(a, b) ((void)0)
#define REG_CLR_BIT(a, b) ((void)0)
#define WDT_RWDT 0
#define WDT_STAGE0 0
#define WDT_STAGE_ACTION_RESET_SYSTEM 0
typedef int portMUX_TYPE;
typedef void *TaskHandle_t;
typedef void *esp_timer_handle_t;
typedef int wdt_hal_context_t;
typedef struct { int id; } tinyusb_event_t;
typedef struct { void (*callback)(tinyusb_event_t *, void *); } tinyusb_config_t;
typedef struct { int type; struct { bool dtr, rts; } line_state_changed_data; } cdcacm_event_t;
typedef struct {
    int cdc_port;
    void (*callback_rx)(int, cdcacm_event_t *);
    void (*callback_rx_wanted_char)(int, cdcacm_event_t *);
    void (*callback_line_state_changed)(int, cdcacm_event_t *);
    void (*callback_line_coding_changed)(int, cdcacm_event_t *);
} tinyusb_config_cdcacm_t;
void vTaskDelete(void *);
int xTaskNotifyWait(uint32_t, uint32_t, uint32_t *, int);
int xTaskNotify(void *, uint32_t, int);
int esp_timer_stop(void *);
int esp_timer_start_once(void *, int);
uint32_t rtc_clk_slow_freq_get_hz(void);
void wdt_hal_init();
void wdt_hal_write_protect_disable();
void wdt_hal_config_stage();
void wdt_hal_enable();
void wdt_hal_write_protect_enable();
static int usb_live, cdc_live, vfs_live, installs, uninstalls, cdc_removes, vfs_removes;
static int register_failure, unregister_failure, driver_failure, cdc_failure;
static int open_calls, usb_reopens, uart_reopens, fail_open, fail_usb_reopen, fail_uart_reopen;
typedef struct { bool live, usb; } fake_file_t;
static fake_file_t files[32];
static int file_count = 3;
static FILE *task_stdin, *task_stdout, *task_stderr;
static FILE *fake_fopen(const char *p, const char *m)
{
    (void)m;
    if (++open_calls == fail_open) return NULL;
    assert(file_count < 32);
    bool usb = strstr(p, "tusb") != NULL;
    if (usb) assert(vfs_live);
    files[file_count] = (fake_file_t){true, usb};
    return (FILE *)&files[file_count++];
}
static int fake_fclose(FILE *f)
{
    fake_file_t *s = (fake_file_t *)f;
    assert(s->live); s->live = false; return 0;
}
static FILE *fake_freopen(const char *p, const char *m, FILE *f)
{
    (void)m;
    fake_file_t *s = (fake_file_t *)f;
    assert(s->live); /* A closed FILE from a failed freopen must not be reused. */
    bool usb = strstr(p, "tusb") != NULL;
    int call = usb ? ++usb_reopens : ++uart_reopens;
    if (call == (usb ? fail_usb_reopen : fail_uart_reopen)) {
        s->live = false;
        return NULL;
    }
    s->usb = usb; return f;
}
static int fake_setvbuf(FILE *f, char *buffer, int mode, size_t size)
{
    fake_file_t *s = (fake_file_t *)f;
    assert(s->live && buffer == NULL && mode == _IOLBF && size == BUFSIZ);
    return 0;
}
#undef stdin
#undef stdout
#undef stderr
#define stdin task_stdin
#define stdout task_stdout
#define stderr task_stderr
#define fopen fake_fopen
#define fclose fake_fclose
#define freopen fake_freopen
#define setvbuf fake_setvbuf
static int tinyusb_driver_install(const tinyusb_config_t *c)
{ (void)c; assert(!usb_live); installs++; if (driver_failure) return ESP_FAIL; usb_live = 1; return 0; }
static int tinyusb_driver_uninstall(void)
{ assert(!vfs_live && !cdc_live); uninstalls++; usb_live = 0; return 0; }
static int tinyusb_cdcacm_init(const tinyusb_config_cdcacm_t *c)
{ (void)c; assert(usb_live && !cdc_live); if (cdc_failure) return ESP_FAIL; cdc_live = 1; return 0; }
static int tinyusb_cdcacm_deinit(int n)
{ (void)n; assert(!vfs_live); cdc_removes++; cdc_live = 0; return 0; }
static int tinyusb_cdcacm_write_flush(int n, int t) { (void)n; (void)t; return 0; }
static int tinyusb_cdcacm_unregister_callback(int n, int t) { (void)n; (void)t; return 0; }
static int esp_vfs_tusb_cdc_register(int n, const char *p)
{ (void)n; (void)p; assert(cdc_live && !vfs_live); if (register_failure) return ESP_FAIL; vfs_live = 1; return 0; }
static int esp_vfs_tusb_cdc_unregister(const char *p)
{ (void)p; assert(vfs_live); vfs_removes++; if (unregister_failure) return ESP_FAIL; vfs_live = 0; return 0; }
'''

USB_MAIN = r'''
int main(int argc, char **argv)
{
    assert(argc == 2);
    int scenario = atoi(argv[1]);
    for (int i = 0; i < 3; i++) files[i].live = true;
    task_stdin = (FILE *)&files[0]; task_stdout = (FILE *)&files[1]; task_stderr = (FILE *)&files[2];
    if (scenario >= 1 && scenario <= 6) fail_open = scenario;
    if (scenario >= 11 && scenario <= 13) fail_usb_reopen = scenario - 10;
    if (scenario == 20) { fail_usb_reopen = 3; fail_uart_reopen = 1; }
    if (scenario == 30) register_failure = 1;
    if (scenario == 31) { fail_open = 1; unregister_failure = 1; }
    if (scenario == 32) driver_failure = 1;
    if (scenario == 33) cdc_failure = 1;
    int ret = esp_mosaico_usb_console_init();
    if (scenario == 0 || scenario == 40) {
        assert(ret == ESP_OK && initialized);
        for (int i = 0; i < 3; i++) assert(files[i].live && files[i].usb);
        assert(esp_mosaico_usb_console_init() == ESP_OK && installs == 1);
        if (scenario == 40) fail_uart_reopen = 2;
        cleanup_before_restart();
        if (scenario == 0) {
            assert(!usb_live && !cdc_live && !vfs_live);
        } else {
            assert(usb_live && cdc_live && vfs_live);
            assert(!files[1].live && ((fake_file_t *)task_stdout)->live);
        }
    } else if ((scenario >= 11 && scenario <= 20) || scenario == 31) {
        assert(ret != ESP_OK && terminal_error != ESP_OK);
        assert(usb_live && cdc_live && vfs_live && uninstalls == 0 && cdc_removes == 0);
        int calls = open_calls + usb_reopens + uart_reopens;
        assert(esp_mosaico_usb_console_init() != ESP_OK);
        assert(installs == 1 && calls == open_calls + usb_reopens + uart_reopens);
        FILE *current[] = {task_stdin, task_stdout, task_stderr};
        for (int i = 0; i < 3; i++) {
            fake_file_t *s = (fake_file_t *)current[i];
            assert(s->live && !s->usb); /* Only the current task is restored. */
        }
        if (scenario != 31) assert(!files[fail_usb_reopen - 1].live);
    } else {
        assert(ret != ESP_OK && terminal_error == ESP_OK);
        assert(!usb_live && !cdc_live && !vfs_live && usb_reopens == 0);
        for (int i = 0; i < 3; i++) assert(files[i].live && !files[i].usb);
        fail_open = driver_failure = cdc_failure = register_failure = 0;
        assert(esp_mosaico_usb_console_init() == ESP_OK);
        cleanup_before_restart();
    }
    return 0;
}
'''


class MosaicoCleanupTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if shutil.which('cc') is None:
            raise RuntimeError('A host C compiler is required')
        cls.temporary = tempfile.TemporaryDirectory(prefix='mosaico-cleanup-')
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = pathlib.Path(cls.temporary.name)
        (cls.directory / 'stub.h').write_text(STUB)
        headers = [
            'driver/gpio.h', 'driver/spi_master.h', 'driver/i2c_master.h',
            'brookesia/hal_adaptor/board_manager.h', 'esp_bit_defs.h', 'esp_blockdev.h',
            'esp_err.h', 'esp_log.h', 'periph_spi.h',
            'spi_nand_flash.h', 'esp_check.h', 'esp_system.h', 'esp_timer.h',
            'freertos/FreeRTOS.h', 'freertos/semphr.h', 'freertos/task.h',
            'hal/usb_serial_jtag_ll.h', 'soc/soc_caps.h', 'sdkconfig.h',
        ]
        for name in headers:
            header = cls.directory / name
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#include "stub.h"\n')
        for name in ('fs_nand', 'bq27220_fuel_gauge'):
            api_name = 'esp_mosaico_nand' if name == 'fs_nand' else 'esp_mosaico_fuel_gauge'
            defines = f'''
                #define INIT {api_name}_init
                #define DEINIT {api_name}_deinit
                #define PENDING {name}_cleanup_pending
                #define CLEANUP {name}_cleanup
                #define ERROR {name}_cleanup_error
                #define CONFIG {api_name}_config_t
            '''
            if name == 'fs_nand':
                defines += '''
                    #define PROBE_FAILURE bdl_init_failure
                    #define CONFIG_VALUE {.peripheral_name = "spi", .cs_gpio_num = 5, \\
                        .hold_gpio_num = 3, .wp_gpio_num = 4, .clock_speed_hz = 1000000, \\
                        .queue_size = 1, .gc_factor = 4}
                '''
            else:
                defines += '''
                    #define PROBE_FAILURE probe_failure
                    #define CONFIG_VALUE {.peripheral_name = "i2c", .i2c_addr = 0x55, .frequency = 400000}
                '''
            cls.compile(name, f'#include "{CUSTOM / "src/board" / (name + ".c")}"\n' + defines + DRIVER_MAIN)
        cls.compile('manager', f'#include "{CUSTOM / "src/expansion/mosaico_module_manager.c"}"\n' + MANAGER_MAIN)
        usb = MOSAICO / 'esp_mosaico_usb_console'
        for name in ('esp_private/startup_internal.h', 'esp_task.h', 'hal/wdt_hal.h',
                     'soc/lp_system_reg.h', 'soc/rtc.h', 'soc/soc.h', 'tinyusb.h',
                     'tinyusb_cdc_acm.h', 'tinyusb_default_config.h', 'vfs_tinyusb.h'):
            header = cls.directory / name
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#include "stub.h"\n')
        cls.compile('usb', USB_STUB + f'\n#include "{usb / "src/esp_mosaico_usb_console.c"}"\n' + USB_MAIN,
                    extra=['-I', str(usb / 'include')])

    @classmethod
    def compile(cls, name, source, extra=None):
        path = cls.directory / (name + '.c')
        path.write_text(source)
        result = subprocess.run([
            'cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
            '-Wno-unused-variable', '-g', '-fsanitize=undefined',
            '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
            '-I', str(cls.directory), '-I', str(CUSTOM / 'include'),
            str(path), '-o', str(cls.directory / name),
        ] + (extra or []), text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def run_case(self, name, scenario):
        result = subprocess.run([str(self.directory / name), str(scenario)], text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_child_failure_retains_owner_and_queries_do_not_retry(self):
        for name in ('fs_nand', 'bq27220_fuel_gauge'):
            with self.subTest(device=name):
                self.run_case(name, 0)

    def test_init_rollback_completes_before_next_init(self):
        for name in ('fs_nand', 'bq27220_fuel_gauge'):
            with self.subTest(device=name):
                self.run_case(name, 1)

    def test_unknown_bus_release_blocks_every_retry(self):
        for name in ('fs_nand', 'bq27220_fuel_gauge', 'manager'):
            for consumes in (False, True):
                with self.subTest(device=name, consumes=consumes):
                    self.run_case(name, int(consumes) + (0 if name == 'manager' else 2))

    def test_bdl_error_is_reported_without_releasing_consumed_handle_again(self):
        self.run_case('fs_nand', 4)

    def test_gpio_cleanup_retry_does_not_release_nand_or_bus_again(self):
        self.run_case('fs_nand', 5)

    def test_manager_child_failure_can_recover_without_bm_reference_cycle(self):
        self.run_case('manager', 2)

    def test_manager_failed_init_rollback_isolates_unknown_bus_owner(self):
        for scenario in (3, 4):
            with self.subTest(failure=scenario):
                self.run_case('manager', scenario)

    def test_usb_preparation_failure_keeps_shared_uart_untouched(self):
        for scenario in (*range(1, 7), 30, 32, 33):
            with self.subTest(failure=scenario):
                self.run_case('usb', scenario)

    def test_usb_failed_shared_stream_is_never_reused_and_requires_restart(self):
        for scenario in (11, 12, 13, 20):
            with self.subTest(failure=scenario):
                self.run_case('usb', scenario)

    def test_usb_failed_vfs_unregistration_retains_dependencies(self):
        self.run_case('usb', 31)

    def test_usb_success_and_restart_cleanup(self):
        self.run_case('usb', 0)

    def test_usb_restart_cleanup_failure_does_not_destroy_vfs_dependencies(self):
        self.run_case('usb', 40)


def run():
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(MosaicoCleanupTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful():
        raise RuntimeError('Mosaico cleanup regression failed')
