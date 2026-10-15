/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if defined(ESP_PLATFORM)
#   include "sdkconfig.h"
#endif

#if defined(ESP_PLATFORM) && defined(CONFIG_BROOKESIA_GUI_BACKEND_GSP) && CONFIG_BROOKESIA_GUI_BACKEND_GSP
#include "super_shell_dark_gsp.h"

namespace esp_brookesia::system::core {

esp_gsp_config_t super_shell_dark_config()
{
    return gsp_super_shell_dark_config();
}

} // namespace esp_brookesia::system::core
#endif
