/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/settings_app_internal.hpp"

namespace esp_brookesia::app::settings {

using namespace detail;

SettingsApp::SettingsApp()
    : impl_(std::make_unique<Impl>())
{
    impl_->surface = make_settings_surface();
}

SettingsApp::~SettingsApp()
{
    if (wifi_ui_mailbox_) {
        wifi_ui_mailbox_->close();
    }
    disconnect_sntp_events();
    shutdown_brightness_callbacks();
}

#include "app/lifecycle.ipp"

} // namespace esp_brookesia::app::settings
