/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "soc/usb_pins.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "bsp/display.h"

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        0
#define BSP_CAPS_AUDIO          0
#define BSP_CAPS_AUDIO_SPEAKER  0
#define BSP_CAPS_AUDIO_MIC      0
#define BSP_CAPS_SDCARD         1
#define BSP_CAPS_IMU            0

/* I2C */
#define BSP_I2C_SCL           (GPIO_NUM_1)
#define BSP_I2C_SDA           (GPIO_NUM_2)
#define BSP_IMU_INT           (GPIO_NUM_21)

/* Audio */
#define BSP_I2S_SCLK          (GPIO_NUM_40) // BCLK
#define BSP_I2S_MCLK          (GPIO_NUM_42)
#define BSP_I2S_LCLK          (GPIO_NUM_39) // WS
#define BSP_I2S_DOUT          (GPIO_NUM_41)     // To Codec ES8311
#if CONFIG_BSP_PCB_VERSION_V1_2
#   define BSP_I2S_DSIN        (GPIO_NUM_3)
#   define BSP_POWER_AMP_IO    (GPIO_NUM_15)
#   define BSP_POWER_CODEC_EN  (GPIO_NUM_48)
#elif CONFIG_BSP_PCB_VERSION_V1_0
#   define BSP_I2S_DSIN        (GPIO_NUM_15)    // From ADC ES7210
#   define BSP_POWER_AMP_IO    (GPIO_NUM_4)
#endif

/* Display */
#define BSP_LCD_DATA3         (GPIO_NUM_12)
#define BSP_LCD_DATA2         (GPIO_NUM_11)
#define BSP_LCD_DATA1         (GPIO_NUM_13)
#define BSP_LCD_DATA0         (GPIO_NUM_46)
#define BSP_LCD_PCLK          (GPIO_NUM_18)
#define BSP_LCD_CS            (GPIO_NUM_14)
#define BSP_LCD_DC            (GPIO_NUM_45)
#if CONFIG_BSP_PCB_VERSION_V1_2
#   define BSP_LCD_RST        (GPIO_NUM_47)
#elif CONFIG_BSP_PCB_VERSION_V1_0
#   define BSP_LCD_RST        (GPIO_NUM_3)
#endif
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_44)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_10)

/* Power */
#define BSP_POWER_OFF         (GPIO_NUM_9)

/* SD card */
#define BSP_SD_D0             (GPIO_NUM_17)
#define BSP_SD_CMD            (GPIO_NUM_38)
#define BSP_SD_CLK            (GPIO_NUM_16)

/* USB */
#define BSP_USB_DM            (GPIO_NUM_19)
#define BSP_USB_DP            (GPIO_NUM_20)

/* Others */
#if CONFIG_BSP_PCB_VERSION_V1_2
#   define BSP_UART1_TX       (GPIO_NUM_5)
#   define BSP_UART1_RX       (GPIO_NUM_4)
#elif CONFIG_BSP_PCB_VERSION_V1_0
#   define BSP_UART1_TX       (GPIO_NUM_6)
#   define BSP_UART1_RX       (GPIO_NUM_5)
#endif
#if CONFIG_BSP_PCB_VERSION_V1_2
#   define BSP_TOUCH_PAD1     (GPIO_NUM_7)
#   define BSP_TOUCH_PAD2     (GPIO_NUM_6)
#elif CONFIG_BSP_PCB_VERSION_V1_0
#   define BSP_TOUCH_PAD1     (GPIO_NUM_7)
#endif
#define BSP_HEAD_LED          (GPIO_NUM_43) // GREEN LED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t        buffer_size;    /*!< Size of the buffer for the screen in pixels */
    bool            double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
        unsigned int default_dummy_draw: 1;   /* Use dummy draw to bypass the display driver */
    } flags;
} bsp_display_cfg_t;

/**************************************************************************************************
 *
 * I2C interface
 *
 * There are multiple devices connected to I2C peripheral:
 *  - Codec ES8311 (configuration only)
 *  - ADC ES7210 (configuration only)
 *  - Encryption chip ATECC608A (NOT populated on most boards)
 *  - LCD Touch controller
 *  - Inertial Measurement Unit ICM-42607-P
 *
 * After initialization of I2C, use BSP_I2C_NUM macro when creating I2C devices drivers ie.:
 * \code{.c}
 * icm42670_handle_t imu = icm42670_create(BSP_I2C_NUM, ICM42670_I2C_ADDRESS);
 * \endcode
 **************************************************************************************************/
#define BSP_I2C_NUM     CONFIG_BSP_I2C_NUM

/**
 * @brief Init I2C driver
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C driver handle
 *
 * @return
 *      - I2C handle
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 *
 * LCD interface
 *
 * ESP-BOX is shipped with 2.4inch ST7789 display controller.
 * It features 16-bit colors, 320x240 resolution and capacitive touch controller.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/
#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define BSP_LCD_SPI_NUM            (SPI2_HOST)

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Initialize display brightness
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief Initialize power
 *
 * @param power_en power enable
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_power_init(bool power_en);

/**************************************************************************************************
 *
 * SD card
 *
 * After mounting the SD card, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_SD_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello %s!\n", bsp_sdcard->cid.name);
 * fclose(f);
 * \endcode
 *
 * @attention IO2 is also routed to RGB LED and push button
 **************************************************************************************************/
#define BSP_SD_MOUNT_POINT      CONFIG_BSP_SD_MOUNT_POINT

/**
 * @brief BSP SD card configuration structure
 */
typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t   *sdmmc;
    } slot;
} bsp_sdcard_cfg_t;

/**
 * @brief Mount microSD card to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount micorSD card from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the partition table does not contain FATFS partition with given label
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_spiflash_mount was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes from wear levelling library, SPI flash driver, or FATFS drivers
 */
esp_err_t bsp_sdcard_unmount(void);

/**
 * @brief Get SD card handle
 *
 * @return SD card handle
 */
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**
 * @brief Get SD card MMC host config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_get_sdmmc_host(const int slot, sdmmc_host_t *config);

/**
 * @brief Get SD card MMC slot config
 *
 * @param slot SD card slot
 * @param config Structure which will be filled
 */
void bsp_sdcard_sdmmc_get_slot(const int slot, sdmmc_slot_config_t *config);

/**
 * @brief Mount microSD card to virtual file system (MMC mode)
 *
 * @param cfg SD card configuration
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_sdmmc_mount(bsp_sdcard_cfg_t *cfg);

/**
 * @brief Get SD card handle
 *
 * @return SD card handle
 */
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**
 * @brief Set peripheral power
 *
 * @param on true to turn on, false to turn off
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bsp_set_peripheral_power(bool on);


#ifdef __cplusplus
}
#endif
