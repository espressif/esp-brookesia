/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"

/**
 * @file softap.hpp
 * @brief Declares the Wi-Fi SoftAP HAL interface.
 */

namespace esp_brookesia::hal::wifi {

/**
 * @brief Wi-Fi SoftAP interface for AP mode and provisioning.
 */
class SoftApIface: public Interface {
public:
    static constexpr const char *NAME = "WifiSoftAp"; ///< Interface registry name.

    struct Callbacks {
        std::function<void(SoftApEvent)> on_event; ///< Called when a SoftAP event happens.
        std::function<void(const SoftApParams &)> on_params_updated; ///< Persist backend-updated params.
        std::function<bool(StationAction)> on_station_action_requested; ///< Called when SoftAP needs station action.
    };

    /**
     * @brief Construct a SoftAP Wi-Fi interface.
     */
    SoftApIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic interfaces.
     */
    virtual ~SoftApIface() = default;

    /**
     * @brief Configure SoftAP callbacks.
     *
     * @param[in] callbacks Callback hooks used to report SoftAP state and requests.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool configure(Callbacks callbacks) = 0;

    /**
     * @brief Clear all configured callbacks.
     */
    virtual void clear_callbacks() = 0;

    /**
     * @brief Set SoftAP runtime parameters.
     *
     * @param[in] params SoftAP parameters.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_params(const SoftApParams &params) = 0;

    /**
     * @brief Get SoftAP runtime parameters.
     *
     * @return SoftAP parameters.
     */
    virtual const SoftApParams &get_params() const = 0;

    /**
     * @brief Start SoftAP mode.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool start() = 0;

    /**
     * @brief Stop SoftAP mode.
     */
    virtual void stop() = 0;

    /**
     * @brief Start SoftAP provisioning.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool start_provision() = 0;

    /**
     * @brief Stop SoftAP provisioning.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool stop_provision() = 0;
};

} // namespace esp_brookesia::hal::wifi
