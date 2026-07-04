/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file sntp_client.hpp
 * @brief Declares the SNTP HAL interface.
 */

namespace esp_brookesia::hal::network {

/**
 * @brief Platform SNTP control interface.
 */
class SntpClientIface: public Interface {
public:
    static constexpr const char *NAME = "NetworkSntpClient";  ///< Interface registry name.

    /**
     * @brief SNTP initialization configuration.
     */
    struct Config {
        std::vector<std::string> servers; ///< Static NTP servers used by the platform SNTP backend.
        std::string timezone;             ///< POSIX timezone string applied to the platform runtime.
        bool use_dhcp = true;             ///< Whether the backend may accept NTP servers from DHCP.
    };

    /**
     * @brief Construct an SNTP protocol interface.
     */
    SntpClientIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic protocol interfaces.
     */
    virtual ~SntpClientIface() = default;

    /**
     * @brief Initialize the SNTP backend.
     *
     * @param[in] config Backend configuration.
     * @return true on success, otherwise false.
     */
    virtual bool init(const Config &config) = 0;

    /**
     * @brief Deinitialize the SNTP backend.
     */
    virtual void deinit() = 0;

    /**
     * @brief Start SNTP time synchronization.
     *
     * @return true on success, otherwise false.
     */
    virtual bool start() = 0;

    /**
     * @brief Stop SNTP time synchronization without clearing persisted configuration.
     */
    virtual void stop() = 0;

    /**
     * @brief Check whether the backend is initialized.
     *
     * @return true if initialized, otherwise false.
     */
    virtual bool is_initialized() const = 0;

    /**
     * @brief Check whether the backend is running.
     *
     * @return true if running, otherwise false.
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Wait for a time synchronization notification.
     *
     * @param[in] timeout_ms Wait timeout in milliseconds.
     * @return true if time is synchronized, otherwise false.
     */
    virtual bool wait_time_sync(uint32_t timeout_ms) = 0;

    /**
     * @brief Check whether time has been synchronized by this backend.
     *
     * @return true if synchronized, otherwise false.
     */
    virtual bool is_time_synced() const = 0;

    /**
     * @brief Set static NTP servers for the next initialization.
     *
     * @param[in] servers Server list.
     * @return true on success, otherwise false.
     */
    virtual bool set_servers(const std::vector<std::string> &servers) = 0;

    /**
     * @brief Get configured static NTP servers.
     *
     * @return Server list.
     */
    virtual std::vector<std::string> get_servers() const = 0;

    /**
     * @brief Get the maximum number of static NTP servers supported by the backend.
     *
     * @return Maximum server count.
     */
    virtual std::size_t get_max_servers() const = 0;

    /**
     * @brief Apply the runtime timezone.
     *
     * @param[in] timezone POSIX timezone string.
     * @return true on success, otherwise false.
     */
    virtual bool set_timezone(const std::string &timezone) = 0;

    /**
     * @brief Get the configured runtime timezone.
     *
     * @return POSIX timezone string.
     */
    virtual std::string get_timezone() const = 0;

    /**
     * @brief Get the most recent backend error.
     *
     * @return Human-readable error string.
     */
    const std::string &get_last_error() const
    {
        return last_error_;
    }

protected:
    std::string last_error_;
};

BROOKESIA_DESCRIBE_STRUCT(SntpClientIface::Config, (), (servers, timezone, use_dhcp));

} // namespace esp_brookesia::hal::network
