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
 * The caller keeps the HAL interfaces alive for the whole session. This module
 * does not release them and does not register a second LCD callback owner.
 */
struct DisplayLease {
    hal::display::PanelIface *panel = nullptr; ///< Initialized panel. Required.
    hal::display::TouchIface *touch = nullptr; ///< Initialized touch controller. Required for input.
    std::shared_ptr<DesktopDisplay> desktop; ///< Linux renderer; panel and touch are unused when set.
};

/**
 * @brief One GSP UI running on a borrowed panel and touch controller.
 *
 * @note Serialize lifecycle calls on an application task. Do not call stop()
 *       or destroy the session from a GSP event, timer, or render callback.
 *       If stop() fails, keep this session, the callback context, and borrowed
 *       display and bundle resources alive, then retry from the application task.
 *       Destruction terminates the process if the UI cannot be stopped safely.
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
     *                   pointed-to bundle bytes must outlive the session.
     * @param[in] display Borrowed panel and touch. Both must already be initialized.
     * @param[in] on_event Application callback for UI events. May be null. It runs
     *                     on the GSP render task and must not block or stop the session.
     * @param[in] user_ctx Pointer passed through to @p on_event.
     * @return `true` after the UI task has started. If callback registration
     *         fails and cleanup also fails, running() remains true; the caller
     *         must finish stopping the session before releasing borrowed data.
     */
    bool start(
        const esp_gsp_config_t &bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx
    );

    /**
     * @brief Start an owned compiled bundle and retain it until stop succeeds.
     *
     * The bundle remains owned even if callback registration fails and the
     * subsequent stop attempt fails. The caller still owns the display and
     * callback context until stop succeeds.
     */
    bool start(
        std::shared_ptr<const Bundle> bundle, const DisplayLease &display, EventCallback on_event, void *user_ctx
    );

    /**
     * @brief Stop the UI from an application task and release GSP-owned resources.
     *
     * Borrowed HAL devices stay initialized. A session that is already stopped
     * returns `true`. On failure, the handle and all borrowed resources remain
     * owned; retry while keeping the callback context alive. Do not submit
     * more UI updates after a failed stop attempt.
     */
    bool stop();

    /**
     * @brief Whether a UI handle is currently owned by this session.
     */
    bool running() const;

    /**
     * @brief Handle for generated property setters. Null when the session is stopped.
     */
    esp_gsp_handle_t handle() const;

private:
    bool start_impl(
        const esp_gsp_config_t &config, std::shared_ptr<const Bundle> bundle, const DisplayLease &display,
        EventCallback on_event, void *user_ctx
    );

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::gui::gsp
