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
#include "file_system_impl.hpp"
#include "key_value_nvs_impl.hpp"

namespace esp_brookesia::hal {

bool StorageDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> StorageDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
    specs.push_back({storage::FileSystemIface::NAME, FILE_SYSTEM_IFACE_NAME});
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
    specs.push_back({storage::KeyValueIface::NAME, KEY_VALUE_IFACE_NAME});
#endif
    return specs;
}

bool StorageDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_file_system(), false, "Failed to init file system storage");
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_key_value(), false, "Failed to init key-value storage");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid storage interfaces found");

    return true;
}

void StorageDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
    deinit_file_system();
#endif
#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
    deinit_key_value();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL
bool StorageDevice::init_file_system()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(FILE_SYSTEM_IFACE_NAME)) {
        BROOKESIA_LOGD("File system storage is already initialized, skip");
        return true;
    }

    std::shared_ptr<StorageFileSystemImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<StorageFileSystemImpl>(), false, "Failed to create file system storage interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(!iface->get_all_info().empty(), false, "No file system found");

    interfaces_.emplace(FILE_SYSTEM_IFACE_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void StorageDevice::deinit_file_system()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(FILE_SYSTEM_IFACE_NAME)) {
        BROOKESIA_LOGD("File system storage is not initialized, skip");
        return;
    }

    interfaces_.erase(FILE_SYSTEM_IFACE_NAME);
}
#endif // BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
bool StorageDevice::init_key_value()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(KEY_VALUE_IFACE_NAME)) {
        BROOKESIA_LOGD("Key-value storage is already initialized, skip");
        return true;
    }

    std::shared_ptr<StorageKeyValueNvsImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<StorageKeyValueNvsImpl>(), false, "Failed to create key-value storage interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([&iface]() {
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->init(), false, "Failed to initialize KV storage interface");

    interfaces_.emplace(KEY_VALUE_IFACE_NAME, iface);
    iface_delete_guard.release();

    return true;
}

void StorageDevice::deinit_key_value()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto it = interfaces_.find(KEY_VALUE_IFACE_NAME);
    if (it == interfaces_.end()) {
        BROOKESIA_LOGD("Key-value storage is not initialized, skip");
        return;
    }

    if (auto iface = std::dynamic_pointer_cast<StorageKeyValueNvsImpl>(it->second)) {
        iface->deinit();
    }
    interfaces_.erase(it);
}
#endif // BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL

#if BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_FILE_SYSTEM_IMPL || BROOKESIA_HAL_ADAPTOR_STORAGE_ENABLE_KEY_VALUE_IMPL
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, StorageDevice, StorageDevice::DEVICE_NAME, StorageDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_STORAGE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
