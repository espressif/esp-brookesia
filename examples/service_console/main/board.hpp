/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/service_audio.hpp"

bool board_manager_init();

bool board_audio_peripheral_init(esp_brookesia::service::Audio::PeripheralConfig &config);
