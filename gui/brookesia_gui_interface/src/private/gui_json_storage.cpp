/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/gui_json_storage.hpp"

namespace esp_brookesia::gui::parser_detail {

boost::json::storage_ptr make_gui_json_storage()
{
    // Universal default allocator. Preferring PSRAM does not reduce footprint and
    // is unavailable on chips without SPIRAM; keep Boost.JSON's normal path.
    return {};
}

} // namespace esp_brookesia::gui::parser_detail
