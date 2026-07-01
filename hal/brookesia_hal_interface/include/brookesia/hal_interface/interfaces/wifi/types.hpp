/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"

/**
 * @file types.hpp
 * @brief Declares shared Wi-Fi HAL interface types.
 */

namespace esp_brookesia::hal::wifi {

/**
 * @brief General Wi-Fi control action used by service-level dispatch.
 */
enum class GeneralAction {
    Init,       ///< Initialize Wi-Fi resources.
    Deinit,     ///< Deinitialize Wi-Fi resources.
    Start,      ///< Start Wi-Fi subsystem.
    Stop,       ///< Stop Wi-Fi subsystem.
    Connect,    ///< Connect to configured AP.
    Disconnect, ///< Disconnect from current AP.
    Max,        ///< Sentinel value.
};

/**
 * @brief General Wi-Fi lifecycle event used by service-level dispatch.
 */
enum class GeneralEvent {
    Deinited,     ///< Wi-Fi has been deinitialized.
    Inited,       ///< Wi-Fi has been initialized.
    Stopped,      ///< Wi-Fi has been stopped.
    Started,      ///< Wi-Fi has started.
    Disconnected, ///< Wi-Fi disconnected from AP.
    Connected,    ///< Wi-Fi connected to AP.
    Max,          ///< Sentinel value.
};

/**
 * @brief Basic Wi-Fi controller lifecycle action.
 */
enum class BasicAction {
    Init,   ///< Initialize Wi-Fi resources.
    Deinit, ///< Deinitialize Wi-Fi resources.
    Start,  ///< Start Wi-Fi subsystem.
    Stop,   ///< Stop Wi-Fi subsystem.
    Max,    ///< Sentinel value.
};

/**
 * @brief Basic Wi-Fi controller lifecycle event.
 */
enum class BasicEvent {
    Deinited, ///< Wi-Fi has been deinitialized.
    Inited,   ///< Wi-Fi has been initialized.
    Stopped,  ///< Wi-Fi has been stopped.
    Started,  ///< Wi-Fi has started.
    Max,      ///< Sentinel value.
};

/**
 * @brief Wi-Fi station action.
 */
enum class StationAction {
    Connect,    ///< Connect to configured AP.
    Disconnect, ///< Disconnect from current AP.
    Max,        ///< Sentinel value.
};

/**
 * @brief Wi-Fi station event.
 */
enum class StationEvent {
    Disconnected, ///< Wi-Fi disconnected from AP.
    Connected,    ///< Wi-Fi connected to AP.
    Max,          ///< Sentinel value.
};

/**
 * @brief Target AP connection information.
 */
struct ConnectApInfo {
    /**
     * @brief Construct empty AP connection information.
     */
    ConnectApInfo() = default;

    /**
     * @brief Construct AP connection information.
     *
     * @param[in] ssid AP SSID.
     * @param[in] password AP password.
     */
    ConnectApInfo(const std::string &ssid, const std::string &password = "")
        : ssid(ssid)
        , password(password)
    {
    }

    /**
     * @brief Compare AP connection information for equality.
     *
     * @param[in] other AP connection information to compare with.
     * @return `true` if all fields are equal; otherwise `false`.
     */
    bool operator==(const ConnectApInfo &other) const
    {
        return (ssid == other.ssid) && (password == other.password) && (is_connectable == other.is_connectable);
    }

    /**
     * @brief Compare AP connection information for inequality.
     *
     * @param[in] other AP connection information to compare with.
     * @return `true` if any field is different; otherwise `false`.
     */
    bool operator!=(const ConnectApInfo &other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Check whether this AP information contains a valid SSID.
     *
     * @return `true` if the SSID is not empty; otherwise `false`.
     */
    bool is_valid() const
    {
        return !ssid.empty();
    }

    std::string ssid;             ///< AP SSID.
    std::string password;         ///< AP password.
    bool is_connectable = true;   ///< Whether this AP is considered connectable.
};

/**
 * @brief Parameters for periodic AP scanning.
 */
struct ScanParams {
    /**
     * @brief Compare scan parameters for equality.
     *
     * @param[in] other Scan parameters to compare with.
     * @return `true` if all fields are equal; otherwise `false`.
     */
    bool operator==(const ScanParams &other) const
    {
        return (ap_count == other.ap_count) && (interval_ms == other.interval_ms) && (timeout_ms == other.timeout_ms);
    }

    /**
     * @brief Compare scan parameters for inequality.
     *
     * @param[in] other Scan parameters to compare with.
     * @return `true` if any field is different; otherwise `false`.
     */
    bool operator!=(const ScanParams &other) const
    {
        return !(*this == other);
    }

