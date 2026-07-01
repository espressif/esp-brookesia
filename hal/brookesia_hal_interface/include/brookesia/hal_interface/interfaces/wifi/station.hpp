/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <list>
#include <span>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"

/**
 * @file station.hpp
 * @brief Declares the Wi-Fi station HAL interface.
 */

namespace esp_brookesia::hal::wifi {

/**
 * @brief Wi-Fi station interface for AP connection and scanning.
 */
class StationIface: public Interface {
public:
    static constexpr const char *NAME = "WifiStation"; ///< Interface registry name.

    struct Callbacks {
        std::function<void(StationAction)> on_action; ///< Called when a station action is triggered.
        std::function<void(StationEvent, bool)> on_event; ///< Called when a station event happens.
        std::function<void()> on_error; ///< Called when a station action fails.
        std::function<bool(StationAction)> on_action_requested; ///< Called when backend requests a station action.
        std::function<void(bool)> on_scan_state_changed; ///< Called when periodic scan state changes.
        std::function<void(std::span<const ScanApInfo>)> on_scan_ap_infos_updated; ///< Called with scan results.
        std::function<void(const ConnectApInfo &)> on_last_connected_ap_info_updated; ///< Persist last AP.
        std::function<void(const ConnectApInfoList &)> on_connected_ap_infos_updated; ///< Persist known APs.
    };

    /**
     * @brief Construct a station Wi-Fi interface.
     */
    StationIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic interfaces.
     */
    virtual ~StationIface() = default;

    /**
     * @brief Configure station callbacks.
     *
     * @param[in] callbacks Callback hooks used to report station state and data.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool configure(Callbacks callbacks) = 0;

    /**
     * @brief Clear all configured callbacks.
     */
    virtual void clear_callbacks() = 0;

    /**
     * @brief Request a station action.
     *
     * @param[in] action Action to execute.
     * @param[in] is_force Whether to force the action even if state checks would normally skip it.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool do_action(StationAction action, bool is_force = false) = 0;

    /**
     * @brief Check whether a station action is running.
     *
     * @param[in] action Action to query.
     * @return `true` if the action is running; otherwise `false`.
     */
    virtual bool is_action_running(StationAction action) = 0;

    /**
     * @brief Check whether a station event is ready.
     *
     * @param[in] event Event to query.
     * @return `true` if the event is ready; otherwise `false`.
     */
    virtual bool is_event_ready(StationEvent event) = 0;

    /**
     * @brief Mark whether the target AP is currently connectable.
     *
     * @param[in] connectable Whether the target AP can be used for connection attempts.
     */
    virtual void mark_target_connect_ap_info_connectable(bool connectable) = 0;

    /**
     * @brief Set the target AP connection information.
     *
     * @param[in] ap_info Target AP connection information.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_target_connect_ap_info(const ConnectApInfo &ap_info) = 0;

    /**
     * @brief Set the last connected AP information.
     *
     * @param[in] ap_info Last connected AP information.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_last_connected_ap_info(const ConnectApInfo &ap_info) = 0;

    /**
     * @brief Set the persisted known AP list.
     *
     * @param[in] ap_infos Known AP list.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_connected_ap_infos(const ConnectApInfoList &ap_infos) = 0;

    /**
     * @brief Get the target AP connection information.
     *
     * @return Target AP connection information.
     */
    virtual const ConnectApInfo &get_target_connect_ap_info() const = 0;

    /**
     * @brief Get the last connected AP information.
     *
     * @return Last connected AP information.
     */
    virtual const ConnectApInfo &get_last_connected_ap_info() const = 0;

    /**
     * @brief Get the persisted known AP list.
     *
     * @return Known AP list.
     */
    virtual const ConnectApInfoList &get_connected_ap_infos() const = 0;

    /**
     * @brief Set periodic scan parameters.
     *
     * @param[in] params Scan parameters.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_scan_params(const ScanParams &params) = 0;

    /**
     * @brief Get periodic scan parameters.
     *
     * @return Scan parameters.
     */
    virtual const ScanParams &get_scan_params() const = 0;

    /**
     * @brief Start periodic AP scanning.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool start_scan() = 0;

    /**
     * @brief Stop periodic AP scanning.
     */
    virtual void stop_scan() = 0;
};

} // namespace esp_brookesia::hal::wifi
