/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/hal_adaptor/expansion/module_provider.hpp"
#include "brookesia/hal_custom/expansion/mosaico_module_manager.h"
#include "esp_board_manager.h"
#include "brookesia/hal_adaptor/board_manager.h"
#include "esp_err.h"

namespace esp_brookesia::hal::expansion {

namespace {

constexpr std::string_view PROVIDER_NAME = "mosaico";
constexpr const char *RUNTIME_PIN_DEVICE_NAME = "expansion_runtime_pin";

std::string make_error(std::string_view operation, esp_err_t error)
{
    return std::string(operation) + ": " + esp_err_to_name(error);
}

std::expected<esp_mosaico_expansion_slot_t, std::string> parse_slot(std::string_view slot)
{
    if (slot == "left") {
        return ESP_MOSAICO_EXPANSION_SLOT_LEFT;
    }
    if (slot == "right") {
        return ESP_MOSAICO_EXPANSION_SLOT_RIGHT;
    }
    return std::unexpected("Unknown Mosaico expansion slot: " + std::string(slot));
}

ModuleState map_state(esp_mosaico_expansion_state_t state)
{
    switch (state) {
    case ESP_MOSAICO_EXPANSION_STATE_EMPTY:
        return ModuleState::Empty;
    case ESP_MOSAICO_EXPANSION_STATE_INVALID:
        return ModuleState::Invalid;
    case ESP_MOSAICO_EXPANSION_STATE_UNSUPPORTED:
        return ModuleState::Unsupported;
    case ESP_MOSAICO_EXPANSION_STATE_READY:
        return ModuleState::Ready;
    case ESP_MOSAICO_EXPANSION_STATE_CLAIMED:
        return ModuleState::Active;
    case ESP_MOSAICO_EXPANSION_STATE_ERROR:
        return ModuleState::Error;
    case ESP_MOSAICO_EXPANSION_STATE_UNKNOWN:
    default:
        return ModuleState::Unknown;
    }
}

bool has_valid_identity(esp_mosaico_expansion_state_t state)
{
    return (state == ESP_MOSAICO_EXPANSION_STATE_UNSUPPORTED) ||
           (state == ESP_MOSAICO_EXPANSION_STATE_READY) ||
           (state == ESP_MOSAICO_EXPANSION_STATE_CLAIMED);
}

ScanInfo make_scan_info(const esp_mosaico_expansion_info_t &info)
{
    ScanInfo result = {
        .state = map_state(info.state),
        .type = {},
        .board_id = 0,
        .board_name = {},
    };
    if (has_valid_identity(info.state)) {
        result.type = esp_mosaico_board_type_to_name(info.eeprom.board_type);
        result.board_id = info.eeprom.board_id;
        result.board_name.assign(
            info.eeprom.board_name,
            strnlen(info.eeprom.board_name, ESP_MOSAICO_MODULE_BOARD_NAME_SIZE)
        );
    }
    result.identity = info.generation;
    return result;
}

class MosaicoModuleProvider final: public ModuleProvider {
public:
    std::string_view get_name() const override
    {
        return PROVIDER_NAME;
    }

    std::vector<std::string> get_slots() const override
    {
        return {"left", "right"};
    }

    std::expected<void, std::string> start() override
    {
        esp_brookesia::hal::detail::LifecycleGuard lifecycle_guard;
        if (started_) {
            void *handle = nullptr;
            if ((brookesia_hal_board_manager_get_device_handle(RUNTIME_PIN_DEVICE_NAME, &handle) == ESP_OK) && handle) {
                return {};
            }
            started_ = false;
        }

        /* This child device owns a separate manager dependency reference for
         * exactly as long as the common expansion runtime is active. It avoids
         * borrowing camera_slot_claim's shorter-lived dependency reference. */
        const esp_err_t ret = brookesia_hal_board_manager_init_device_by_name(RUNTIME_PIN_DEVICE_NAME);
        if (ret != ESP_OK) {
            return std::unexpected(make_error("Initialize expansion runtime pin", ret));
        }
        started_ = true;
        return {};
    }

