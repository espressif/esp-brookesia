/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <atomic>
#include <vector>
#include "brookesia/service_manager/service/manager.hpp"

class GeneralServices {
public:
    static GeneralServices &get_instance()
    {
        static GeneralServices instance;
        return instance;
    }

    bool init(std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler);

    bool is_initialized() const
    {
        return task_scheduler_ != nullptr;
    }

    void init_audio();
    void start_sntp();
    void start_nvs();

private:
    GeneralServices() = default;
    ~GeneralServices() = default;
    GeneralServices(const GeneralServices &) = delete;
    GeneralServices &operator=(const GeneralServices &) = delete;

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    // Keep service bindings to avoid frequent start and stop of services
    std::vector<esp_brookesia::service::ServiceBinding> service_bindings_;
};
