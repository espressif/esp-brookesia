/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <memory>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/wifi/types.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

/**
 * @file basic.hpp
 * @brief Declares the basic Wi-Fi controller HAL interface.
 */

namespace esp_brookesia::hal::wifi {

/**
 * @brief Basic Wi-Fi controller interface for shared lifecycle and runtime context.
 */
class BasicIface: public Interface {
public:
    static constexpr const char *NAME = "WifiBasic"; ///< Interface registry name.

    struct RuntimeContext {
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler; ///< Scheduler used for async Wi-Fi work.
        lib_utils::TaskScheduler::Group task_group;               ///< Scheduler group used by the Wi-Fi service.
    };

    struct Callbacks {
        std::function<void(BasicAction)> on_action; ///< Called when a basic action is triggered.
        std::function<void(BasicEvent, bool)> on_event; ///< Called when a basic event happens.
        std::function<void()> on_error; ///< Called when a basic action fails.
    };

    /**
     * @brief Construct a basic Wi-Fi interface.
     */
    BasicIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic interfaces.
     */
    virtual ~BasicIface() = default;

    /**
     * @brief Configure runtime resources and callbacks.
     *
     * @param[in] runtime Runtime context shared with the Wi-Fi backend.
     * @param[in] callbacks Callback hooks used to report basic Wi-Fi state.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool configure(RuntimeContext runtime, Callbacks callbacks) = 0;

    /**
     * @brief Clear all configured callbacks.
     */
    virtual void clear_callbacks() = 0;

    /**
     * @brief Initialize Wi-Fi resources.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool init() = 0;

    /**
     * @brief Deinitialize Wi-Fi resources.
     */
    virtual void deinit() = 0;

    /**
     * @brief Start the Wi-Fi subsystem.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the Wi-Fi subsystem.
     */
    virtual void stop() = 0;

    /**
     * @brief Reset backend runtime data.
     */
    virtual void reset_data() = 0;

    /**
     * @brief Request a basic Wi-Fi action.
     *
     * @param[in] action Action to execute.
     * @param[in] is_force Whether to force the action even if state checks would normally skip it.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool do_action(BasicAction action, bool is_force = false) = 0;

    /**
     * @brief Check whether a basic Wi-Fi action is running.
     *
     * @param[in] action Action to query.
     * @return `true` if the action is running; otherwise `false`.
     */
    virtual bool is_action_running(BasicAction action) = 0;

    /**
     * @brief Check whether a basic Wi-Fi event is ready.
     *
     * @param[in] event Event to query.
     * @return `true` if the event is ready; otherwise `false`.
     */
    virtual bool is_event_ready(BasicEvent event) = 0;
};

} // namespace esp_brookesia::hal::wifi
