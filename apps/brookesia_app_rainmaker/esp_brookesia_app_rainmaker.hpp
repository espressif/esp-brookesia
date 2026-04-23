/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief RainMaker phone app (Squareline UI).
 */
class RainmakerDemo : public systems::phone::App {
public:
    static RainmakerDemo *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    ~RainmakerDemo();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    RainmakerDemo(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;
    bool cleanResource(void) override;

private:
    static RainmakerDemo *_instance;
};

} // namespace esp_brookesia::apps
