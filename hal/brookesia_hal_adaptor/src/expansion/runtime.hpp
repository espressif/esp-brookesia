/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/hal_adaptor/expansion/module_provider.hpp"

namespace esp_brookesia::hal::expansion::detail {

class ExpansionRuntime {
public:
    static ExpansionRuntime &get_instance();

    bool register_provider(std::shared_ptr<ModuleProvider> provider);
    bool unregister_provider(std::string_view provider_name);
    bool has_providers() const;

    bool retain_consumer(std::string *error_message = nullptr);
    void release_consumer();

    std::vector<ModuleInfo> get_module_infos() const;
    ModuleManagerIface::EventListenerId add_event_listener(ModuleManagerIface::EventListener listener);
    bool remove_event_listener(ModuleManagerIface::EventListenerId id);

    ModuleLease claim_module(
        std::string_view provider, std::string_view slot, uint64_t expected_generation,
        std::string *error_message
    );
    ModuleLease claim_matching(
        std::string_view provider, std::string_view slot, std::string_view type,
        uint32_t timeout_ms, std::string *error_message
    );
    bool release_lease(const ModuleInfo &active_info, bool pause_all_scanning, std::string *error_message);
    bool request_rescan();

private:
    ExpansionRuntime();
    ~ExpansionRuntime();

    ExpansionRuntime(const ExpansionRuntime &) = delete;
    ExpansionRuntime &operator=(const ExpansionRuntime &) = delete;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::hal::expansion::detail
