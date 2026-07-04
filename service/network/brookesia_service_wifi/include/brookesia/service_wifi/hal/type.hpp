/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service::wifi {

/**
 * @brief Re-exported helper types used by the Wi-Fi HAL-facing APIs.
 */
using GeneralAction = hal::wifi::GeneralAction;
/**
 * @brief Re-exported general Wi-Fi event identifiers.
 */
using GeneralEvent = hal::wifi::GeneralEvent;
/**
 * @brief Parameters used to connect to an access point.
 */
using ConnectApInfo = hal::wifi::ConnectApInfo;
/**
 * @brief Parameters used to configure an access-point scan.
 */
using ScanParams = hal::wifi::ScanParams;
/**
 * @brief Signal strength classification for scanned access points.
 */
using ScanApSignalLevel = hal::wifi::ScanApSignalLevel;
/**
 * @brief Information returned for one scanned access point.
 */
using ScanApInfo = hal::wifi::ScanApInfo;
/**
 * @brief Parameters used to configure soft-AP mode.
 */
using SoftApParams = hal::wifi::SoftApParams;
/**
 * @brief Re-exported soft-AP event identifiers.
 */
using SoftApEvent = hal::wifi::SoftApEvent;

} // namespace esp_brookesia::service::wifi
