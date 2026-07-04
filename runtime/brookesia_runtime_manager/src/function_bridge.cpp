/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_manager/function_bridge.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

#include "brookesia/runtime_manager/macro_configs.h"
#if !BROOKESIA_RUNTIME_MANAGER_FUNCTION_BRIDGE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::runtime {
namespace {

thread_local std::optional<AppId> current_app_id;

} // namespace

class RuntimeFunctionBridge::Impl {
public:
    std::expected<void, std::string> add_module(NativeModule module);
    std::expected<void, std::string> discover_registered_providers();
    std::vector<NativeModule> get_modules() const;
    std::vector<NativeModule> get_modules_for_app(const RuntimeFunctionBridge &bridge, AppId id) const;
    int64_t get_or_create_context_token(AppId id);
    std::expected<void, std::string> attach_context(int64_t token);
    void release_app_context(AppId id);
    void release_all_app_contexts();

    mutable std::mutex mutex_;
    std::vector<NativeModule> modules_;
    std::set<std::string> discovered_providers_;
    std::map<int64_t, AppId> token_to_app_id_;
    std::map<AppId, int64_t> app_id_to_token_;
    int64_t next_token_id_ = 1;
};

std::expected<void, std::string> RuntimeFunctionBridge::Impl::add_module(NativeModule module)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: module(%1%), function_count(%2%)", module.name, module.functions.size());

    if (module.name.empty()) {
        return std::unexpected("Runtime native module name is empty");
    }

    std::set<std::string> function_names;
    for (const auto &function : module.functions) {
        if (function.name.empty()) {
            return std::unexpected("Runtime native function name is empty");
        }
        if (!function.function && !function.async_function) {
            return std::unexpected("Runtime native function handler is null: " + module.name + "." + function.name);
        }
        if (function.function && function.async_function) {
            return std::unexpected(
                       "Runtime native function cannot be both sync and async: " + module.name + "." + function.name
                   );
        }
        if (!function_names.insert(function.name).second) {
            BROOKESIA_LOGW(
                "Runtime native function is duplicated: module(%1%), function(%2%)", module.name, function.name
            );
            return std::unexpected("Runtime native function is duplicated: " + module.name + "." + function.name);
        }
    }

    std::lock_guard lock(mutex_);
    auto module_it = std::find_if(
                         modules_.begin(), modules_.end(),
    [&module](const NativeModule & item) {
        return item.name == module.name;
    }
                     );
    if (module_it != modules_.end()) {
        BROOKESIA_LOGW("Runtime native module is duplicated: module(%1%)", module.name);
        return std::unexpected("Runtime native module is duplicated: " + module.name);
    }

    BROOKESIA_LOGD("Runtime native module added: module(%1%), functions(%2%)", module.name, module.functions.size());
    modules_.push_back(std::move(module));
    return {};
}

std::expected<void, std::string> RuntimeFunctionBridge::Impl::discover_registered_providers()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto providers = RuntimeFunctionProviderRegistry::get_all_instances();
    for (auto &[name, provider] : providers) {
        if (!provider) {
            BROOKESIA_LOGW("Runtime function provider plugin is null: name(%1%)", name);
            continue;
        }
        if (!discovered_providers_.insert(name).second) {
            continue;
        }

        auto modules = provider->get_native_modules();
        BROOKESIA_LOGD(
            "Runtime function provider discovered: name(%1%), module_count(%2%)",
            name, modules.size()
        );
        for (auto &module : modules) {
            auto result = add_module(std::move(module));
            if (!result) {
                BROOKESIA_LOGW(
                    "Runtime function provider registration failed: provider(%1%), error(%2%)",
                    name, result.error()
                );
                return result;
            }
        }
    }
    return {};
}

std::vector<NativeModule> RuntimeFunctionBridge::Impl::get_modules() const
{
    std::lock_guard lock(mutex_);
    return modules_;
}

std::vector<NativeModule> RuntimeFunctionBridge::Impl::get_modules_for_app(
    const RuntimeFunctionBridge &bridge, AppId id
) const
{
    std::vector<NativeModule> modules;
    std::lock_guard lock(mutex_);
    modules.reserve(modules_.size());
    for (const auto &source_module : modules_) {
        NativeModule module;
        module.name = source_module.name;
        module.functions.reserve(source_module.functions.size());
        for (const auto &source_function : source_module.functions) {
            NativeFunctionSpec function;
            function.name = source_function.name;
            if (source_function.function) {
                function.function = [&bridge, id, native_function = source_function.function](const NativeArgs & args) {
                    auto app_context = bridge.make_app_context_guard(id);
                    return native_function(args);
                };
            }
            if (source_function.async_function) {
                function.async_function =
                    [&bridge, id, native_function = source_function.async_function](
                        const NativeArgs & args, NativeAsyncCallback callback
                ) {
                    auto app_context = bridge.make_app_context_guard(id);
                    native_function(args, std::move(callback));
                };
            }
            module.functions.push_back(std::move(function));
        }
        modules.push_back(std::move(module));
    }
    return modules;
}

