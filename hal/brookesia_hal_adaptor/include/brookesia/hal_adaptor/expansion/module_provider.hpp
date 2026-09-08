/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file module_provider.hpp
 * @brief Provider and lease API for hot-pluggable expansion modules.
 */
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"

namespace esp_brookesia::hal::expansion {

/** @brief One raw provider scan sample. */
struct ScanInfo {
    ModuleState state = ModuleState::Unknown;
    std::string type;
    uint32_t board_id = 0;
    std::string board_name;
    uint64_t identity = 0; ///< Provider-private identity/revision used to detect silent swaps.
};

/** @brief Provider requirements returned after a successful claim. */
struct ClaimOptions {
    bool pause_all_scanning = false; ///< Pause every provider while this lease is active.
};

/**
 * @brief Board-specific expansion-slot provider.
 *
 * Providers perform one physical sample per `scan()` call. Periodic scheduling,
 * debounce, state publication, serialization, and ownership are handled by the
 * adaptor runtime.
 */
class ModuleProvider {
public:
    virtual ~ModuleProvider() = default;

    /** @brief Return a process-wide unique provider name. */
    virtual std::string_view get_name() const = 0;

    /** @brief Return stable provider-local slot names. */
    virtual std::vector<std::string> get_slots() const = 0;

    /** @brief Acquire provider resources before the first consumer starts scanning. */
    virtual std::expected<void, std::string> start()
    {
        return {};
    }

    /** @brief Release provider resources after the last consumer or lease exits. */
    virtual std::expected<void, std::string> stop()
    {
        return {};
    }

    /** @brief Take one physical sample of a slot. */
    virtual std::expected<ScanInfo, std::string> scan(std::string_view slot) = 0;

    /** @brief Transfer a ready module from scanning to a consumer. */
    virtual std::expected<ClaimOptions, std::string> claim(
        const ModuleInfo &module, uint64_t scan_identity
    ) = 0;

    /**
     * @brief Return a claimed module to a scan-safe state.
     *
     * The provider must restore a scan-safe hardware state before returning,
     * including when it reports a secondary cleanup error.
     */
    virtual std::expected<void, std::string> release(const ModuleInfo &module) = 0;
};

namespace detail {
class ExpansionRuntime;
}

/** @brief Move-only ownership token for one claimed expansion module. */
class ModuleLease {
public:
    ModuleLease();
    ~ModuleLease();

    ModuleLease(ModuleLease &&other) noexcept;
    ModuleLease &operator=(ModuleLease &&other) noexcept;
    ModuleLease(const ModuleLease &) = delete;
    ModuleLease &operator=(const ModuleLease &) = delete;

    /** @brief Test whether this object owns an active module. */
    explicit operator bool() const noexcept;

    /** @brief Get the active module snapshot owned by this lease. */
    const ModuleInfo &get_info() const noexcept;

    /** @brief Release the module now; the destructor performs the same operation. */
    bool reset(std::string *error_message = nullptr);

private:
    friend class detail::ExpansionRuntime;
    struct Impl;

    explicit ModuleLease(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

/** @brief Register one board-specific provider before the manager starts. */
bool register_module_provider(std::shared_ptr<ModuleProvider> provider);

/** @brief Remove a provider while the manager is stopped. Primarily useful to tests. */
bool unregister_module_provider(std::string_view provider_name);

/**
 * @brief Atomically claim one exact ready snapshot.
 *
 * This call retains the expansion runtime on success. A stale generation fails
 * without invoking the provider.
 */
ModuleLease claim_module(
    std::string_view provider, std::string_view slot, uint64_t expected_generation,
    std::string *error_message = nullptr
);

/**
 * @brief Start scanning, wait for a stable matching ready module, and claim it.
 *
 * Empty provider, slot, or type strings act as wildcards. The temporary runtime
 * reference is transferred to the returned lease on success.
 */
ModuleLease claim_matching(
    std::string_view provider, std::string_view slot, std::string_view type,
    uint32_t timeout_ms = 1200, std::string *error_message = nullptr
);

/** @brief Request one immediate full scan while the runtime is active. */
bool request_rescan();

BROOKESIA_DESCRIBE_STRUCT(ScanInfo, (), (state, type, board_id, board_name, identity));
BROOKESIA_DESCRIBE_STRUCT(ClaimOptions, (), (pause_all_scanning));

} // namespace esp_brookesia::hal::expansion

/**
 * @brief Register a provider during static initialization.
 *
 * The component defining the provider must preserve `symbol_name` with a linker
 * `-u` option when its translation unit is held in a static library.
 */
#define ESP_BROOKESIA_HAL_EXPANSION_REGISTER_PROVIDER(symbol_name, ...) \
    BROOKESIA_PLUGIN_REGISTER_PRE_MAIN_FUNCTION(symbol_name, ([]() { \
        ::esp_brookesia::hal::expansion::register_module_provider((__VA_ARGS__)); \
    }))