    size_t ap_count = 20;         ///< Maximum number of AP entries to keep.
    uint32_t interval_ms = 10000; ///< Scan interval in milliseconds.
    uint32_t timeout_ms = 60000;  ///< Total scan timeout in milliseconds.
};

/**
 * @brief Signal-strength bucket for a scanned AP.
 */
enum class ScanApSignalLevel {
    LEVEL_0, ///< RSSI <= -81 dBm.
    LEVEL_1, ///< RSSI in [-80, -71] dBm.
    LEVEL_2, ///< RSSI in [-70, -61] dBm.
    LEVEL_3, ///< RSSI in [-60, -51] dBm.
    LEVEL_4, ///< RSSI >= -50 dBm.
};

/**
 * @brief One scanned AP entry.
 */
struct ScanApInfo {
    /**
     * @brief Convert RSSI to a signal-strength bucket.
     *
     * @param[in] rssi RSSI in dBm.
     * @return Signal-strength bucket.
     */
    static ScanApSignalLevel get_signal_level(int rssi)
    {
        if (rssi <= -81) {
            return ScanApSignalLevel::LEVEL_0;
        }
        if (rssi <= -71) {
            return ScanApSignalLevel::LEVEL_1;
        }
        if (rssi <= -61) {
            return ScanApSignalLevel::LEVEL_2;
        }
        if (rssi <= -51) {
            return ScanApSignalLevel::LEVEL_3;
        }
        return ScanApSignalLevel::LEVEL_4;
    }

    std::string ssid;                         ///< AP SSID.
    bool is_locked = false;                   ///< Whether AP authentication is required.
    int rssi = 0;                             ///< Signal strength in dBm.
    ScanApSignalLevel signal_level = ScanApSignalLevel::LEVEL_0; ///< Bucketed signal level.
    uint8_t channel = 0;                      ///< RF channel.
};

/**
 * @brief SoftAP runtime configuration.
 */
struct SoftApParams {
    /**
     * @brief Compare SoftAP parameters for equality.
     *
     * @param[in] other SoftAP parameters to compare with.
     * @return `true` if all fields are equal; otherwise `false`.
     */
    bool operator==(const SoftApParams &other) const
    {
        if ((ssid != other.ssid) || (password != other.password) ||
                (max_connection != other.max_connection) || (channel.has_value() != other.channel.has_value())) {
            return false;
        }
        if (channel.has_value() && (get_channel() != other.get_channel())) {
            return false;
        }
        return true;
    }

    /**
     * @brief Compare SoftAP parameters for inequality.
     *
     * @param[in] other SoftAP parameters to compare with.
     * @return `true` if any field is different; otherwise `false`.
     */
    bool operator!=(const SoftApParams &other) const
    {
        return !(*this == other);
    }

    /**
     * @brief Check whether a fixed SoftAP channel is set.
     *
     * @return `true` if a fixed channel is configured; otherwise `false`.
     */
    bool has_channel() const
    {
        return channel.has_value();
    }

    /**
     * @brief Get the configured fixed SoftAP channel.
     *
     * @return Fixed SoftAP channel.
     */
    uint8_t get_channel() const
    {
        return channel.value();
    }

    std::string ssid;                         ///< SoftAP SSID.
    std::string password;                     ///< SoftAP password.
    uint8_t max_connection = 4;               ///< Maximum station connections.
    std::optional<uint8_t> channel = std::nullopt; ///< Optional fixed channel.
};

/**
 * @brief SoftAP lifecycle event.
 */
enum class SoftApEvent {
    Started, ///< SoftAP started.
    Stopped, ///< SoftAP stopped.
    Max,     ///< Sentinel value.
};

/**
 * @brief List of AP connection information entries.
 */
using ConnectApInfoList = std::list<ConnectApInfo>;

/**
 * @brief List of scanned AP information entries.
 */
using ScanApInfoList = std::vector<ScanApInfo>;

BROOKESIA_DESCRIBE_ENUM(GeneralAction, Init, Deinit, Start, Stop, Connect, Disconnect, Max);
BROOKESIA_DESCRIBE_ENUM(GeneralEvent, Deinited, Inited, Stopped, Started, Disconnected, Connected, Max);
BROOKESIA_DESCRIBE_ENUM(BasicAction, Init, Deinit, Start, Stop, Max);
BROOKESIA_DESCRIBE_ENUM(BasicEvent, Deinited, Inited, Stopped, Started, Max);
BROOKESIA_DESCRIBE_ENUM(StationAction, Connect, Disconnect, Max);
BROOKESIA_DESCRIBE_ENUM(StationEvent, Disconnected, Connected, Max);
BROOKESIA_DESCRIBE_STRUCT(ConnectApInfo, (), (ssid, password, is_connectable));
BROOKESIA_DESCRIBE_STRUCT(ScanParams, (), (ap_count, interval_ms, timeout_ms));
BROOKESIA_DESCRIBE_ENUM(ScanApSignalLevel, LEVEL_0, LEVEL_1, LEVEL_2, LEVEL_3, LEVEL_4);
BROOKESIA_DESCRIBE_STRUCT(ScanApInfo, (), (ssid, is_locked, rssi, signal_level, channel));
BROOKESIA_DESCRIBE_STRUCT(SoftApParams, (), (ssid, password, max_connection, channel));
BROOKESIA_DESCRIBE_ENUM(SoftApEvent, Started, Stopped, Max);

} // namespace esp_brookesia::hal::wifi