    std::expected<void, std::string> stop() override
    {
        esp_brookesia::hal::detail::LifecycleGuard lifecycle_guard;
        if (!started_) {
            return {};
        }
        const esp_err_t ret = brookesia_hal_board_manager_deinit_device_by_name(RUNTIME_PIN_DEVICE_NAME);
        if (ret != ESP_OK) {
            // A dependency cleanup error may be reported after BM consumed the
            // runtime pin. Keep started_ only when that pin still exists, so a
            // later start cannot skip pending cleanup and falsely enable scans.
            void *handle = nullptr;
            started_ = (brookesia_hal_board_manager_get_device_handle(RUNTIME_PIN_DEVICE_NAME, &handle) == ESP_OK) &&
                       (handle != nullptr);
            return std::unexpected(make_error("Release expansion runtime pin", ret));
        }
        started_ = false;
        return {};
    }

    std::expected<ScanInfo, std::string> scan(std::string_view slot_name) override
    {
        if (!started_) {
            return std::unexpected("Mosaico expansion manager is not started");
        }
        auto slot = parse_slot(slot_name);
        if (!slot) {
            return std::unexpected(slot.error());
        }

        esp_mosaico_expansion_info_t info = {};
        const esp_err_t ret = esp_mosaico_expansion_scan(*slot, &info);
        if (ret != ESP_OK) {
            return std::unexpected(make_error("Scan Mosaico expansion slot", ret));
        }
        return make_scan_info(info);
    }

    std::expected<ClaimOptions, std::string> claim(
        const ModuleInfo &module, uint64_t scan_identity
    ) override
    {
        if (!started_) {
            return std::unexpected("Mosaico expansion manager is not started");
        }
        if ((module.provider != PROVIDER_NAME) || (module.slot != "left") ||
                (module.type != "camera") || (module.state != ModuleState::Ready)) {
            return std::unexpected("The requested module is not a ready left-slot Mosaico camera");
        }

        esp_mosaico_expansion_info_t current = {};
        esp_err_t ret = esp_mosaico_expansion_scan(ESP_MOSAICO_EXPANSION_SLOT_LEFT, &current);
        if (ret != ESP_OK) {
            return std::unexpected(make_error("Recheck Mosaico camera module", ret));
        }
        if ((current.state != ESP_MOSAICO_EXPANSION_STATE_READY) ||
                (current.eeprom.board_type != ESP_MOSAICO_BOARD_TYPE_CAMERA) ||
                (current.eeprom.board_id != module.board_id) ||
                (current.generation != scan_identity)) {
            return std::unexpected("The Mosaico camera changed before it could be claimed");
        }

        const esp_mosaico_camera_slot_config_t camera_config = {
            .slot = ESP_MOSAICO_CAMERA_SLOT,
            .expected_board_type = ESP_MOSAICO_CAMERA_BOARD_TYPE,
            .flash_gpio_num = ESP_MOSAICO_CAMERA_FLASH_GPIO_NUM,
            .flash_off_level = ESP_MOSAICO_CAMERA_FLASH_OFF_LEVEL,
            .settle_time_ms = ESP_MOSAICO_CAMERA_SETTLE_TIME_MS,
        };
        ret = esp_mosaico_camera_slot_acquire(&camera_config);
        if (ret != ESP_OK) {
            return std::unexpected(make_error("Acquire Mosaico camera wiring", ret));
        }
        return ClaimOptions{.pause_all_scanning = true};
    }

    std::expected<void, std::string> release(const ModuleInfo &module) override
    {
        if ((module.provider != PROVIDER_NAME) || (module.slot != "left") ||
                (module.type != "camera")) {
            return std::unexpected("The active module is not a left-slot Mosaico camera");
        }
        const esp_err_t ret = esp_mosaico_camera_slot_release(ESP_MOSAICO_EXPANSION_SLOT_LEFT);
        if (ret != ESP_OK) {
            return std::unexpected(make_error("Release Mosaico camera wiring", ret));
        }
        return {};
    }

private:
    bool started_ = false;
};

} // namespace

ESP_BROOKESIA_HAL_EXPANSION_REGISTER_PROVIDER(
    MOSAICO_EXPANSION_PROVIDER_PLUGIN_SYMBOL, std::make_shared<MosaicoModuleProvider>()
);

} // namespace esp_brookesia::hal::expansion
