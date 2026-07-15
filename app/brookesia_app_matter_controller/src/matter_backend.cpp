/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "matter_backend.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "private/utils.hpp"

#if __has_include(<app_rmaker_matter_device_list.h>)
#include <app_rmaker_matter_device_list.h>
#endif

#include <esp_matter_core.h>

#if defined(CONFIG_ESP_MATTER_CONTROLLER_ENABLE) && \
    __has_include(<esp_matter_controller_cluster_command.h>) && \
    __has_include(<esp_matter_controller_client.h>) && \
    __has_include(<app_rmaker_matter_device_list.h>) && \
    __has_include(<clusters/ColorControl/AttributeIds.h>) && __has_include(<clusters/ColorControl/Commands.h>) && \
    __has_include(<clusters/LevelControl/AttributeIds.h>) && __has_include(<clusters/LevelControl/Commands.h>) && \
    __has_include(<clusters/OnOff/AttributeIds.h>) && __has_include(<clusters/OnOff/Commands.h>) && \
    __has_include(<esp_matter_endpoint.h>)
#define BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND 1
#include <app_rmaker_matter_device_list.h>
#include <clusters/ColorControl/AttributeIds.h>
#include <clusters/ColorControl/Commands.h>
#include <clusters/ColorControl/Enums.h>
#include <clusters/LevelControl/AttributeIds.h>
#include <clusters/LevelControl/Commands.h>
#include <clusters/OnOff/AttributeIds.h>
#include <clusters/OnOff/Commands.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_matter_endpoint.h>
#endif

namespace esp_brookesia::app::matter_controller {
namespace {

constexpr const char *DEFAULT_ROOM_KEY = "room.unassigned";

ControllerDevice make_mock_light(
    std::string name_key, std::string room_key, std::string status_key, uint64_t node_id,
    int brightness, int cct, bool powered_on)
{
    return {
        .name_key = std::move(name_key),
        .display_name = "",
        .device_type = "light",
        .card_template = "light_color_temp",
        .tag_key = "tag.matter_light",
        .room_key = std::move(room_key),
        .powered_on = powered_on,
        .brightness = brightness,
        .cct = cct,
        .status_key = std::move(status_key),
        .node_id = node_id,
        .reachable = true,
        .is_commissioned = true,
        .supports_onoff = true,
        .supports_level = true,
        .supports_color_temp = true,
        // The esp-matter v1.5 extended_color_light endpoint enables XY and
        // Color Temperature, but not the Hue/Saturation feature. A real Hue
        // control is enabled later only if FeatureMap/ColorCapabilities says so.
        .supports_hue = false,
    };
}

ControllerDevice make_mock_socket(
    std::string name_key, std::string room_key, uint64_t node_id, bool powered_on)
{
    return {
        .name_key = std::move(name_key),
        .display_name = "",
        .device_type = "socket",
        .card_template = "basic",
        .tag_key = "tag.matter_socket",
        .room_key = std::move(room_key),
        .powered_on = powered_on,
        .status_key = powered_on ? "status.active" : "status.off",
        .node_id = node_id,
        .reachable = true,
        .is_commissioned = true,
        .supports_onoff = true,
    };
}

std::vector<ControllerDevice> make_mock_devices()
{
    return {
        make_mock_light(
            "device.living_room_light", "room.living_room", "status.warm_white",
            0x1001, 80, 40, true
        ),
        make_mock_socket("device.kitchen_socket", "room.kitchen", 0x1002, true),
    };
}

std::vector<ControllerRoom> make_mock_rooms()
{
    return {
        {.name_key = "room.living_room"},
        {.name_key = "room.kitchen"},
        {.name_key = "room.study"},
        {.name_key = "room.bedroom"},
    };
}

class MockMatterBackend final: public MatterBackend {
public:
    MockMatterBackend()
        : devices_(make_mock_devices())
        , rooms_(make_mock_rooms())
    {
    }

    std::expected<void, std::string> start() override
    {
        dirty_ = true;
        return {};
    }

    void stop() override {}

    std::expected<void, std::string> tick() override
    {
        return {};
    }

    bool consume_dirty() override
    {
        const bool dirty = dirty_;
        dirty_ = false;
        return dirty;
    }

    const std::vector<ControllerDevice> &devices() const override
    {
        return devices_;
    }

    const std::vector<ControllerRoom> &rooms() const override
    {
        return rooms_;
    }

