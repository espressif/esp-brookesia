/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/json.hpp"

namespace esp_brookesia::gui::parser_detail {

/**
 * @brief Storage for GUI JSON parse trees and document constants.
 *
 * Always returns Boost.JSON's default storage. Memory footprint reductions come
 * from sparse models and fewer copies, not from relocating allocations to PSRAM.
 */
boost::json::storage_ptr make_gui_json_storage();

} // namespace esp_brookesia::gui::parser_detail
