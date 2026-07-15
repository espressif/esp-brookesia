/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace esp_brookesia::app::matter_controller {

struct MatterRuntimeState {
    bool available = false;
    bool matter_started = false;
    bool root_cluster_ready = false;
    bool controller_endpoint_ready = false;
    bool rainmaker_started = false;
    bool network_provisioned = false;
    uint16_t controller_endpoint_id = 0;
};

struct MatterPairingPayload {
    bool available = false;
    std::string qr_code;
    std::string manual_pairing_code;
    std::string setup_pin_code;
    uintptr_t qr_native_src = 0;
    int32_t qr_width = 0;
    int32_t qr_height = 0;
};

std::expected<void, std::string> ensure_matter_runtime_started();
MatterRuntimeState get_matter_runtime_state();
std::expected<MatterPairingPayload, std::string> get_matter_pairing_payload();
void release_matter_pairing_qr_image();
std::expected<void, std::string> request_matter_factory_reset();

} // namespace esp_brookesia::app::matter_controller
