/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#if !defined(BROOKESIA_GUI_GSP_HOST_TEST)
#   include "brookesia/hal_interface/interfaces/display/panel.hpp"
#   include "brookesia/hal_interface/interfaces/display/touch.hpp"
#   include "esp_gsp.h"
#endif

/**
 * @file session.hpp
 * @brief Starts and stops one GSP UI on borrowed Brookesia display devices.
 */

namespace esp_brookesia::gui::gsp {

class Bundle;
class BundleContract;
class DesktopDisplay;

/**
 * @brief Borrowed display devices for one exclusive GSP session.
 *
 * The lifetime owner keeps the HAL interfaces initialized until GSP stops.
 * This module does not register a second LCD callback owner.
 */
struct DisplayLease {
    hal::display::PanelIface *panel = nullptr; ///< Initialized panel. Required.
    hal::display::TouchIface *touch = nullptr; ///< Initialized touch controller. Required for input.
    std::shared_ptr<DesktopDisplay> desktop; ///< Linux renderer; panel and touch are unused when set.
    std::shared_ptr<void> lifetime; ///< Owner of both HAL interface handles on ESP.
};

/**
 * @brief One GSP UI running on a borrowed panel and touch controller.
 *
 * @note Serialize lifecycle calls on an application task. Do not call stop()
 *       from a GSP event, timer, or render callback.
 *       If stop() fails, the session retains its resources. Destruction transfers
 *       them to a cleanup task that retries until GSP stops.
 */
class Session {
public:
    using EventCallback = void (*)(esp_gsp_handle_t gsp, const esp_gsp_event_t *event, void *user_ctx);

    Session();
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    /**
     * @brief Start a compiled bundle on the borrowed display.
     *
     * @param[in] bundle Configuration from the generated `gsp_*_config()`. The
     *                   pointed-to bundle bytes and generated tables must stay alive
     *                   until the session has stopped.
     * @param[in] display Borrowed panel and touch. Both must already be initialized.
     * @param[in] on_event Application callback for UI events. May be null. It runs
     *                     on the GSP render task and must not block or stop the session.
     * @param[in] user_ctx Pointer passed through to @p on_event.
     * @param[in] callback_owner Owner of @p user_ctx; required when it is non-null.
     * @param[in] bundle_owner Owner of all data referenced by the config. Required;
     *                         static data may use a no-op shared owner.
     * @return `true` after the UI task has started. If callback registration
     *         fails and cleanup also fails, running() remains true; the caller
     *         can retry stop(); the session retains the supplied owners.
     */
    bool start(
        const esp_gsp_config_t &bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx,
        std::shared_ptr<void> callback_owner, std::shared_ptr<const void> bundle_owner
    );

    /**
     * @brief Start an owned compiled bundle and retain it until stop succeeds.
     *
     * The bundle remains owned even if callback registration fails and the
     * subsequent stop attempt fails. The session retains the display and
     * callback owners until stop succeeds.
     */
    bool start(
        std::shared_ptr<const Bundle> bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx,
        std::shared_ptr<void> callback_owner = {}
    );

    /**
     * @brief Stop the UI from an application task and release GSP-owned resources.
     *
     * Borrowed HAL devices stay initialized. A session that is already stopped
     * returns `true`. On failure, the handle and all borrowed resources remain
     * owned; retry the stop. Do not submit
     * more UI updates after a failed stop attempt.
     */
    bool stop();

    /**
     * @brief Whether the UI session is still active.
     *
     * On Linux, bridge failure reports false after bridge and renderer cleanup.
     * The next start(), stop(), or destruction joins the exited bridge thread.
     */
    bool running() const;

    /**
     * @brief Handle for generated property setters. Null when the session is stopped.
     */
    esp_gsp_handle_t handle() const;

private:
    bool start_impl(
        const esp_gsp_config_t &config, std::shared_ptr<const Bundle> bundle, const DisplayLease &display,
        EventCallback on_event, void *user_ctx, std::shared_ptr<void> callback_owner,
        std::shared_ptr<const void> bundle_owner
    );

    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace esp_brookesia::gui::gsp
