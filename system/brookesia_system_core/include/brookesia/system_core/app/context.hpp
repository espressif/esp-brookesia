/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/system_core/app/gui_runtime.hpp"
#include "brookesia/system_core/app/system_api.hpp"
#include "brookesia/system_core/app/timer_runtime.hpp"

namespace esp_brookesia::system::core {

class AppContext {
public:
    AppContext(System &system, AppId app_id);

    AppId app_id() const
    {
        return app_id_;
    }

    SystemApi &system_service()
    {
        return system_api_;
    }

    AppTimerRuntime &timer()
    {
        return timer_runtime_;
    }

    AppGuiRuntime &gui()
    {
        return gui_runtime_;
    }

private:
    AppId app_id_ = INVALID_APP_ID;
    SystemApi system_api_;
    AppTimerRuntime timer_runtime_;
    AppGuiRuntime gui_runtime_;
};

} // namespace esp_brookesia::system::core
