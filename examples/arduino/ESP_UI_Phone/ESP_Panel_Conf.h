/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// Debug Configurations /////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Set to 1 if assert on error. Otherwise print error message */
#define ESP_PANEL_CHECK_RESULT_ASSERT       (0)         // 0/1

/* Set to 1 if print log message for debug */
#define ESP_PANEL_ENABLE_LOG                (0)         // 0/1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////// Touch Driver Configurations //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Maximum point number */
#define ESP_PANEL_TOUCH_MAX_POINTS          (5)
/* Maximum button number */
#define ESP_PANEL_TOUCH_MAX_BUTTONS         (1)

/**
 * XPT2046 related
 *
 */
#define ESP_PANEL_TOUCH_XPT2046_Z_THRESHOLD             (400)   // Minimum Z pressure threshold
/**
 * Enable Interrupt (PENIRQ) output, also called Full Power Mode.
 * Enable this to configure the XPT2046 to output low on the PENIRQ output if a touch is detected.
 * This mode uses more power when enabled. Note that this signal goes low normally when a read is active.
 */
#define ESP_PANEL_TOUCH_XPT2046_INTERRUPT_MODE          (0)     // 0/1
/**
 * Keep internal Vref enabled.
 * Enable this to keep the internal Vref enabled between conversions. This uses slightly more power,
 * but requires fewer transactions when reading the battery voltage, aux voltage and temperature.
 *
 */
#define ESP_PANEL_TOUCH_XPT2046_VREF_ON_MODE            (0)     // 0/1
/**
 * Convert touch coordinates to screen coordinates.
 * When this option is enabled the raw ADC values will be converted from 0-4096 to 0-{screen width} or 0-{screen height}.
 * When this option is disabled the process_coordinates method will need to be used to convert the raw ADC values into a
 * screen coordinate.
 *
 */
#define ESP_PANEL_TOUCH_XPT2046_CONVERT_ADC_TO_COORDS   (1)     // 0/1
/**
 * Enable data structure locking.
 * By enabling this option the XPT2046 driver will lock the touch position data structures when reading values from the
 * XPT2046 and when reading position data via API.
 * WARNING: enabling this option may result in unintended crashes.
 *
 */
#define ESP_PANEL_TOUCH_XPT2046_ENABLE_LOCKING          (0)     // 0/1

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// File Version ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Do not change the following versions, they are used to check if the configurations in this file are compatible with
 * the current version of `ESP_Panel_Conf.h` in the library. The detailed rules are as follows:
 *
 *   1. If the major version is not consistent, then the configurations in this file are incompatible with the library
 *      and must be replaced with the file from the library.
 *   2. If the minor version is not consistent, this file might be missing some new configurations, which will be set to
 *      default values. It is recommended to replace it with the file from the library.
 *   3. Even if the patch version is not consistent, it will not affect normal functionality.
 *
 */
#define ESP_PANEL_CONF_FILE_VERSION_MAJOR 0
#define ESP_PANEL_CONF_FILE_VERSION_MINOR 1
#define ESP_PANEL_CONF_FILE_VERSION_PATCH 2
