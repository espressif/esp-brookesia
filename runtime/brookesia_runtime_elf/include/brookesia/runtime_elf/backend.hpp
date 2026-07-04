/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>

#include "brookesia/runtime_elf/macro_configs.h"
#include "brookesia/runtime_manager.hpp"

namespace esp_brookesia::runtime::elf {

/**
 * @brief ELF runtime backend registered with the runtime manager.
 */
class Backend final : public IRuntimeBackend {
public:
    /**
     * @brief Get the singleton ELF backend instance.
     *
     * @return ELF backend singleton.
     */
    static Backend &get_instance()
    {
        static Backend instance;
        return instance;
    }

    void set_native_modules(std::vector<NativeModule> modules) override;
    std::expected<void, std::string> init() override;
    void deinit() override;
    std::expected<void, std::string> load_app(AppId id, const AppConfig &config) override;
    std::expected<void, std::string> unload_app(AppId id) override;
    std::expected<void, std::string> start_app(AppId id) override;
    std::expected<void, std::string> stop_app(AppId id) override;
    std::expected<NativeValue, std::string> call_function(
        AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
    ) override;
    AppState get_app_state(AppId id) const override;

private:
    Backend();
    ~Backend() override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::runtime::elf
