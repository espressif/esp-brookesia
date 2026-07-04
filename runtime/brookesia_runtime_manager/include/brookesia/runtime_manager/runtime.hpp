/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/runtime_manager/backend.hpp"
#include "brookesia/runtime_manager/host_bridge.hpp"
#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime {

/**
 * @brief High-level runtime manager that loads applications through the selected backend.
 *
 * Runtime owns backend application records and uses the host bridge to expose native services,
 * functions, and values to JavaScript, Lua, WASM, ELF, or other registered backends.
 */
class Runtime {
public:
    /**
     * @brief Create a runtime manager.
     *
     * @param host_bridge Bridge used by runtime backends to call host-provided functions.
     */
    explicit Runtime(std::shared_ptr<IRuntimeHostBridge> host_bridge = std::make_shared<RuntimeHostBridge>());
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    ~Runtime();

    /**
     * @brief Initialize the runtime manager and prepare registered backends.
     *
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> init();

    /**
     * @brief Deinitialize the runtime manager and unload managed backend state.
     */
    void deinit();

    /**
     * @brief Load an application from runtime configuration.
     *
     * @param config Application configuration including backend type and entry resources.
     * @return Runtime application id on success, or an error string.
     */
    std::expected<AppId, std::string> load_app(const AppConfig &config);

    /**
     * @brief Unload a previously loaded application.
     *
     * @param id Runtime application id returned by load_app().
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> unload_app(AppId id);

    /**
     * @brief Start a loaded application.
     *
     * @param id Runtime application id returned by load_app().
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> start_app(AppId id);

    /**
     * @brief Stop a running application.
     *
     * @param id Runtime application id returned by load_app().
     * @return Empty result on success, or an error string.
     */
    std::expected<void, std::string> stop_app(AppId id);

    /**
     * @brief Invoke a function exported by a loaded runtime application.
     *
     * @param id Runtime application id returned by load_app().
     * @param module_name Module namespace that owns the function.
     * @param function_name Function name to call.
     * @param args Native argument list passed to the function.
     * @return Function result value on success, or an error string.
     */
    std::expected<NativeValue, std::string> call_function(
        AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args = {}
    );

    /**
     * @brief Get a loaded application record by id.
     *
     * @param id Runtime application id returned by load_app().
     * @return Runtime application record when found.
     */
    std::optional<RuntimeApp> get_app(AppId id) const;

    /**
     * @brief List all applications currently known to the runtime manager.
     *
     * @return Snapshot of runtime application records.
     */
    std::vector<RuntimeApp> list_apps() const;

    /**
     * @brief Get the backend type selected by the runtime manager.
     *
     * @return Runtime backend type.
     */
    BackendType get_type() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::runtime
