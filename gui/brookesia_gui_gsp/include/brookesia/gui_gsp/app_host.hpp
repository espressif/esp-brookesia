/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>

#include "brookesia/gui_gsp/session.hpp"

namespace esp_brookesia::gui::gsp {

/**
 * @brief Loads one compiled bundle and runs it in a Session for a system app.
 *
 * Owns loaded deployable bundles and decompressed bytes until stop() succeeds.
 * The caller keeps the callback context, HAL devices, and borrowed generated
 * config data alive until then, including after a failed start or stop.
 */
class AppHost {
public:
    AppHost();
    ~AppHost();

    AppHost(const AppHost &) = delete;
    AppHost &operator=(const AppHost &) = delete;

    std::expected<void, std::string> start(
        std::span<const std::byte> bytes, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
    );
    std::expected<void, std::string> start(
        const esp_gsp_config_t &config, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
    );
    std::expected<void, std::string> stop();

    bool running() const;
    esp_gsp_handle_t handle() const;
    static AppHost *current();
    const std::shared_ptr<const Bundle> &bundle() const;
    void *event_user_ctx() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace esp_brookesia::gui::gsp
