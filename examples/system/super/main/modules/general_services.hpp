/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <memory>
#include <vector>

#include "brookesia/lib_utils/task_scheduler.hpp"
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

    bool init_audio();
    bool start_audio_services();
    void start_device();

private:
    GeneralServices() = default;
    ~GeneralServices() = default;
    GeneralServices(const GeneralServices &) = delete;
    GeneralServices &operator=(const GeneralServices &) = delete;

    std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler_;
    // Keep service bindings to avoid frequent start and stop of services
    std::vector<esp_brookesia::service::ServiceBinding> service_bindings_;
    bool audio_initialized_ = false;
    bool audio_services_started_ = false;
};
