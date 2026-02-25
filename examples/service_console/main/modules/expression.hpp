/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <atomic>
#include <vector>
#include "sdkconfig.h"
#include "board.hpp"
#include "brookesia/service_manager/event/registry.hpp"
#include "brookesia/service_manager/service/manager.hpp"

class Expression {
public:
    static Expression &get_instance()
    {
        static Expression instance;
        return instance;
    }

    /**
     * @brief Init expression service
     * @return true if initialized successfully, false otherwise
     */
    bool init(std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler);

    /**
     * @brief Check if expression service is initialized
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const
    {
        return is_initialized_.load();
    }

    void init_emote();

private:
    Expression() = default;
    ~Expression() = default;
    Expression(const Expression &) = delete;
    Expression &operator=(const Expression &) = delete;

    std::atomic<bool> is_initialized_{false};
    DisplayPeripheralConfig display_config_ {};
    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    // Keep service bindings to avoid frequent start and stop of services
    std::vector<esp_brookesia::service::ServiceBinding> service_bindings_;
    // Keep event connections
    std::vector<esp_brookesia::service::EventRegistry::SignalConnection> service_connections_;
};
