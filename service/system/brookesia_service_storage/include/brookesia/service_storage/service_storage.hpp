/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

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
    using FileInfo = Helper::FileInfo;
    using FileEntry = Helper::FileEntry;

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
    static std::string get_component_version();

#if BROOKESIA_SERVICE_STORAGE_ENABLE_PRIVATE_SCHEDULER
    static lib_utils::TaskScheduler::StartConfig make_default_task_scheduler_start_config()
    {
        lib_utils::TaskScheduler::StartConfig config{
            .worker_configs = {},
        };
        lib_utils::ThreadConfig worker0;
        worker0.name = std::string(BROOKESIA_SERVICE_STORAGE_WORKER_NAME) + "0";
        worker0.core_id = BROOKESIA_SERVICE_STORAGE_WORKER_0_CORE_ID;
        worker0.priority = BROOKESIA_SERVICE_STORAGE_WORKER_PRIORITY;
        worker0.stack_size = BROOKESIA_SERVICE_STORAGE_WORKER_STACK_SIZE;
        worker0.stack_in_ext = false;
        config.worker_configs.push_back(worker0);
#   if BROOKESIA_SERVICE_STORAGE_WORKER_NUM >= 2
        lib_utils::ThreadConfig worker1;
        worker1.name = std::string(BROOKESIA_SERVICE_STORAGE_WORKER_NAME) + "1";
        worker1.core_id = BROOKESIA_SERVICE_STORAGE_WORKER_1_CORE_ID;
        worker1.priority = BROOKESIA_SERVICE_STORAGE_WORKER_PRIORITY;
        worker1.stack_size = BROOKESIA_SERVICE_STORAGE_WORKER_STACK_SIZE;
        worker1.stack_in_ext = false;
        config.worker_configs.push_back(worker1);
#   endif
        config.worker_poll_interval_ms = BROOKESIA_SERVICE_STORAGE_WORKER_POLL_INTERVAL_MS;
        return config;
    }
#endif // BROOKESIA_SERVICE_STORAGE_ENABLE_PRIVATE_SCHEDULER

    Storage()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .description = "Provide secure file-system and key-value storage operations.",
        .version = get_component_version(),
        // Storage operations can use NVS-backed HAL implementations that require an internal SRAM stack.
#if BROOKESIA_SERVICE_STORAGE_ENABLE_PRIVATE_SCHEDULER
        .task_scheduler_config = make_default_task_scheduler_start_config()
#elif BROOKESIA_SERVICE_MANAGER_SECONDARY_SCHEDULER_ENABLE
        .scheduler_type = ServiceBase::SchedulerType::Secondary
#endif
    })
    {}
    ~Storage() = default;

    bool on_init() override;
    void on_deinit() override;
    void on_function_batch_start(const std::vector<FunctionCall> &calls) override;
    void on_function_batch_end() override;

    std::expected<std::filesystem::path, std::string> resolve_file_system_path(const std::string &path);

    std::expected<boost::json::array, std::string> function_list(const std::string &nspace);
    std::expected<void, std::string> function_set(const std::string &nspace, boost::json::object &&key_value_map);
    std::expected<boost::json::object, std::string> function_get(const std::string &nspace, boost::json::array &&keys);
    std::expected<void, std::string> function_erase(const std::string &nspace, boost::json::array &&keys);
    std::expected<boost::json::array, std::string> function_get_file_systems();
    std::expected<boost::json::object, std::string> function_get_file_system_capacity(
        const std::string &mount_point
    );
    std::expected<boost::json::object, std::string> function_fs_stat(const std::string &path);
    std::expected<boost::json::array, std::string> function_fs_list(const std::string &path);
    std::expected<void, std::string> function_fs_mkdir(const std::string &path);
    std::expected<std::string, std::string> function_fs_read_text(const std::string &path);
    std::expected<double, std::string> function_fs_read(const std::string &path, const RawBuffer &buffer);
    std::expected<void, std::string> function_fs_write_text(const std::string &path, const std::string &data);
    std::expected<void, std::string> function_fs_write(const std::string &path, const RawBuffer &data);
    std::expected<void, std::string> function_fs_remove(const std::string &path);
    std::expected<void, std::string> function_fs_rename(const std::string &from, const std::string &to);
    std::expected<void, std::string> function_fs_copy_tree(
        const std::string &from, const std::string &to, bool overwrite
    );
    std::expected<boost::json::object, std::string> function_make_kv_key(
        boost::json::array &&parts, const std::string &separator
    );
    std::expected<boost::json::object, std::string> function_make_kv_namespace(
        boost::json::array &&parts, const std::string &separator
    );

    hal::InterfaceHandle<hal::storage::KeyValueIface> kv_iface_;
    hal::InterfaceHandle<hal::storage::FileSystemIface> fs_iface_;
    std::vector<std::filesystem::path> file_system_roots_;
    std::unordered_set<std::filesystem::path> validated_batch_directories_;
    bool batch_path_validation_cache_enabled_ = false;

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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FSStat, std::string,
                function_fs_stat(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FSList, std::string,
                function_fs_list(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FSMkdir, std::string,
                function_fs_mkdir(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FSReadText, std::string,
                function_fs_read_text(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::FSRead, std::string, RawBuffer,
                function_fs_read(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::FSWriteText, std::string, std::string,
                function_fs_write_text(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::FSWrite, std::string, RawBuffer,
                function_fs_write(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FSRemove, std::string,
                function_fs_remove(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::FSRename, std::string, std::string,
                function_fs_rename(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_3(
                Helper, Helper::FunctionId::FSCopyTree, std::string, std::string, bool,
                function_fs_copy_tree(PARAM1, PARAM2, PARAM3)
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
