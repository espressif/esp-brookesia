/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_BOARD_INFO_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "board_info_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL
#include "esp_board_manager_includes.h"

namespace esp_brookesia::hal {

namespace {
std::string to_string_or_empty(const char *value)
{
    return (value != nullptr) ? value : "";
}

BoardInfoIface::Info generate_info()
{
    esp_board_info_t board_info = {};
    auto ret = esp_board_manager_get_board_info(&board_info);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, {}, "Failed to get board info");

    return BoardInfoIface::Info {
        .name = to_string_or_empty(board_info.name),
        .chip = to_string_or_empty(board_info.chip),
        .version = to_string_or_empty(board_info.version),
        .description = to_string_or_empty(board_info.description),
        .manufacturer = to_string_or_empty(board_info.manufacturer),
    };
}
} // namespace

BoardInfoImpl::BoardInfoImpl()
    : BoardInfoIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Info: %1%", get_info());
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL
