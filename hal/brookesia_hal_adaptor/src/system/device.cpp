/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor/system/device.hpp"
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
#   include "board_info_impl.hpp"
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
#   include "ota_updater_impl.hpp"
#endif

namespace esp_brookesia::hal {

bool SystemDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

std::vector<InterfaceSpec> SystemDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
    specs.push_back({system::BoardInfoIface::NAME, BOARD_INFO_IFACE_NAME});
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    specs.push_back({system::OtaUpdaterIface::NAME, OTA_UPDATER_IFACE_NAME});
#endif
    return specs;
}

bool SystemDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_board_info(), false, "Failed to init board info");
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_ota_updater(), false, "Failed to init OTA updater");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid system interfaces initialized");

    return true;
}

void SystemDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
    deinit_board_info();
#endif
#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
    deinit_ota_updater();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_BOARD_INFO_IMPL
bool SystemDevice::init_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(BOARD_INFO_IFACE_NAME)) {
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

    interfaces_.emplace(BOARD_INFO_IFACE_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void SystemDevice::deinit_board_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(BOARD_INFO_IFACE_NAME)) {
        BROOKESIA_LOGD("Board info is not initialized, skip");
        return;
    }

    interfaces_.erase(BOARD_INFO_IFACE_NAME);
}
#endif

#if BROOKESIA_HAL_ADAPTOR_SYSTEM_ENABLE_OTA_UPDATER_IMPL
bool SystemDevice::init_ota_updater()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(OTA_UPDATER_IFACE_NAME)) {
        BROOKESIA_LOGD("OTA updater is already initialized, skip");
        return true;
    }

    std::shared_ptr<OtaUpdaterAdaptorIface> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<OtaUpdaterAdaptorIface>(), false, "Failed to create OTA updater interface"
    );
    interfaces_.emplace(OTA_UPDATER_IFACE_NAME, iface);

    return true;
}

void SystemDevice::deinit_ota_updater()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(OTA_UPDATER_IFACE_NAME)) {
        BROOKESIA_LOGD("OTA updater is not initialized, skip");
        return;
    }

    interfaces_.erase(OTA_UPDATER_IFACE_NAME);
}
#endif

#if BROOKESIA_HAL_ADAPTOR_ENABLE_SYSTEM_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, SystemDevice, SystemDevice::DEVICE_NAME, SystemDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_SYSTEM_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
