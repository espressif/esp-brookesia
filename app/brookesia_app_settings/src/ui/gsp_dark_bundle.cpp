/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_settings/macro_configs.h"

#if defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#include "settings_dark_gsp.h"

namespace esp_brookesia::app::settings {

esp_gsp_config_t settings_dark_config()
{
    return gsp_settings_dark_config();
}

} // namespace esp_brookesia::app::settings
#endif
