/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_manager/host_bridge.hpp"

#include <cstddef>
#include <cstdio>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "brookesia/runtime_manager/macro_configs.h"
#if !BROOKESIA_RUNTIME_MANAGER_HOST_BRIDGE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::runtime {
namespace {

int64_t native_value_to_handle(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> int64_t {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            return static_cast<int64_t>(item);
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item ? 1 : 0;
        } else
        {
            return 0;
        }
    },
    value
           );
}

NativeResult make_success()
{
    return NativeValue{true};
}

std::string native_value_to_print_string(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> std::string {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, std::string>)
        {
            return item;
        } else if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return std::to_string(item);
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), "%g", item);
            return buffer;
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return item ? "true" : "false";
        } else
        {
            return "nil";
        }
    },
    value
           );
}

} // namespace

class RuntimeHostBridge::Impl {
public:
    explicit Impl(std::shared_ptr<RuntimeFunctionBridge> function_bridge)
        : function_bridge_(std::move(function_bridge))
    {
        auto result = function_bridge_->add_module(make_brookesia_module());
        if (!result) {
            BROOKESIA_LOGW("Failed to add built-in Brookesia runtime module: %1%", result.error());
        }
    }

    NativeModule make_brookesia_module();
    NativeResult current_app_context(const NativeArgs &args);
    NativeResult finish_app(const NativeArgs &args);
    NativeResult attach_app_context(const NativeArgs &args);
    NativeResult detach_app_context(const NativeArgs &args);
    NativeResult print(const NativeArgs &args);
    void request_finish_app(AppId id);
    bool consume_app_finish_request(AppId id);
    void release_app_resources(AppId id);
    void release_all_app_resources();

    std::shared_ptr<RuntimeFunctionBridge> function_bridge_;
    mutable std::mutex mutex_;
    std::set<AppId> finish_requests_;
};

void RuntimeHostBridge::Impl::request_finish_app(AppId id)
{
    std::lock_guard lock(mutex_);
    finish_requests_.insert(id);
    BROOKESIA_LOGD("App finish requested: app_id(%1%)", id);
}

bool RuntimeHostBridge::Impl::consume_app_finish_request(AppId id)
{
    std::lock_guard lock(mutex_);
    return finish_requests_.erase(id) > 0;
}

void RuntimeHostBridge::Impl::release_app_resources(AppId id)
{
    {
        std::lock_guard lock(mutex_);
        finish_requests_.erase(id);
    }
    function_bridge_->release_app_context(id);
}

void RuntimeHostBridge::Impl::release_all_app_resources()
{
    {
        std::lock_guard lock(mutex_);
        finish_requests_.clear();
    }
    function_bridge_->release_all_app_contexts();
}

NativeResult RuntimeHostBridge::Impl::current_app_context(const NativeArgs &args)
{
    (void)args;
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    return NativeValue{function_bridge_->get_or_create_context_token(app_id.value())};
}

NativeResult RuntimeHostBridge::Impl::finish_app(const NativeArgs &args)
{
    (void)args;
    auto app_id = function_bridge_->get_current_app_id();
    if (!app_id) {
        return std::unexpected(app_id.error());
    }
    request_finish_app(app_id.value());
    return make_success();
}

NativeResult RuntimeHostBridge::Impl::attach_app_context(const NativeArgs &args)
{
    if (args.empty()) {
        return std::unexpected("brookesia.attach_app_context(token) requires a context token");
    }
    const int64_t token = native_value_to_handle(args[0]);
    auto result = function_bridge_->attach_context(token);
    if (!result) {
        return std::unexpected(result.error());
    }
    return make_success();
}

NativeResult RuntimeHostBridge::Impl::detach_app_context(const NativeArgs &args)
{
    (void)args;
    function_bridge_->detach_context();
    return make_success();
}

NativeResult RuntimeHostBridge::Impl::print(const NativeArgs &args)
{
    std::string message;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            message += ' ';
        }
        message += native_value_to_print_string(args[i]);
    }
    BROOKESIA_LOGI("%1%", message);
    return NativeValue{};
}

NativeModule RuntimeHostBridge::Impl::make_brookesia_module()
{
    NativeModule module;
    module.name = "brookesia";

    auto current_app_context_func = [this](const NativeArgs & args) {
        return current_app_context(args);
    };
    auto finish_app_func = [this](const NativeArgs & args) {
        return finish_app(args);
    };
    auto attach_app_context_func = [this](const NativeArgs & args) {
        return attach_app_context(args);
    };
    auto detach_app_context_func = [this](const NativeArgs & args) {
        return detach_app_context(args);
    };
    auto print_func = [this](const NativeArgs & args) {
        return print(args);
    };

    module.functions = {
        {.name = "current_app_context", .function = std::move(current_app_context_func), .async_function = nullptr},
        {.name = "finish_app", .function = std::move(finish_app_func), .async_function = nullptr},
        {.name = "attach_app_context", .function = std::move(attach_app_context_func), .async_function = nullptr},
        {.name = "detach_app_context", .function = std::move(detach_app_context_func), .async_function = nullptr},
        {.name = "print", .function = std::move(print_func), .async_function = nullptr},
    };
    return module;
}

RuntimeHostBridge::RuntimeHostBridge(std::shared_ptr<RuntimeFunctionBridge> function_bridge)
    : impl_(std::make_unique<Impl>(
                function_bridge ? std::move(function_bridge) : std::make_shared<RuntimeFunctionBridge>()
            ))
{}

RuntimeHostBridge::~RuntimeHostBridge() = default;

void RuntimeHostBridge::add_module(NativeModule module)
{
    auto result = impl_->function_bridge_->add_module(std::move(module));
    if (!result) {
        BROOKESIA_LOGW("Add native module failed: %1%", result.error());
    }
}

std::vector<NativeModule> RuntimeHostBridge::get_modules() const
{
    return impl_->function_bridge_->get_modules();
}

std::vector<NativeModule> RuntimeHostBridge::get_modules_for_app(AppId id) const
{
    return impl_->function_bridge_->get_modules_for_app(id);
}

RuntimeHostBridge::AppContextGuard RuntimeHostBridge::make_app_context_guard(AppId id) const
{
    return impl_->function_bridge_->make_app_context_guard(id);
}

std::shared_ptr<RuntimeFunctionBridge> RuntimeHostBridge::get_function_bridge() const
{
    return impl_->function_bridge_;
}

bool RuntimeHostBridge::consume_app_finish_request(AppId id)
{
    return impl_->consume_app_finish_request(id);
}

void RuntimeHostBridge::release_app_resources(AppId id)
{
    impl_->release_app_resources(id);
}

void RuntimeHostBridge::release_all_app_resources()
{
    impl_->release_all_app_resources();
}

} // namespace esp_brookesia::runtime
