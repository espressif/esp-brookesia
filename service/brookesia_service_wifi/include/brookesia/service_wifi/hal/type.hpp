/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/wifi.hpp"

namespace esp_brookesia::service::wifi {

/**
 * @brief Re-exported helper types used by the Wi-Fi HAL-facing APIs.
 */
using GeneralAction = helper::Wifi::GeneralAction;
/**
 * @brief Re-exported general Wi-Fi event identifiers.
 */
using GeneralEvent = helper::Wifi::GeneralEvent;
/**
 * @brief Parameters used to connect to an access point.
 */
using ConnectApInfo = helper::Wifi::ConnectApInfo;
/**
 * @brief Parameters used to configure an access-point scan.
 */
using ScanParams = helper::Wifi::ScanParams;
/**
 * @brief Signal strength classification for scanned access points.
 */
using ScanApSignalLevel = helper::Wifi::ScanApSignalLevel;
/**
 * @brief Information returned for one scanned access point.
 */
using ScanApInfo = helper::Wifi::ScanApInfo;
/**
 * @brief Parameters used to configure soft-AP mode.
 */
using SoftApParams = helper::Wifi::SoftApParams;
/**
 * @brief Re-exported soft-AP event identifiers.
 */
using SoftApEvent = helper::Wifi::SoftApEvent;

} // namespace esp_brookesia::service::wifi
