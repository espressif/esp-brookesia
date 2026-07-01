/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_storage/macro_configs.h"

namespace esp_brookesia::service {

/**
 * @brief Service facade for namespace-based key-value operations.
 */
class Storage : public ServiceBase {
public:
    /**
     * @brief Helper type used to expose Storage schemas and conversion helpers.
     */
    using Helper = helper::Storage;
    /**
     * @brief Key-value map type used by helper APIs.
     */
    using KeyValueMap = Helper::KeyValueMap;
    using FileSystemCapacity = Helper::FileSystemCapacity;

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton Storage service.
     */
    static Storage &get_instance()
    {
        static Storage instance;
        return instance;
    }

private:
    Storage()
        : ServiceBase({
        .name = Helper::get_name().data(),
        // Storage operations can use NVS-backed HAL implementations that require an internal SRAM stack.
#if BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_STORAGE_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_STORAGE_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_STORAGE_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_STORAGE_WORKER_STACK_SIZE,
                    .stack_in_ext = false,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_STORAGE_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
    })
    {}
    ~Storage() = default;

    bool on_init() override;
    void on_deinit() override;

    std::expected<boost::json::array, std::string> function_list(const std::string &nspace);
    std::expected<void, std::string> function_set(const std::string &nspace, boost::json::object &&key_value_map);
    std::expected<boost::json::object, std::string> function_get(const std::string &nspace, boost::json::array &&keys);
    std::expected<void, std::string> function_erase(const std::string &nspace, boost::json::array &&keys);
    std::expected<boost::json::array, std::string> function_get_file_systems();
    std::expected<boost::json::object, std::string> function_get_file_system_capacity(
        const std::string &mount_point
    );
    std::expected<boost::json::object, std::string> function_make_kv_key(
        boost::json::array &&parts, const std::string &separator
    );
    std::expected<boost::json::object, std::string> function_make_kv_namespace(
        boost::json::array &&parts, const std::string &separator
    );

    hal::InterfaceHandle<hal::storage::KeyValueIface> kv_iface_;
    hal::InterfaceHandle<hal::storage::FileSystemIface> fs_iface_;

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::KVList, std::string,
                function_list(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::KVSet, std::string, boost::json::object,
                function_set(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::KVGet, std::string, boost::json::array,
                function_get(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::KVErase, std::string, boost::json::array,
                function_erase(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetFileSystems,
                function_get_file_systems()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetFileSystemCapacity, std::string,
                function_get_file_system_capacity(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::MakeKVKey, boost::json::array, std::string,
                function_make_kv_key(std::move(PARAM1), PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::MakeKVNamespace, boost::json::array, std::string,
                function_make_kv_namespace(std::move(PARAM1), PARAM2)
            )
        };
    }
};

} // namespace esp_brookesia::service
