/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <vector>

#include "brookesia/runtime_manager/function_bridge.hpp"
#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime {

/**
 * @brief Host bridge used by Runtime to expose native modules and app context.
 */
class IRuntimeHostBridge {
public:
    using AppContextGuard = RuntimeFunctionBridge::AppContextGuard;

    IRuntimeHostBridge() = default;
    IRuntimeHostBridge(const IRuntimeHostBridge &) = delete;
    IRuntimeHostBridge &operator=(const IRuntimeHostBridge &) = delete;
    virtual ~IRuntimeHostBridge() = default;

    virtual void add_module(NativeModule module) = 0;
    virtual std::vector<NativeModule> get_modules() const = 0;
    virtual std::vector<NativeModule> get_modules_for_app(AppId id) const = 0;
    virtual AppContextGuard make_app_context_guard(AppId id) const = 0;
    virtual std::shared_ptr<RuntimeFunctionBridge> get_function_bridge() const = 0;
    virtual bool consume_app_finish_request(AppId id) = 0;
    virtual void release_app_resources(AppId id) = 0;
    virtual void release_all_app_resources() = 0;
};

/**
 * @brief Default host bridge with only basic `brookesia` native helpers.
 */
class RuntimeHostBridge: public IRuntimeHostBridge {
public:
    explicit RuntimeHostBridge(
        std::shared_ptr<RuntimeFunctionBridge> function_bridge = std::make_shared<RuntimeFunctionBridge>()
    );
    ~RuntimeHostBridge() override;

    void add_module(NativeModule module) override;
    std::vector<NativeModule> get_modules() const override;
    std::vector<NativeModule> get_modules_for_app(AppId id) const override;
    AppContextGuard make_app_context_guard(AppId id) const override;
    std::shared_ptr<RuntimeFunctionBridge> get_function_bridge() const override;
    bool consume_app_finish_request(AppId id) override;
    void release_app_resources(AppId id) override;
    void release_all_app_resources() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::runtime
