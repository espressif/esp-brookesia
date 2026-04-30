/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_GENERAL_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor/general/device.hpp"
#include "board_info_impl.hpp"

namespace esp_brookesia::hal {

bool GeneralDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool GeneralDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_board_info(), false, "Failed to init board info");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid general interfaces initialized");

    return true;
}

void GeneralDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_GENERAL_ENABLE_BOARD_INFO_IMPL
    deinit_board_info();
#endif
}

bool GeneralDevice::init_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(BOARD_INFO_IMPL_NAME)) {
        BROOKESIA_LOGD("Board info is already initialized, skip");
        return true;
    }

    std::shared_ptr<BoardInfoImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<BoardInfoImpl>(), false, "Failed to create board info interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Invalid board information");

    interfaces_.emplace(BOARD_INFO_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void GeneralDevice::deinit_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(BOARD_INFO_IMPL_NAME)) {
        BROOKESIA_LOGD("Board info is not initialized, skip");
        return;
    }

    interfaces_.erase(BOARD_INFO_IMPL_NAME);
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_GENERAL_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, GeneralDevice, GeneralDevice::DEVICE_NAME, GeneralDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_GENERAL_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