int64_t RuntimeFunctionBridge::Impl::get_or_create_context_token(AppId id)
{
    std::lock_guard lock(mutex_);
    auto it = app_id_to_token_.find(id);
    if (it != app_id_to_token_.end()) {
        return it->second;
    }

    const int64_t token = next_token_id_++;
    app_id_to_token_.emplace(id, token);
    token_to_app_id_.emplace(token, id);
    return token;
}

std::expected<void, std::string> RuntimeFunctionBridge::Impl::attach_context(int64_t token)
{
    std::lock_guard lock(mutex_);
    auto it = token_to_app_id_.find(token);
    if (it == token_to_app_id_.end()) {
        BROOKESIA_LOGW("Attach app context failed: invalid token(%1%)", token);
        return std::unexpected("Invalid runtime app context token");
    }
    current_app_id = it->second;
    return {};
}

void RuntimeFunctionBridge::Impl::release_app_context(AppId id)
{
    std::lock_guard lock(mutex_);
    auto token_it = app_id_to_token_.find(id);
    if (token_it != app_id_to_token_.end()) {
        token_to_app_id_.erase(token_it->second);
        app_id_to_token_.erase(token_it);
    }
}

void RuntimeFunctionBridge::Impl::release_all_app_contexts()
{
    std::lock_guard lock(mutex_);
    token_to_app_id_.clear();
    app_id_to_token_.clear();
}

RuntimeFunctionBridge::AppContextGuard::AppContextGuard(AppId id)
    : previous_app_id_(current_app_id.value_or(INVALID_APP_ID))
    , had_previous_app_id_(current_app_id.has_value())
    , active_(true)
{
    current_app_id = id;
}

RuntimeFunctionBridge::AppContextGuard::AppContextGuard(AppContextGuard &&other) noexcept
    : previous_app_id_(other.previous_app_id_)
    , had_previous_app_id_(other.had_previous_app_id_)
    , active_(other.active_)
{
    other.active_ = false;
}

RuntimeFunctionBridge::AppContextGuard &RuntimeFunctionBridge::AppContextGuard::operator=(
    AppContextGuard &&other
) noexcept
{
    if (this != &other) {
        if (active_) {
            if (had_previous_app_id_) {
                current_app_id = previous_app_id_;
            } else {
                current_app_id.reset();
            }
        }
        previous_app_id_ = other.previous_app_id_;
        had_previous_app_id_ = other.had_previous_app_id_;
        active_ = other.active_;
        other.active_ = false;
    }
    return *this;
}

RuntimeFunctionBridge::AppContextGuard::~AppContextGuard()
{
    if (!active_) {
        return;
    }
    if (had_previous_app_id_) {
        current_app_id = previous_app_id_;
    } else {
        current_app_id.reset();
    }
}

RuntimeFunctionBridge::RuntimeFunctionBridge()
    : impl_(std::make_unique<Impl>())
{}

RuntimeFunctionBridge::~RuntimeFunctionBridge() = default;

std::expected<void, std::string> RuntimeFunctionBridge::discover_registered_providers()
{
    return impl_->discover_registered_providers();
}

std::expected<void, std::string> RuntimeFunctionBridge::add_module(NativeModule module)
{
    return impl_->add_module(std::move(module));
}

std::vector<NativeModule> RuntimeFunctionBridge::get_modules() const
{
    return impl_->get_modules();
}

std::vector<NativeModule> RuntimeFunctionBridge::get_modules_for_app(AppId id) const
{
    return impl_->get_modules_for_app(*this, id);
}

RuntimeFunctionBridge::AppContextGuard RuntimeFunctionBridge::make_app_context_guard(AppId id) const
{
    return AppContextGuard(id);
}

std::expected<AppId, std::string> RuntimeFunctionBridge::get_current_app_id() const
{
    if (!current_app_id || (current_app_id.value() == INVALID_APP_ID)) {
        return std::unexpected("No active runtime app context");
    }
    return current_app_id.value();
}

int64_t RuntimeFunctionBridge::get_or_create_context_token(AppId id)
{
    return impl_->get_or_create_context_token(id);
}

std::expected<void, std::string> RuntimeFunctionBridge::attach_context(int64_t token)
{
    return impl_->attach_context(token);
}

void RuntimeFunctionBridge::detach_context()
{
    current_app_id.reset();
}

void RuntimeFunctionBridge::release_app_context(AppId id)
{
    impl_->release_app_context(id);
}

void RuntimeFunctionBridge::release_all_app_contexts()
{
    impl_->release_all_app_contexts();
}

} // namespace esp_brookesia::runtime
