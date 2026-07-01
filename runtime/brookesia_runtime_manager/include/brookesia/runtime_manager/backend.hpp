/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime {

class IRuntimeBackend {
public:
    struct Attributes {
        std::string name;
    };

    IRuntimeBackend(const IRuntimeBackend &) = delete;
    IRuntimeBackend &operator=(const IRuntimeBackend &) = delete;
    IRuntimeBackend(IRuntimeBackend &&) = delete;
    IRuntimeBackend &operator=(IRuntimeBackend &&) = delete;
    virtual ~IRuntimeBackend() = default;

    BackendType get_type() const
    {
        return type_;
    }

    const Attributes &get_attributes() const
    {
        return attributes_;
    }

    virtual void set_native_modules(std::vector<NativeModule> modules) = 0;
    virtual std::expected<void, std::string> init() = 0;
    virtual void deinit() = 0;
    virtual std::expected<void, std::string> load_app(AppId id, const AppConfig &config) = 0;
    virtual std::expected<void, std::string> unload_app(AppId id) = 0;
    virtual std::expected<void, std::string> start_app(AppId id) = 0;
    virtual std::expected<void, std::string> stop_app(AppId id) = 0;
    virtual std::expected<NativeValue, std::string> call_function(
        AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
    ) = 0;
    virtual AppState get_app_state(AppId id) const = 0;

protected:
    IRuntimeBackend(BackendType type, Attributes attributes)
        : attributes_(std::move(attributes))
        , type_(type)
    {}

private:
    Attributes attributes_;
    BackendType type_ = BackendType::Unknown;
};

using RuntimeBackendRegistry = lib_utils::PluginRegistry<IRuntimeBackend>;

} // namespace esp_brookesia::runtime
