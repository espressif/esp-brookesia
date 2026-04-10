/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/hal_interface/storage/fs.hpp"

namespace esp_brookesia::hal {

class GeneralStorageFsImpl: public StorageFsIface {
public:
    GeneralStorageFsImpl();
    ~GeneralStorageFsImpl();

private:
    bool init_spiffs();
    void deinit_spiffs();

    bool init_sdcard();
    void deinit_sdcard();
};

} // namespace esp_brookesia::hal
