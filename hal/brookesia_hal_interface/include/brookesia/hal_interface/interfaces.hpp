/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file interfaces.hpp
 * @brief Convenience include for all public HAL interface type headers.
 */
#pragma once

#include "interfaces/audio/codec_player.hpp"
#include "interfaces/audio/codec_recorder.hpp"
#include "interfaces/audio/processor.hpp"
#include "interfaces/display/backlight.hpp"
#include "interfaces/display/panel.hpp"
#include "interfaces/display/touch.hpp"
#include "interfaces/network/connectivity.hpp"
#include "interfaces/network/http_client.hpp"
#include "interfaces/network/sntp_client.hpp"
#include "interfaces/power/battery.hpp"
#include "interfaces/storage/file_system.hpp"
#include "interfaces/storage/key_value.hpp"
#include "interfaces/system/board_info.hpp"
#include "interfaces/system/ota_updater.hpp"
#include "interfaces/video/camera.hpp"
#include "interfaces/video/processor.hpp"
#include "interfaces/wifi/basic.hpp"
#include "interfaces/wifi/softap.hpp"
#include "interfaces/wifi/station.hpp"