    std::expected<void, std::string> toggle_device(uint64_t node_id, uint16_t endpoint_id) override
    {
        auto *device = find_device(node_id, endpoint_id);
        if (!device) {
            return std::unexpected("Selected device was not found");
        }
        device->powered_on = !device->powered_on;
        if (!device->powered_on) {
            device->status_key = "status.off";
        } else if (device->device_type == "light" && device->supports_level) {
            if (device->supports_hue) {
                device->status_key = "status.hue";
            } else if (device->supports_color_temp) {
                device->status_key = "status.cct";
            } else {
                device->status_key = "status.bright";
            }
        } else if (device->device_type == "socket") {
            device->status_key = "status.active";
        } else {
            device->status_key = "status.on";
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_brightness(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        auto *device = find_device(node_id, endpoint_id);
        if (!device) {
            return std::unexpected("Selected device was not found");
        }
        if (!device->supports_level) {
            return std::unexpected("Selected device does not support brightness");
        }
        device->brightness = std::clamp(value, 0, 100);
        device->status_key = "status.bright";
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_cct(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        auto *device = find_device(node_id, endpoint_id);
        if (!device) {
            return std::unexpected("Selected device was not found");
        }
        if (!device->supports_color_temp) {
            return std::unexpected("Selected device does not support color temperature");
        }
        device->cct = std::clamp(value, 27, 65);
        device->status_key = "status.cct";
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_hue(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        auto *device = find_device(node_id, endpoint_id);
        if (!device) {
            return std::unexpected("Selected device was not found");
        }
        if (!device->supports_hue) {
            return std::unexpected("Selected device does not support hue");
        }
        device->hue = std::clamp(value, 0, 359);
        device->status_key = "status.hue";
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_saturation(
        uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        auto *device = find_device(node_id, endpoint_id);
        if (!device) {
            return std::unexpected("Selected device was not found");
        }
        if (!device->supports_hue) {
            return std::unexpected("Selected device does not support saturation");
        }
        device->saturation = std::clamp(value, 0, 100);
        device->status_key = "status.saturation";
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> assign_room(
        std::string room_name, const std::vector<std::pair<uint64_t, uint16_t>> &device_ids) override
    {
        for (const auto &[node_id, endpoint_id] : device_ids) {
            auto *device = find_device(node_id, endpoint_id);
            if (device) {
                device->room_key = room_name;
            }
        }
        rooms_.push_back({.name_key = std::move(room_name)});
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> remove_device(uint64_t node_id, uint16_t endpoint_id) override
    {
        if (!find_device(node_id, endpoint_id)) {
            return std::unexpected("Selected device was not found");
        }
        std::erase_if(devices_, [node_id](const ControllerDevice &device) {
            return device.node_id == node_id;
        });
        rebuild_rooms();
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> reset() override
    {
        devices_ = make_mock_devices();
        rooms_ = make_mock_rooms();
        dirty_ = true;
        return {};
    }

private:
    ControllerDevice *find_device(uint64_t node_id, uint16_t endpoint_id)
    {
        auto it = std::find_if(devices_.begin(), devices_.end(), [&](const ControllerDevice & device) {
            return device.node_id == node_id && device.endpoint_id == endpoint_id;
        });
        if (it != devices_.end()) {
            return &(*it);
        }
        it = std::find_if(devices_.begin(), devices_.end(), [&](const ControllerDevice & device) {
            return device.node_id == node_id;
        });
        return it != devices_.end() ? &(*it) : nullptr;
    }

    void rebuild_rooms()
    {
        std::vector<ControllerRoom> next_rooms;
        std::unordered_set<std::string> seen;
        for (const auto &device : devices_) {
            const auto &room_key = device.room_key.empty() ? std::string(DEFAULT_ROOM_KEY) : device.room_key;
            if (seen.insert(room_key).second) {
                next_rooms.push_back({.name_key = room_key});
            }
        }
        rooms_ = std::move(next_rooms);
    }

    bool dirty_ = true;
    std::vector<ControllerDevice> devices_;
    std::vector<ControllerRoom> rooms_;
};

#if BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND

// Cross-task ingress is kept separate from the UI model. RainMaker report and
// device-list callbacks only coalesce small scalar updates here; tick() applies
// them on the app/backend task.
struct PendingMatterUpdate {
    enum class Type {
        Reachability,
        Attribute,
    };

    Type type = Type::Attribute;
    uint64_t node_id = 0;
    uint16_t endpoint_id = 0;
    uint32_t cluster_id = 0;
    uint32_t attribute_id = 0;
    int64_t value = 0;
    bool online = false;
};

static std::mutex s_pending_mutex;
static std::vector<ControllerDevice> s_pending_devices;
static bool s_pending_devices_dirty = false;
static std::vector<PendingMatterUpdate> s_pending_updates;

std::optional<size_t> find_device_index(
    const std::vector<ControllerDevice> &devices, uint64_t node_id, uint16_t endpoint_id)
{
    auto it = std::find_if(devices.begin(), devices.end(), [&](const ControllerDevice & device) {
        return device.node_id == node_id && device.endpoint_id == endpoint_id;
    });
    if (it == devices.end()) {
        return std::nullopt;
    }
    return static_cast<size_t>(std::distance(devices.begin(), it));
}

// `esp_matter_controller_pairing_command.cpp` is intentionally excluded when
// CONFIG_ESP_MATTER_COMMISSIONER_ENABLE is disabled. Use the controller client
// that is always built in this product configuration and retain this object
// until CHIP completes the asynchronous CurrentFabricRemover transaction.
class MatterFabricRemover final : private chip::Controller::CurrentFabricRemover {
public:
    static esp_err_t start(chip::Controller::DeviceController *controller, chip::NodeId node_id)
    {
        if (controller == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        auto *remover = chip::Platform::New<MatterFabricRemover>(controller);
        if (remover == nullptr) {
            return ESP_ERR_NO_MEM;
        }
        // CurrentFabricRemover enters CHIP controller code immediately.  This
        // backend method is invoked from the Brookesia/UI task, not the CHIP
        // task, so acquire the same CHIP stack lock used by send_matter_command()
        // before starting the asynchronous transaction.  The remover remains
        // alive until its completion callback runs on the CHIP task.
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        const CHIP_ERROR err =
            remover->CurrentFabricRemover::RemoveCurrentFabric(node_id, &remover->completion_callback_);
        if (err != CHIP_NO_ERROR) {
            chip::Platform::Delete(remover);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    // chip::Platform::New() invokes this constructor outside the class.
    explicit MatterFabricRemover(chip::Controller::DeviceController *controller)
        : chip::Controller::CurrentFabricRemover(controller)
        , completion_callback_(on_complete, this)
    {
    }

private:
    static void on_complete(void *context, chip::NodeId node_id, CHIP_ERROR status)
    {
        ESP_LOGI("MatterBackend", "Matter unpair completed: node=%llu, status=%s",
                 static_cast<unsigned long long>(node_id),
                 status == CHIP_NO_ERROR ? "success" : "failed");
        chip::Platform::Delete(static_cast<MatterFabricRemover *>(context));
    }

    chip::Callback::Callback<chip::Controller::OnCurrentFabricRemove> completion_callback_;
};

std::string make_level_payload(int value)
{
    const int level = std::clamp(static_cast<int>(std::lround((value / 100.0F) * 254.0F)), 0, 254);
    std::ostringstream stream;
    stream << "{\"0:U8\": " << level << ", \"1:U16\": 0, \"2:U8\": 0, \"3:U8\": 0}";
    return stream.str();
}

std::string make_cct_payload(int value)
{
    const int kelvin = std::clamp(value, 27, 65) * 100;
    const int mireds = std::clamp(static_cast<int>(std::lround(1000000.0 / kelvin)), 153, 500);
    std::ostringstream stream;
    stream << "{\"0:U16\": " << mireds << ", \"1:U16\": 0, \"2:U8\": 0, \"3:U8\": 0}";
    return stream.str();
}

std::string make_hue_payload(int value)
{
    const int matter_hue = std::clamp(static_cast<int>(std::lround((value / 359.0F) * 254.0F)), 0, 254);
    std::ostringstream stream;
    stream << "{\"0:U8\": " << matter_hue << ", \"1:U8\": 0, \"2:U16\": 0, \"3:U8\": 0, \"4:U8\": 0}";
    return stream.str();
}

std::string make_saturation_payload(int value)
{
    const int matter_saturation =
        std::clamp(static_cast<int>(std::lround((value / 100.0F) * 254.0F)), 0, 254);
    std::ostringstream stream;
    stream << "{\"0:U8\": " << matter_saturation
           << ", \"1:U16\": 0, \"2:U8\": 0, \"3:U8\": 0}";
    return stream.str();
}

int level_to_percent(uint8_t value)
{
    return std::clamp(static_cast<int>(std::lround((value / 254.0F) * 100.0F)), 0, 100);
}

int mireds_to_ui_cct(uint16_t value)
{
    if (value == 0) {
        return 27;
    }
    const int kelvin = static_cast<int>(std::lround(1000000.0 / value));
    return std::clamp(static_cast<int>(std::lround(kelvin / 100.0)), 27, 65);
}

int matter_hue_to_ui(uint8_t value)
{
    return std::clamp(static_cast<int>(std::lround((value / 254.0F) * 359.0F)), 0, 359);
}

int matter_saturation_to_ui(uint8_t value)
{
    return std::clamp(static_cast<int>(std::lround((value / 254.0F) * 100.0F)), 0, 100);
}

// Helper: check if any device type in the endpoint's device_type_list matches
// a given Matter device type ID.
static bool endpoint_has_device_type(const endpoint_entry_t &endpoint, uint32_t type_id)
{
    for (uint8_t i = 0; i < endpoint.device_type_count; ++i) {
        if (endpoint.device_type_list[i] == type_id) {
            return true;
        }
    }
    return false;
}

// The endpoint Device Type gives a low-cost initial card selection, but some
// real-world lights advertise Extended Color Light while exposing only a subset
// of Color Control features. ColorCapabilities/FeatureMap and live Hue reports
// promote the card; once Hue is discovered it is sticky so a later incomplete
// capability bitmap cannot hide the RGB sliders after a device-list refresh.
void update_light_card_template(ControllerDevice &device)
{
    if (device.device_type != "light") {
        return;
    }

    if (!device.supports_level) {
        device.card_template = "light_onoff";
    } else if (device.supports_hue) {
        device.card_template = "light_extended";
    } else if (device.supports_color_temp) {
        device.card_template = "light_color_temp";
    } else {
        device.card_template = "light_dimmable";
    }
}

std::optional<ControllerDevice> make_device_from_endpoint(
    uint64_t node_id, const char *device_name, const endpoint_entry_t &endpoint, bool reachable)
{
    // Device Type determines the product/card family. Unknown endpoints and
    // client-side Switch endpoints are intentionally ignored instead of being
    // rendered as a fallback Light card.
    const bool is_extended_light = endpoint_has_device_type(endpoint, ESP_MATTER_EXTENDED_COLOR_LIGHT_DEVICE_TYPE_ID);
    const bool is_color_temp_light = endpoint_has_device_type(endpoint, ESP_MATTER_COLOR_TEMPERATURE_LIGHT_DEVICE_TYPE_ID);
    const bool is_dimmable_light = endpoint_has_device_type(endpoint, ESP_MATTER_DIMMABLE_LIGHT_DEVICE_TYPE_ID);
    const bool is_onoff_light = endpoint_has_device_type(endpoint, ESP_MATTER_ON_OFF_LIGHT_DEVICE_TYPE_ID);
    const bool is_light = is_extended_light || is_color_temp_light || is_dimmable_light || is_onoff_light;

    // Matter calls the standard socket endpoint an On/Off Plug-in Unit
    // (0x010A). It is normalized to the single UI product name "socket".
    // Dimmable Plug-in Unit and all Switch device types are not supported.
    const bool is_socket = endpoint_has_device_type(endpoint, ESP_MATTER_ON_OFF_PLUG_IN_UNIT_DEVICE_TYPE_ID);
    if (!is_light && !is_socket) {
        return std::nullopt;
    }

    ControllerDevice device = {
        .display_name = device_name ? std::string(device_name) : std::string(),
        .device_type = is_socket ? "socket" : "light",
        // Start conservatively for an Extended Color Light. The device-list
        // metadata can label a reduced-capability lamp as 0x010D; its later
        // ColorCapabilities report promotes this to the full card only when
        // Hue/Saturation is actually supported.
        .card_template = is_socket ? "basic" :
                         ((is_extended_light || is_color_temp_light) ? "light_color_temp" :
                          (is_dimmable_light ? "light_dimmable" : "light_onoff")),
        .tag_key = is_socket ? "tag.matter_socket" : "tag.matter_light",
        .room_key = DEFAULT_ROOM_KEY,
        .powered_on = false,
        .brightness = 100,
        .cct = 40,
        .hue = 30,
        .saturation = 100,
        .status_key = "status.off",
        .node_id = node_id,
        .endpoint_id = endpoint.endpoint_id,
        .reachable = reachable,
        .is_commissioned = true,
        .supports_onoff = true,
        .supports_level = is_extended_light || is_color_temp_light || is_dimmable_light,
        .supports_color_temp = is_extended_light || is_color_temp_light,
        // ColorCapabilities is authoritative for Hue/Saturation. Keep the
        // initial card conservative until the report arrives, rather than
        // trusting an Extended Color Light declaration blindly.
        .supports_hue = false,
    };

    if (device.display_name.empty()) {
        device.display_name = "Matter Device";
    }
    return device;
}

constexpr const char *MATTER_BACKEND_TAG = "MatterBackend";

struct PendingCommandSlot {
    void *context = nullptr;
    uint64_t node_id = 0;
};

constexpr size_t kMaxPendingCommands = 8;
std::mutex s_command_mutex;
std::array<PendingCommandSlot, kMaxPendingCommands> s_pending_commands = {};

void finish_pending_command(void *context)
{
    std::lock_guard<std::mutex> lock(s_command_mutex);
    const auto entry = std::find_if(s_pending_commands.begin(), s_pending_commands.end(),
        [context](const PendingCommandSlot &slot) { return slot.context == context; });
    if (entry != s_pending_commands.end()) {
        *entry = {};
    }
}

void matter_command_success(
    void *context, const chip::app::ConcreteCommandPath &path,
    const chip::app::StatusIB &status, chip::TLV::TLVReader *)
{
    finish_pending_command(context);
    ESP_LOGI(
        MATTER_BACKEND_TAG,
        "Command response: endpoint=%u cluster=0x%08" PRIx32
        " command=0x%08" PRIx32 " status=0x%02x",
        static_cast<unsigned>(path.mEndpointId), path.mClusterId, path.mCommandId,
        static_cast<unsigned>(status.mStatus)
    );
}

void matter_command_error(void *context, CHIP_ERROR error)
{
    finish_pending_command(context);
    ESP_LOGE(MATTER_BACKEND_TAG, "Command interaction failed: %" CHIP_ERROR_FORMAT, error.Format());
}

void matter_command_connect_failure(
    void *context, const chip::ScopedNodeId &peer_id, CHIP_ERROR error)
{
    finish_pending_command(context);
    ESP_LOGE(
        MATTER_BACKEND_TAG,
        "Command CASE connection failed: node=0x%016" PRIx64 " fabric=%u error=%" CHIP_ERROR_FORMAT,
        static_cast<uint64_t>(peer_id.GetNodeId()), static_cast<unsigned>(peer_id.GetFabricIndex()), error.Format()
    );
    notify_matter_node_reachability(peer_id.GetNodeId(), false);
}

std::expected<void, std::string> send_matter_command(
    uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
    uint32_t command_id, const char *payload)
{
    PendingCommandSlot *free_slot = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_command_mutex);
        for (auto &slot : s_pending_commands) {
            if (slot.context != nullptr && slot.node_id == node_id) {
                return std::unexpected("A Matter command is already pending for this device");
            }
            if (slot.context == nullptr && free_slot == nullptr) {
                free_slot = &slot;
            }
        }
        if (free_slot == nullptr) {
            return std::unexpected("Too many Matter commands are pending");
        }
    }

    auto *command = chip::Platform::New<esp_matter::controller::cluster_command>(
        node_id, endpoint_id, cluster_id, command_id, payload, chip::NullOptional,
        matter_command_success, matter_command_error, matter_command_connect_failure
    );
    if (command == nullptr) {
        return std::unexpected("Failed to allocate Matter cluster command");
    }
    {
        std::lock_guard<std::mutex> lock(s_command_mutex);
        // Re-scan under the lock because another caller could have occupied
        // the slot while the Matter command object was being allocated.
        free_slot = nullptr;
        for (auto &slot : s_pending_commands) {
            if (slot.context != nullptr && slot.node_id == node_id) {
                chip::Platform::Delete(command);
                return std::unexpected("A Matter command is already pending for this device");
            }
            if (slot.context == nullptr && free_slot == nullptr) {
                free_slot = &slot;
            }
        }
        if (free_slot == nullptr) {
            chip::Platform::Delete(command);
            return std::unexpected("Too many Matter commands are pending");
        }
        free_slot->context = command;
        free_slot->node_id = node_id;
    }

    ESP_LOGI(
        MATTER_BACKEND_TAG,
        "Queue command: node=0x%016" PRIx64 " endpoint=%u cluster=0x%08" PRIx32
        " command=0x%08" PRIx32,
        node_id, static_cast<unsigned>(endpoint_id), cluster_id, command_id
    );

    esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
    const auto err = command->send_command();
    // cluster_command owns and deletes itself after send_command(), including
    // the immediate failure path.
    if (err != ESP_OK) {
        finish_pending_command(command);
        return std::unexpected(std::string("Failed to queue Matter command: ") + esp_err_to_name(err));
    }
    return {};
}

class EspMatterBackend final: public MatterBackend {
public:
    std::expected<void, std::string> start() override
    {
        dirty_ = true;
        auto refresh_result = refresh_devices();
        if (!refresh_result) {
            return refresh_result;
        }
        apply_pending_updates();
        return {};
    }

    void stop() override {}

    std::expected<void, std::string> tick() override
    {
        auto refresh_result = refresh_devices();
        if (!refresh_result) {
            return refresh_result;
        }
        apply_pending_updates();
        return {};
    }

    bool consume_dirty() override
    {
        const bool dirty = dirty_;
        dirty_ = false;
        return dirty;
    }

    const std::vector<ControllerDevice> &devices() const override
    {
        return devices_;
    }

    const std::vector<ControllerRoom> &rooms() const override
    {
        return rooms_;
    }

    std::expected<void, std::string> toggle_device(uint64_t node_id, uint16_t endpoint_id) override
    {
        const auto device_index = find_device_index(devices_, node_id, endpoint_id);
        if (!device_index) {
            return std::unexpected("Selected Matter device was not found");
        }
        auto &device = devices_[*device_index];
        const auto previous_power = device.powered_on;
        const auto previous_status = device.status_key;
        device.powered_on = !device.powered_on;
        update_power_status(device);

        auto result = send_matter_command(
            node_id, endpoint_id, chip::app::Clusters::OnOff::Id,
            chip::app::Clusters::OnOff::Commands::Toggle::Id, nullptr
        );
        if (!result) {
            device.powered_on = previous_power;
            device.status_key = previous_status;
            return result;
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_brightness(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        const auto device_index = find_device_index(devices_, node_id, endpoint_id);
        if (!device_index) {
            return std::unexpected("Selected Matter device was not found");
        }
        auto &device = devices_[*device_index];
        if (!device.supports_level) {
            return std::unexpected("Selected Matter device does not support brightness");
        }
        const auto previous = device;
        device.brightness = std::clamp(value, 0, 100);
        device.powered_on = true;
        device.status_key = "status.bright";
        const auto payload = make_level_payload(device.brightness);
        auto result = send_matter_command(
            node_id, endpoint_id, chip::app::Clusters::LevelControl::Id,
            chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id, payload.c_str()
        );
        if (!result) {
            device = previous;
            return result;
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_cct(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        const auto device_index = find_device_index(devices_, node_id, endpoint_id);
        if (!device_index) {
            return std::unexpected("Selected Matter device was not found");
        }
        auto &device = devices_[*device_index];
        if (!device.supports_color_temp) {
            return std::unexpected("Selected Matter device does not support color temperature");
        }
        const auto previous = device;
        device.cct = std::clamp(value, 27, 65);
        device.powered_on = true;
        device.status_key = "status.cct";
        const auto payload = make_cct_payload(device.cct);
        auto result = send_matter_command(
            node_id, endpoint_id, chip::app::Clusters::ColorControl::Id,
            chip::app::Clusters::ColorControl::Commands::MoveToColorTemperature::Id, payload.c_str()
        );
        if (!result) {
            device = previous;
            return result;
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_hue(uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        const auto device_index = find_device_index(devices_, node_id, endpoint_id);
        if (!device_index) {
            return std::unexpected("Selected Matter device was not found");
        }
        auto &device = devices_[*device_index];
        if (!device.supports_hue) {
            return std::unexpected("Selected Matter device does not support hue");
        }
        const auto previous = device;
        device.hue = std::clamp(value, 0, 359);
        device.powered_on = true;
        device.status_key = "status.hue";
        const auto payload = make_hue_payload(device.hue);
        auto result = send_matter_command(
            node_id, endpoint_id, chip::app::Clusters::ColorControl::Id,
            chip::app::Clusters::ColorControl::Commands::MoveToHue::Id, payload.c_str()
        );
        if (!result) {
            device = previous;
            return result;
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> set_saturation(
        uint64_t node_id, uint16_t endpoint_id, int value) override
    {
        const auto device_index = find_device_index(devices_, node_id, endpoint_id);
        if (!device_index) {
            return std::unexpected("Selected Matter device was not found");
        }
        auto &device = devices_[*device_index];
        if (!device.supports_hue) {
            return std::unexpected("Selected Matter device does not support saturation");
        }
        const auto previous = device;
        device.saturation = std::clamp(value, 0, 100);
        device.powered_on = true;
        device.status_key = "status.saturation";
        const auto payload = make_saturation_payload(device.saturation);
        auto result = send_matter_command(
            node_id, endpoint_id, chip::app::Clusters::ColorControl::Id,
            chip::app::Clusters::ColorControl::Commands::MoveToSaturation::Id, payload.c_str()
        );
        if (!result) {
            device = previous;
            return result;
        }
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> assign_room(
        std::string room_name, const std::vector<std::pair<uint64_t, uint16_t>> &device_ids) override
    {
        for (const auto &[node_id, endpoint_id] : device_ids) {
            const auto device_index = find_device_index(devices_, node_id, endpoint_id);
            if (device_index) {
                devices_[*device_index].room_key = room_name;
            }
        }
        rebuild_rooms();
        dirty_ = true;
        return {};
    }

    std::expected<void, std::string> remove_device(uint64_t node_id, uint16_t endpoint_id) override
    {
        if (!find_device_index(devices_, node_id, endpoint_id)) {
            return std::unexpected("Selected Matter device was not found");
        }

        // Pairing commands are not linked in this non-Commissioner build.
        // Start the same Matter CurrentFabricRemover transaction through the
        // controller client that is active for the RainMaker device list.
        auto &controller_client = esp_matter::controller::matter_controller_client::get_instance();
        const esp_err_t err = MatterFabricRemover::start(controller_client.get_controller(), node_id);
        if (err != ESP_OK) {
            return std::unexpected(std::string("Matter unpair request failed: ") + esp_err_to_name(err));
        }

        locally_removed_node_ids_.insert(node_id);
        std::erase_if(devices_, [node_id](const ControllerDevice &device) {
            return device.node_id == node_id;
        });
        rebuild_rooms();
        dirty_ = true;

        if (app_rmaker_matter_device_list_updatable()) {
            const esp_err_t refresh_err = app_rmaker_matter_device_list_update();
            if (refresh_err != ESP_OK) {
                ESP_LOGW(MATTER_BACKEND_TAG, "Matter device-list refresh after unpair failed: %s",
                         esp_err_to_name(refresh_err));
            }
        }
        return {};
    }

    std::expected<void, std::string> reset() override
    {
        locally_removed_node_ids_.clear();
        devices_.clear();
        rooms_.clear();
        dirty_ = true;
        return {};
    }

private:
    static void update_power_status(ControllerDevice &device)
    {
        if (!device.powered_on) {
            device.status_key = "status.off";
        } else if (device.device_type == "light" && device.supports_level) {
            device.status_key = device.supports_hue ? "status.hue" :
                                (device.supports_color_temp ? "status.cct" : "status.bright");
        } else if (device.device_type == "socket") {
            device.status_key = "status.active";
        } else {
            device.status_key = "status.on";
        }
    }

    std::expected<void, std::string> refresh_devices()
    {
        std::vector<ControllerDevice> pending;
        bool has_update = false;
        {
            std::lock_guard<std::mutex> lock(s_pending_mutex);
            if (s_pending_devices_dirty) {
                pending = std::move(s_pending_devices);
                s_pending_devices.clear();
                s_pending_devices_dirty = false;
                has_update = true;
            }
        }
        if (!has_update) {
            return {};
        }


        // Cloud list updates can arrive before the asynchronous unpair has
        // propagated to RainMaker. Keep a successfully requested removal out
        // of the UI rather than recreating its card from a stale snapshot.
        std::erase_if(pending, [this](const ControllerDevice &device) {
            return locally_removed_node_ids_.contains(device.node_id);
        });

        // Device metadata refreshes must not overwrite the latest report state.
        for (auto &next_device : pending) {
            const auto existing_index = find_device_index(devices_, next_device.node_id, next_device.endpoint_id);
            if (!existing_index) {
                continue;
            }
            const auto &existing = devices_[*existing_index];
            next_device.powered_on = existing.powered_on;
            next_device.brightness = existing.brightness;
            next_device.cct = existing.cct;
            next_device.hue = existing.hue;
            next_device.saturation = existing.saturation;
            next_device.status_key = existing.status_key;
            next_device.room_key = existing.room_key;
            next_device.reachable = existing.reachable;
            // A device-list refresh contains product metadata, while Color
            // Control reports carry the discovered capabilities. Keep every
            // capability learned from reports (and the Extended Color Light
            // baseline) instead of hiding Hue/Saturation on a later refresh.
            next_device.supports_onoff = existing.supports_onoff || next_device.supports_onoff;
            next_device.supports_level = existing.supports_level || next_device.supports_level;
            next_device.supports_color_temp = existing.supports_color_temp || next_device.supports_color_temp;
            next_device.supports_hue = existing.supports_hue || next_device.supports_hue;
            // make_device_from_endpoint() starts every Extended Color Light on
            // the conservative color-temp card. After OR-ing the previously
            // discovered ColorCapabilities, recompute the template or a later
            // socket/list refresh will demote an already-promoted RGB card.
            update_light_card_template(next_device);
        }

        if (pending != devices_) {
            devices_ = std::move(pending);
            rebuild_rooms();
            dirty_ = true;
        }
        return {};
    }

    void apply_pending_updates()
    {
        std::vector<PendingMatterUpdate> updates;
        {
            std::lock_guard<std::mutex> lock(s_pending_mutex);
            updates.swap(s_pending_updates);
        }

        for (const auto &update : updates) {
            if (update.type == PendingMatterUpdate::Type::Reachability) {
                for (auto &device : devices_) {
                    if (device.node_id != update.node_id || device.reachable == update.online) {
                        continue;
                    }
                    device.reachable = update.online;
                    dirty_ = true;
                }
                continue;
            }

            const auto device_index = find_device_index(devices_, update.node_id, update.endpoint_id);
            if (!device_index) {
                continue;
            }
            auto &device = devices_[*device_index];
            bool changed = false;

            if (update.cluster_id == chip::app::Clusters::OnOff::Id &&
                    update.attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
                const bool value = update.value != 0;
                if (device.powered_on != value) {
                    device.powered_on = value;
                    update_power_status(device);
                    changed = true;
                }
            } else if (update.cluster_id == chip::app::Clusters::LevelControl::Id &&
                       update.attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) {
                const auto raw_level = static_cast<uint8_t>(std::clamp<int64_t>(update.value, 0, 254));
                const int brightness = level_to_percent(raw_level);
                if (device.brightness != brightness) {
                    device.brightness = brightness;
                    device.status_key = "status.bright";
                    changed = true;
                }
            } else if (update.cluster_id == chip::app::Clusters::ColorControl::Id &&
                       update.attribute_id == chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id) {
                const auto raw_mireds = static_cast<uint16_t>(std::clamp<int64_t>(update.value, 0, UINT16_MAX));
                const int cct = mireds_to_ui_cct(raw_mireds);
                if (device.cct != cct) {
                    device.cct = cct;
                    device.status_key = "status.cct";
                    changed = true;
                }
            } else if (update.cluster_id == chip::app::Clusters::ColorControl::Id &&
                       (update.attribute_id ==
                            chip::app::Clusters::ColorControl::Attributes::ColorCapabilities::Id ||
                        update.attribute_id ==
                            chip::app::Clusters::ColorControl::Attributes::FeatureMap::Id)) {
                constexpr uint32_t kHueSaturationBit = 0x01U;
                constexpr uint32_t kEnhancedHueBit = 0x02U;
                constexpr uint32_t kColorTemperatureBit = 0x10U;
                const uint32_t capabilities = static_cast<uint32_t>(update.value);

                // Zero means "unknown" — ignore it. Non-zero reports only
                // promote HS/CT; they never demote an already-discovered RGB card
                // (incomplete resubscribe bitmaps were wiping hue/sat sliders).
                if (capabilities != 0) {
                    // Promote-only: ColorCapabilities/FeatureMap may discover HS
                    // or CT, but must not tear down an already-promoted extended
                    // card when a later report is incomplete.
                    const bool reported_hue =
                        (capabilities & (kHueSaturationBit | kEnhancedHueBit)) != 0;
                    const bool reported_color_temp =
                        (capabilities & kColorTemperatureBit) != 0;
                    if (reported_hue && !device.supports_hue) {
                        device.supports_hue = true;
                        changed = true;
                    }
                    if (reported_color_temp && !device.supports_color_temp) {
                        device.supports_color_temp = true;
                        changed = true;
                    }

                    const std::string previous_template = device.card_template;
                    update_light_card_template(device);
                    if (device.card_template != previous_template) {
                        ESP_LOGI(MATTER_BACKEND_TAG,
                                 "Light capability card update: node=0x%016" PRIx64
                                 " endpoint=%u attr=0x%08" PRIx32 " caps=0x%08" PRIx32
                                 " template=%s",
                                 device.node_id, static_cast<unsigned>(device.endpoint_id),
                                 update.attribute_id, capabilities,
                                 device.card_template.c_str());
                        changed = true;
                    }
                }
            } else if (update.cluster_id == chip::app::Clusters::ColorControl::Id &&
                       update.attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id) {
                const auto raw_hue = static_cast<uint8_t>(std::clamp<int64_t>(update.value, 0, 254));
                const int hue = matter_hue_to_ui(raw_hue);
                // A live CurrentHue report is stronger evidence than Device Type.
                // Promote the RGB card here so a missed/zero ColorCapabilities
                // report cannot leave the UI stuck on color-temp forever.
                if (!device.supports_hue) {
                    device.supports_hue = true;
                    update_light_card_template(device);
                    changed = true;
                }
                if (device.hue != hue) {
                    device.hue = hue;
                    device.status_key = "status.hue";
                    changed = true;
                }
            } else if (update.cluster_id == chip::app::Clusters::ColorControl::Id &&
                       update.attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id) {
                const auto raw_saturation =
                    static_cast<uint8_t>(std::clamp<int64_t>(update.value, 0, 254));
                const int saturation = matter_saturation_to_ui(raw_saturation);
                if (!device.supports_hue) {
                    device.supports_hue = true;
                    update_light_card_template(device);
                    changed = true;
                }
                if (device.saturation != saturation) {
                    device.saturation = saturation;
                    device.status_key = "status.saturation";
                    changed = true;
                }
            }

            if (!device.reachable) {
                device.reachable = true;
                changed = true;
            }
            if (changed) {
                dirty_ = true;
            }
        }
    }

    void rebuild_rooms()
    {
        std::vector<ControllerRoom> next_rooms;
        std::unordered_set<std::string> seen;
        for (const auto &device : devices_) {
            const auto &room_key = device.room_key.empty() ? std::string(DEFAULT_ROOM_KEY) : device.room_key;
            if (seen.insert(room_key).second) {
                next_rooms.push_back({.name_key = room_key});
            }
        }
        rooms_ = std::move(next_rooms);
    }

    bool dirty_ = true;
    std::unordered_set<uint64_t> locally_removed_node_ids_;
    std::vector<ControllerDevice> devices_;
    std::vector<ControllerRoom> rooms_;
};

#endif

} // namespace

void notify_matter_device_list_update(int err, const matter_device_t *dev_list)
{
#if BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND
    if (err != ESP_OK) {
        ESP_LOGW(MATTER_BACKEND_TAG, "Matter device-list update failed: %s", esp_err_to_name(err));
        return;
    }

    std::vector<ControllerDevice> next_devices;
    size_t endpoint_count = 0;
    for (const matter_device_t *entry = dev_list; entry != nullptr; entry = entry->next) {
        for (uint8_t ep_idx = 0; ep_idx < entry->endpoint_count; ++ep_idx) {
            ++endpoint_count;
            auto device = make_device_from_endpoint(
                entry->node_id,
                entry->device_name,
                entry->endpoints[ep_idx],
                /* reachable = */ true // online status is tracked by the report subsystem
            );
            if (device) {
                next_devices.push_back(std::move(*device));
            }
        }
    }


    {
        std::lock_guard<std::mutex> lock(s_pending_mutex);
        s_pending_devices = std::move(next_devices);
        s_pending_devices_dirty = true;
    }
#else
    (void)err;
    (void)dev_list;
#endif
}

std::unique_ptr<MatterBackend> create_default_matter_backend()
{
#if BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND
    return std::make_unique<EspMatterBackend>();
#else
    return std::make_unique<MockMatterBackend>();
#endif
}


void notify_matter_node_reachability(uint64_t node_id, bool online)
{
#if BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND
    if (node_id == 0) {
        return;
    }
    try {
        std::lock_guard<std::mutex> lock(s_pending_mutex);
        const auto existing = std::find_if(s_pending_updates.rbegin(), s_pending_updates.rend(),
            [node_id](const PendingMatterUpdate &update) {
                return update.type == PendingMatterUpdate::Type::Reachability && update.node_id == node_id;
            });
        if (existing != s_pending_updates.rend()) {
            existing->online = online;
            return;
        }
        s_pending_updates.push_back({
            .type = PendingMatterUpdate::Type::Reachability,
            .node_id = node_id,
            .online = online,
        });
    } catch (const std::bad_alloc &) {
        ESP_LOGE(MATTER_BACKEND_TAG, "Dropping reachability update: out of memory");
    }
#else
    (void)node_id;
    (void)online;
#endif
}

void notify_matter_attribute_update(
    uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id,
    uint32_t attribute_id, int64_t value)
{
#if BROOKESIA_APP_MATTER_CONTROLLER_HAS_ESP_BACKEND
    const bool supported =
        (cluster_id == chip::app::Clusters::OnOff::Id &&
         attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) ||
        (cluster_id == chip::app::Clusters::LevelControl::Id &&
         attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) ||
        (cluster_id == chip::app::Clusters::ColorControl::Id &&
         (attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentHue::Id ||
          attribute_id == chip::app::Clusters::ColorControl::Attributes::CurrentSaturation::Id ||
          attribute_id == chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id ||
          attribute_id == chip::app::Clusters::ColorControl::Attributes::FeatureMap::Id ||
          attribute_id == chip::app::Clusters::ColorControl::Attributes::ColorCapabilities::Id));
    if (!supported || node_id == 0) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(s_pending_mutex);
        const auto existing = std::find_if(s_pending_updates.rbegin(), s_pending_updates.rend(),
            [=](const PendingMatterUpdate &update) {
                return update.type == PendingMatterUpdate::Type::Attribute &&
                       update.node_id == node_id && update.endpoint_id == endpoint_id &&
                       update.cluster_id == cluster_id && update.attribute_id == attribute_id;
            });
        if (existing != s_pending_updates.rend()) {
            existing->value = value;
            return;
        }
        s_pending_updates.push_back({
            .type = PendingMatterUpdate::Type::Attribute,
            .node_id = node_id,
            .endpoint_id = endpoint_id,
            .cluster_id = cluster_id,
            .attribute_id = attribute_id,
            .value = value,
        });
    } catch (const std::bad_alloc &) {
        ESP_LOGE(MATTER_BACKEND_TAG, "Dropping attribute update: out of memory");
    }
#else
    (void)node_id;
    (void)endpoint_id;
    (void)cluster_id;
    (void)attribute_id;
    (void)value;
#endif
}

} // namespace esp_brookesia::app::matter_controller
