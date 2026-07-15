/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brookesia/app_matter_controller/matter_controller_model.hpp"

// Forward-declare the Matter device-list type from rmaker_matter_controller.
// The actual definition is in <app_rmaker_matter_device_list.h>.
struct matter_device;

namespace esp_brookesia::app::matter_controller {

class MatterBackend {
public:
    virtual ~MatterBackend() = default;

    virtual std::expected<void, std::string> start() = 0;
    virtual void stop() = 0;
    virtual std::expected<void, std::string> tick() = 0;
    virtual bool consume_dirty() = 0;

    virtual const std::vector<ControllerDevice> &devices() const = 0;
    virtual const std::vector<ControllerRoom> &rooms() const = 0;

    virtual std::expected<void, std::string> toggle_device(uint64_t node_id, uint16_t endpoint_id) = 0;
    virtual std::expected<void, std::string> set_brightness(
        uint64_t node_id, uint16_t endpoint_id, int value) = 0;
    virtual std::expected<void, std::string> set_cct(
        uint64_t node_id, uint16_t endpoint_id, int value) = 0;
    virtual std::expected<void, std::string> set_hue(
        uint64_t node_id, uint16_t endpoint_id, int value) = 0;
    virtual std::expected<void, std::string> set_saturation(
        uint64_t node_id, uint16_t endpoint_id, int value) = 0;
    virtual std::expected<void, std::string> assign_room(
        std::string room_name, const std::vector<std::pair<uint64_t, uint16_t>> &device_ids) = 0;
    /**
     * @brief Remove this controller's Matter fabric from a device.
     *
     * The endpoint identifies the selected UI card; removing a Matter node
     * applies to every endpoint belonging to that node.
     */
    virtual std::expected<void, std::string> remove_device(uint64_t node_id, uint16_t endpoint_id) = 0;
    virtual std::expected<void, std::string> reset() = 0;
};

std::unique_ptr<MatterBackend> create_default_matter_backend();

/**
 * @brief Bridge from the app_controller device-list callback to the active
 *        EspMatterBackend singleton.
 *
 * Thread-safe.  Called from the rmaker_matter_controller event task when the
 * cloud API returns an updated Matter device list.
 *
 * @param err      ESP_OK on success, or an error code.
 * @param dev_list Temporary read-only device list (valid only during this call).
 */
void notify_matter_device_list_update(int err, const matter_device *dev_list);

/** Queue a node reachability change from the RainMaker Matter report task. */
void notify_matter_node_reachability(uint64_t node_id, bool online);

/**
 * Queue a scalar Matter attribute report. The update is consumed by
 * MatterBackend::tick() so CHIP/report tasks never mutate UI-owned models.
 */
void notify_matter_attribute_update(
    uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
    uint32_t attribute_id, int64_t value);

} // namespace esp_brookesia::app::matter_controller
