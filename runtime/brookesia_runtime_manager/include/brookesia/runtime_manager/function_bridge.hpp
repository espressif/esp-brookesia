/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime {

class RuntimeFunctionProvider {
public:
    RuntimeFunctionProvider() = default;
    RuntimeFunctionProvider(const RuntimeFunctionProvider &) = delete;
    RuntimeFunctionProvider &operator=(const RuntimeFunctionProvider &) = delete;
    virtual ~RuntimeFunctionProvider() = default;

    virtual std::vector<NativeModule> get_native_modules() = 0;
};

using RuntimeFunctionProviderRegistry = lib_utils::PluginRegistry<RuntimeFunctionProvider>;

class RuntimeFunctionBridge {
public:
    class AppContextGuard {
    public:
        AppContextGuard(const AppContextGuard &) = delete;
        AppContextGuard &operator=(const AppContextGuard &) = delete;
        AppContextGuard(AppContextGuard &&other) noexcept;
        AppContextGuard &operator=(AppContextGuard &&other) noexcept;
        ~AppContextGuard();

    private:
        friend class RuntimeFunctionBridge;

        explicit AppContextGuard(AppId id);

        AppId previous_app_id_ = INVALID_APP_ID;
        bool had_previous_app_id_ = false;
        bool active_ = false;
    };

    RuntimeFunctionBridge();
    ~RuntimeFunctionBridge();

    std::expected<void, std::string> discover_registered_providers();
    std::expected<void, std::string> add_module(NativeModule module);
    std::vector<NativeModule> get_modules() const;
    std::vector<NativeModule> get_modules_for_app(AppId id) const;

    AppContextGuard make_app_context_guard(AppId id) const;
    std::expected<AppId, std::string> get_current_app_id() const;
    int64_t get_or_create_context_token(AppId id);
    std::expected<void, std::string> attach_context(int64_t token);
    void detach_context();
    void release_app_context(AppId id);
    void release_all_app_contexts();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::runtime

#define BROOKESIA_RUNTIME_FUNCTION_PROVIDER_REGISTER_WITH_SYMBOL(ProviderType, name, instance_expr, symbol_name) \
    BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL( \
        ::esp_brookesia::runtime::RuntimeFunctionProvider, ProviderType, name, instance_expr, symbol_name \
    )
