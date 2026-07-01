/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/runtime_manager.hpp"
#include "brookesia/system_core/system/storage.hpp"

namespace esp_brookesia::system::core {

class SystemHostBridge final: public runtime::IRuntimeHostBridge {
public:
    using AppContextGuard = runtime::IRuntimeHostBridge::AppContextGuard;
    using StoragePathResolver = std::function <
                                std::expected<std::string, std::string>(runtime::AppId, const StorageFileLocation &)
                                >;
    using EventDispatcher = std::function<std::expected<void, std::string>(
                                runtime::AppId,
                                std::string,
                                std::string,
                                std::string
                            )>;

    explicit SystemHostBridge(
        std::shared_ptr<runtime::RuntimeFunctionBridge> function_bridge =
            std::make_shared<runtime::RuntimeFunctionBridge>(),
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler = nullptr,
        StoragePathResolver storage_path_resolver = {},
        EventDispatcher event_dispatcher = {}
    );
    ~SystemHostBridge() override;

    void add_module(runtime::NativeModule module) override;
    std::vector<runtime::NativeModule> get_modules() const override;
    std::vector<runtime::NativeModule> get_modules_for_app(runtime::AppId id) const override;
    AppContextGuard make_app_context_guard(runtime::AppId id) const override;
    std::shared_ptr<runtime::RuntimeFunctionBridge> get_function_bridge() const override;
    bool consume_app_finish_request(runtime::AppId id) override;
    void release_app_resources(runtime::AppId id) override;
    void release_all_app_resources() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::system::core
