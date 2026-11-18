/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <memory>
#include <vector>

#include "brookesia/service_manager/service/manager.hpp"

class GeneralServices {
public:
    static GeneralServices &get_instance()
    {
        static GeneralServices instance;
        return instance;
    }

    bool init();
    bool start_audio_services();

private:
    bool init_audio();

    GeneralServices() = default;
    ~GeneralServices() = default;
    GeneralServices(const GeneralServices &) = delete;
    GeneralServices &operator=(const GeneralServices &) = delete;
};
