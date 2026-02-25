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

using GeneralAction = helper::Wifi::GeneralAction;
using GeneralEvent = helper::Wifi::GeneralEvent;
using ScanParams = helper::Wifi::ScanParams;
using ApSignalLevel = helper::Wifi::ApSignalLevel;
using ApInfo = helper::Wifi::ApInfo;
using SoftApParams = helper::Wifi::SoftApParams;
using SoftApEvent = helper::Wifi::SoftApEvent;

struct ConnectApInfo {
    ConnectApInfo() = default;
    ConnectApInfo(const std::string &ssid, const std::string &password = "")
        : ssid(ssid)
        , password(password)
    {}

    bool operator==(const ConnectApInfo &other) const
    {
        return (ssid == other.ssid) && (password == other.password) && (is_connectable == other.is_connectable);
    }

    bool operator!=(const ConnectApInfo &other) const
    {
        return !(*this == other);
    }

    bool is_valid() const
    {
        return !ssid.empty();
    }

    std::string ssid;
    std::string password;
    bool is_connectable = true;
};
BROOKESIA_DESCRIBE_STRUCT(ConnectApInfo, (), (ssid, password, is_connectable));

} // namespace esp_brookesia::service::wifi
