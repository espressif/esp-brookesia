/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor/storage/device.hpp"
#include "general_fs_impl.hpp"

namespace esp_brookesia::hal {

bool StorageDevice::probe()
{
    return true;
}

bool StorageDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_general_fs(), false, "Failed to init general FS");
#endif

    return true;
}

void StorageDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
    deinit_general_fs();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
bool StorageDevice::init_general_fs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(GENERAL_FS_IMPL_NAME)) {
        BROOKESIA_LOGD("General FS is already initialized, skip");
        return true;
    }

    std::shared_ptr<GeneralStorageFsImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<GeneralStorageFsImpl>(), false, "Failed to create general FS interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(!iface->get_all_info().empty(), false, "No file system found");

    interfaces_.emplace(GENERAL_FS_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void StorageDevice::deinit_general_fs()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(GENERAL_FS_IMPL_NAME)) {
        BROOKESIA_LOGD("General FS is not initialized, skip");
        return;
    }

    interfaces_.erase(GENERAL_FS_IMPL_NAME);
}
#endif // BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_GENERAL_FS_IMPL
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, StorageDevice, StorageDevice::DEVICE_NAME, StorageDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
