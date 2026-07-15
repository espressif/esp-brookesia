/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/app_matter_controller/matter_runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>


#if defined(ESP_PLATFORM) && defined(CONFIG_ESP_MATTER_CONTROLLER_ENABLE) && \
    __has_include(<esp_err.h>) && __has_include(<esp_log.h>) && __has_include(<esp_matter.h>) && \
    __has_include(<esp_check.h>) && __has_include(<esp_matter_console.h>) && \
    __has_include(<esp_rmaker_core.h>) && __has_include(<esp_matter_endpoint.h>) && \
    __has_include(<esp_rmaker_standard_devices.h>) && __has_include(<esp_rmaker_standard_params.h>) && \
    __has_include(<esp_rmaker_standard_types.h>) && __has_include(<setup_payload/OnboardingCodesUtil.h>) && \
    __has_include(<setup_payload/QRCodeSetupPayloadGenerator.h>) && \
    __has_include(<setup_payload/ManualSetupPayloadGenerator.h>) && \
    __has_include(<setup_payload/SetupPayload.h>) && \
    __has_include(<esp_rmaker_schedule.h>) && __has_include(<esp_rmaker_scenes.h>) && \
    __has_include(<app_controller.h>) && \
    __has_include(<app_rmaker_matter_controller.h>) && \
    __has_include(<app_rmaker_matter_device_list.h>) && \
    __has_include("qrcodegen.h")
#define BROOKESIA_APP_MATTER_CONTROLLER_HAS_RUNTIME 1

#include <app_controller.h>
#include <app_controller_op_creds_issuer.h>
#include <app_network.h>
#include <app_rmaker_matter_controller.h>
#include <app_rmaker_matter_device_list.h>
#include <app_rmaker_matter_report.h>
#include <cJSON.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_endpoint.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_auth_service.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_wifi.h>
#include <network_provisioning/manager.h>
#include <protocomm_security.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <controller/CHIPDeviceControllerFactory.h>
#include <credentials/FabricTable.h>
#include <esp_matter_controller_client.h>

#include "matter_backend.hpp"

#if __has_include(<lvgl.h>)
#include <lvgl.h>
#elif __has_include(<lvgl/lvgl.h>)
#include <lvgl/lvgl.h>
#else
#error "LVGL header is required for Matter pairing QR rendering"
#endif

#include "qrcodegen.h"

namespace {

constexpr const char *TAG = "matter_runtime";
constexpr const char *RMAKER_NODE_NAME = "ESP RainMaker Device";
constexpr const char *RMAKER_NODE_TYPE = "Controller";
constexpr const char *RMAKER_DEVICE_NAME = "MatterController";
constexpr bool RMAKER_DEFAULT_POWER = true;

static esp_err_t rmaker_device_write_cb(const esp_rmaker_device_t *device,
                                         const esp_rmaker_param_t  *param,
                                         esp_rmaker_param_val_t     val,
                                         void                      *priv_data,
                                         esp_rmaker_write_ctx_t    *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "RainMaker device write via: %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Power param updated: %s", val.val.b ? "true" : "false");
        esp_rmaker_param_update_and_report(param, val);
    }
    return ESP_OK;
}

constexpr uint32_t CONTROLLER_DEVICE_TYPE_ID = 0xFC01;
constexpr uint8_t CONTROLLER_DEVICE_TYPE_VERSION = 1;
constexpr int32_t QR_IMAGE_SIZE = 226;
constexpr int QR_QUIET_ZONE_MODULES = 4;

struct PairingQrImage {
    std::vector<uint8_t> pixels;
    lv_image_dsc_t descriptor = {};
};

std::mutex s_runtime_mutex;
std::mutex s_runtime_start_mutex;
esp_brookesia::app::matter_controller::MatterRuntimeState s_runtime_state = {
    .available = true,
};
std::unique_ptr<PairingQrImage> s_pairing_qr_image;
std::string s_pairing_qr_code;
std::string s_pairing_manual_code;
std::string s_pairing_setup_pin;

chip::RendezvousInformationFlags get_rendezvous_flags()
{
    return chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE);
}

// ---------------------------------------------------------------------------
// cJSON hooks — prefer PSRAM for large cJSON allocations (device metadata,
// attribute reports). Must be installed before any cJSON object is created.
// ---------------------------------------------------------------------------
static void *cjson_psram_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

static void install_cjson_hooks()
{
    static bool installed = false;
    if (installed) {
        return;
    }
    cJSON_Hooks hooks = {
        .malloc_fn = cjson_psram_malloc,
        .free_fn = free,
    };
    cJSON_InitHooks(&hooks);
    installed = true;
    ESP_LOGI(TAG, "cJSON hooks installed (PSRAM-prefer)");
}

// ---------------------------------------------------------------------------
// Network provisioning event handlers
// ---------------------------------------------------------------------------
static std::expected<std::unique_ptr<PairingQrImage>, std::string> render_qr_image(std::string_view qr_code);
static void app_network_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_WIFI_CRED_RECV:
            ESP_LOGI(TAG, "WiFi credentials received via provisioning");
            break;
        case NETWORK_PROV_WIFI_CRED_SUCCESS: {
            std::lock_guard<std::mutex> lock(s_runtime_mutex);
            s_runtime_state.network_provisioned = true;
            s_pairing_qr_code.clear();
            s_pairing_manual_code.clear();
            s_pairing_setup_pin.clear();
            // Keep an already displayed image alive until the UI unregisters
            // it. This prevents a runtime image descriptor from referencing
            // freed pixels during the success-state transition.
            ESP_LOGI(TAG, "Controller network commissioning completed");
            break;
        }
        default:
            break;
        }
        return;
    }

    if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        if (event_id == PROTOCOMM_SECURITY_SESSION_SETUP_OK) {
            ESP_LOGI(TAG, "Secure provisioning session established");
        }
        return;
    }

    if (event_base != APP_NETWORK_EVENT) {
        return;
    }

    switch (event_id) {
    case APP_NETWORK_EVENT_QR_DISPLAY: {
        // Store the RainMaker provisioning URL, but do not render the bitmap
        // here. The QR page is rarely visible during normal operation and an
        // always-resident 226x226 ARGB image consumed about 200 KiB before the
        // Matter Controller UI was even opened. Render it lazily on demand.
        const char *qr_url = static_cast<const char *>(event_data);
        if (qr_url == nullptr || qr_url[0] == '\0') {
            ESP_LOGW(TAG, "Provisioning QR event with empty payload");
            break;
        }
        ESP_LOGI(TAG, "Provisioning QR URL: %s", qr_url);

        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        s_runtime_state.network_provisioned = false;
        s_pairing_qr_code = qr_url;
        s_pairing_manual_code.clear();     // not applicable for RainMaker provisioning
        s_pairing_setup_pin.clear();       // not applicable
        s_pairing_qr_image.reset();
        ESP_LOGI(TAG, "Provisioning QR payload cached; image will be rendered when Pairing is opened");
        break;
    }
    case APP_NETWORK_EVENT_PROV_TIMEOUT:
        ESP_LOGW(TAG, "Provisioning timed out");
        break;
    case APP_NETWORK_EVENT_PROV_RESTART:
        ESP_LOGI(TAG, "Provisioning restarted");
        break;
    case APP_NETWORK_EVENT_PROV_CRED_MISMATCH:
        ESP_LOGW(TAG, "Provisioning credential mismatch");
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Matter report callback — dispatched from RainMaker cloud for attribute
// reports and online status changes.
// ---------------------------------------------------------------------------
static bool parse_report_key(const char *text, int base, uint64_t maximum, uint64_t &value)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, base);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) {
        return false;
    }
    value = static_cast<uint64_t>(parsed);
    return true;
}

static void on_matter_report(const app_rmaker_matter_report_t *report, void *priv_data)
{
    (void)priv_data;
    if (report == nullptr || report->data == nullptr) {
        return;
    }

    if (report->type == APP_RMAKER_MATTER_REPORT_ONLINE) {
        const cJSON *online_item = cJSON_GetObjectItemCaseSensitive(report->data, "online");
        if (cJSON_IsBool(online_item)) {
            const bool online = cJSON_IsTrue(online_item);
            ESP_LOGI(
                TAG, "Node 0x%016" PRIx64 " is now %s",
                report->node_id, online ? "online" : "offline"
            );
            esp_brookesia::app::matter_controller::notify_matter_node_reachability(report->node_id, online);
        }
        return;
    }

    if (report->type != APP_RMAKER_MATTER_REPORT_ATTR || !cJSON_IsObject(report->data)) {
        return;
    }

    size_t forwarded = 0;
    const cJSON *node_obj = nullptr;
    cJSON_ArrayForEach(node_obj, report->data) {
        uint64_t node_id = 0;
        if (!cJSON_IsObject(node_obj) || !parse_report_key(node_obj->string, 16, UINT64_MAX, node_id)) {
            continue;
        }
        const cJSON *endpoint_obj = nullptr;
        cJSON_ArrayForEach(endpoint_obj, node_obj) {
            uint64_t endpoint_id = 0;
            if (!cJSON_IsObject(endpoint_obj) ||
                    !parse_report_key(endpoint_obj->string, 0, UINT16_MAX, endpoint_id)) {
                continue;
            }
            const cJSON *cluster_obj = nullptr;
            cJSON_ArrayForEach(cluster_obj, endpoint_obj) {
                uint64_t cluster_id = 0;
                if (!cJSON_IsObject(cluster_obj) ||
                        !parse_report_key(cluster_obj->string, 0, UINT32_MAX, cluster_id)) {
                    continue;
                }
                const cJSON *attribute_obj = nullptr;
                cJSON_ArrayForEach(attribute_obj, cluster_obj) {
                    uint64_t attribute_id = 0;
                    if (!cJSON_IsObject(attribute_obj) ||
                            !parse_report_key(attribute_obj->string, 0, UINT32_MAX, attribute_id)) {
                        continue;
                    }
                    const cJSON *value = cJSON_GetObjectItemCaseSensitive(attribute_obj, "value");
                    int64_t scalar = 0;
                    if (cJSON_IsBool(value)) {
                        scalar = cJSON_IsTrue(value) ? 1 : 0;
                    } else if (cJSON_IsNumber(value)) {
                        scalar = static_cast<int64_t>(value->valuedouble);
                    } else {
                        continue;
                    }
                    esp_brookesia::app::matter_controller::notify_matter_attribute_update(
                        node_id, static_cast<uint16_t>(endpoint_id), static_cast<uint32_t>(cluster_id),
                        static_cast<uint32_t>(attribute_id), scalar
                    );
                    ++forwarded;
                }
            }
        }
    }
    if (forwarded != 0) {
        ESP_LOGI(TAG, "Forwarded %u Matter attribute update(s) to UI backend", static_cast<unsigned>(forwarded));
    }
}

std::string format_setup_pin(uint32_t setup_pin)
{
    char buffer[9] = {};
    std::snprintf(buffer, sizeof(buffer), "%08u", static_cast<unsigned>(setup_pin));
    return std::string(buffer);
}

std::expected<std::unique_ptr<PairingQrImage>, std::string> render_qr_image(std::string_view qr_code)
{
#if !defined(LV_USE_QRCODE) || !LV_USE_QRCODE
    return std::unexpected("LV_USE_QRCODE is disabled");
#else
    if (qr_code.empty()) {
        return std::unexpected("Matter QR payload is empty");
    }

    std::vector<uint8_t> temp_buffer(qrcodegen_BUFFER_LEN_MAX, 0);
    std::vector<uint8_t> qr_buffer(qrcodegen_BUFFER_LEN_MAX, 0);
    if (!qrcodegen_encodeText(
                std::string(qr_code).c_str(),
                temp_buffer.data(),
                qr_buffer.data(),
                qrcodegen_Ecc_MEDIUM,
                qrcodegen_VERSION_MIN,
                qrcodegen_VERSION_MAX,
                qrcodegen_Mask_AUTO,
                true
            )) {
        return std::unexpected("Failed to encode Matter QR payload");
    }

    const int qr_size = qrcodegen_getSize(qr_buffer.data());
    if (qr_size <= 0) {
        return std::unexpected("Generated Matter QR payload has invalid size");
    }

    const int total_modules = qr_size + (QR_QUIET_ZONE_MODULES * 2);
    const int scale = std::max(1, static_cast<int>(QR_IMAGE_SIZE / total_modules));
    const int rendered_size = total_modules * scale;
    const int offset = (QR_IMAGE_SIZE - rendered_size) / 2;

    auto image = std::make_unique<PairingQrImage>();
    // The QR code is fully opaque black and white, so ARGB8888 wastes half the
    // memory. RGB565 is supported natively by LVGL and reduces the image buffer
    // from 204,304 bytes to 102,152 bytes.
    image->pixels.resize(static_cast<size_t>(QR_IMAGE_SIZE) * static_cast<size_t>(QR_IMAGE_SIZE) * 2U, 0xFF);

    for (int y = 0; y < qr_size; ++y) {
        for (int x = 0; x < qr_size; ++x) {
            if (!qrcodegen_getModule(qr_buffer.data(), x, y)) {
                continue;
            }
            const int start_x = offset + ((x + QR_QUIET_ZONE_MODULES) * scale);
            const int start_y = offset + ((y + QR_QUIET_ZONE_MODULES) * scale);
            for (int py = 0; py < scale; ++py) {
                for (int px = 0; px < scale; ++px) {
                    const int image_x = start_x + px;
                    const int image_y = start_y + py;
                    if (image_x < 0 || image_x >= QR_IMAGE_SIZE || image_y < 0 || image_y >= QR_IMAGE_SIZE) {
                        continue;
                    }
                    const size_t index =
                        (static_cast<size_t>(image_y) * static_cast<size_t>(QR_IMAGE_SIZE) + static_cast<size_t>(image_x))
                        * 2U;
                    image->pixels[index + 0] = 0x00;
                    image->pixels[index + 1] = 0x00;
                }
            }
        }
    }

    image->descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    image->descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
    image->descriptor.header.flags = 0;
    image->descriptor.header.w = QR_IMAGE_SIZE;
    image->descriptor.header.h = QR_IMAGE_SIZE;
    image->descriptor.header.stride = static_cast<uint16_t>(QR_IMAGE_SIZE * 2);
    image->descriptor.data_size = static_cast<uint32_t>(image->pixels.size());
    image->descriptor.data = image->pixels.data();
    image->descriptor.reserved = nullptr;
    image->descriptor.reserved_2 = nullptr;
    return image;
#endif
}

esp_err_t ensure_controller_endpoint()
{
#if !CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER
    return ESP_OK;
#else
    {
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        if (s_runtime_state.controller_endpoint_ready) {
            return ESP_OK;
        }
    }

    esp_matter::node_t *node = esp_matter::node::get();
    if (node == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_matter::endpoint_t *controller_endpoint =
        esp_matter::endpoint::create(node, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (controller_endpoint == nullptr) {
        ESP_LOGE(TAG, "Failed to create controller endpoint");
        return ESP_ERR_NO_MEM;
    }

    esp_matter::endpoint::add_device_type(
        controller_endpoint, CONTROLLER_DEVICE_TYPE_ID, CONTROLLER_DEVICE_TYPE_VERSION
    );
    esp_matter::cluster::descriptor::config_t descriptor_config;
    if (esp_matter::cluster::descriptor::create(
                controller_endpoint,
                &descriptor_config,
                esp_matter::CLUSTER_FLAG_SERVER
            ) == nullptr) {
        ESP_LOGE(TAG, "Failed to create descriptor cluster for controller endpoint");
        return ESP_FAIL;
    }

    const auto endpoint_id = esp_matter::endpoint::get_id(controller_endpoint);
    {
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        s_runtime_state.controller_endpoint_id = endpoint_id;
        s_runtime_state.controller_endpoint_ready = true;
    }
    ESP_LOGI(TAG, "Controller endpoint created: %u", endpoint_id);
    return ESP_OK;
#endif
}

// ---------------------------------------------------------------------------
// Custom setup callback that handles reboot by reusing the stored fabric
// ---------------------------------------------------------------------------
static esp_err_t custom_setup_callback(uint8_t *ipk, size_t ipk_len, uint64_t fabric_id)
{
    (void)fabric_id;
    chip::MutableByteSpan ipk_span(ipk, ipk_len);
    esp_err_t err = ESP_OK;
    {
        esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
        auto &client = esp_matter::controller::matter_controller_client::get_instance();
        client.init(0, 0, 5580);

        if (ipk == nullptr || ipk_len == 0) {
            // Reboot case: look up the existing fabric index instead of
            // generating a new NOC chain.
            auto &factory = chip::Controller::DeviceControllerFactory::GetInstance();
            auto *ft = factory.GetSystemState() ? factory.GetSystemState()->Fabrics() : nullptr;
            chip::FabricIndex stored_idx = chip::kUndefinedFabricIndex;
            if (ft) {
                for (chip::FabricIndex i = chip::kMinValidFabricIndex;
                     i <= chip::kMaxValidFabricIndex; ++i) {
                    if (ft->FindFabricWithIndex(i) != nullptr) {
                        stored_idx = i;
                        break;
                    }
                }
            }
            err = client.setup_controller(ipk_span, stored_idx);
        } else {
            // First-time setup: generate NOC chain from RainMaker cloud
            err = client.setup_controller(ipk_span);
        }
    }
    if (err == ESP_OK) {
        app_rmaker_matter_device_list_update();
    }
    return err;
}

// ---------------------------------------------------------------------------
// Bridge from app_controller device-list callback to EspMatterBackend
// ---------------------------------------------------------------------------
static void on_device_list_update_callback(esp_err_t err, const matter_device_t *dev_list)
{
    esp_brookesia::app::matter_controller::notify_matter_device_list_update(err, dev_list);
}

// ---------------------------------------------------------------------------
// RainMaker Agent
// ---------------------------------------------------------------------------
esp_err_t ensure_rainmaker_agent()
{
    {
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        if (s_runtime_state.rainmaker_started) {
            return ESP_OK;
        }
    }

    install_cjson_hooks();

    // --- Initialize network provisioning ---
    // Must be called before RainMaker node init, matching the reference
    // app_main() order. This initializes the Wi-Fi stack and the network
    // provisioning manager (BLE advertising for RainMaker app discovery).
    app_network_init();
    bool network_provisioned = false;
#if defined(CONFIG_NETWORK_PROV_NETWORK_TYPE_WIFI)
    const esp_err_t provisioned_err = network_prov_mgr_is_wifi_provisioned(&network_provisioned);
#elif defined(CONFIG_NETWORK_PROV_NETWORK_TYPE_THREAD)
    const esp_err_t provisioned_err = network_prov_mgr_is_thread_provisioned(&network_provisioned);
#else
    const esp_err_t provisioned_err = ESP_ERR_NOT_SUPPORTED;
#endif
    if (provisioned_err == ESP_OK) {
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        s_runtime_state.network_provisioned = network_provisioned;
    } else {
        ESP_LOGW(TAG, "Unable to query controller network provisioning state: %s",
                 esp_err_to_name(provisioned_err));
    }
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID,
                                                &app_network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                &app_network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID,
                                                &app_network_event_handler, NULL));

    // --- Create RainMaker node ---
    esp_rmaker_config_t rainmaker_config = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_config, RMAKER_NODE_NAME, RMAKER_NODE_TYPE);
    if (node == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize RainMaker node");
        return ESP_FAIL;
    }

    // --- System service ---
    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 0,
        .reset_seconds = 2,
        .reset_reboot_seconds = 0,
    };
    esp_rmaker_system_service_enable(&system_serv_config);

    // --- MatterController device ---
    esp_rmaker_device_t *device = esp_rmaker_device_create(RMAKER_DEVICE_NAME, "matter-controller", nullptr);
    if (device == nullptr) {
        ESP_LOGE(TAG, "Failed to create RainMaker controller device");
        return ESP_FAIL;
    }
    // Use the shared app_controller helper to add standard controller params (name, etc.)
    ESP_RETURN_ON_ERROR(app_controller_set_device_params(device), TAG, "Failed to set controller device params");
    // Add a power param as the primary (required for RainMaker node)
    esp_rmaker_param_t *primary = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, RMAKER_DEFAULT_POWER);
    if (primary != nullptr) {
        esp_rmaker_device_add_param(device, primary);
        esp_rmaker_device_assign_primary_param(device, primary);
    }
    esp_rmaker_device_add_cb(device, rmaker_device_write_cb, nullptr);
    ESP_RETURN_ON_ERROR(esp_rmaker_node_add_device(node, device), TAG, "Failed to add RainMaker device");

    // --- Standard RainMaker services ---
    // OTA is intentionally disabled for this UI-only demo.
    esp_rmaker_timezone_service_enable();
    esp_rmaker_schedule_enable();
    esp_rmaker_scenes_enable();

    // --- Initialize Matter controller ---
    app_controller_register_op_creds_issuer();

    matter_controller_config_t controller_config = {
        .setup_callback = custom_setup_callback,
        .update_noc_callback = app_controller_update_noc,
        .device_list_update_callback = on_device_list_update_callback,
    };
    ESP_RETURN_ON_ERROR(
        app_rmaker_matter_controller_enable(&controller_config),
        TAG, "Failed to enable Matter controller"
    );

    ESP_RETURN_ON_ERROR(
        app_controller_init(on_device_list_update_callback),
        TAG, "Failed to initialize Matter controller"
    );

    // --- Register Matter report callback ---
    // Receives attribute reports and online/offline notifications from the
    // RainMaker cloud. Must be set after app_controller_init() and before
    // esp_rmaker_start().
    ESP_RETURN_ON_ERROR(
        app_rmaker_matter_report_set_callback(on_matter_report, NULL),
        TAG, "Failed to set Matter report callback"
    );

    // --- Enable auth service (must be before esp_rmaker_start) ---
    esp_rmaker_auth_service_enable();

    // --- Start RainMaker ---
    ESP_RETURN_ON_ERROR(esp_rmaker_start(), TAG, "Failed to start RainMaker agent");

    // --- Set custom manufacturing data + start network provisioning ---
    // The manufacturing data identifies this device as a Matter controller so
    // the RainMaker app can filter and display it correctly during BLE discovery.
    // app_network_start() starts BLE advertising (if not provisioned) or
    // connects to Wi-Fi (if already provisioned). Must be called after
    // esp_rmaker_start(), matching the reference order.
    //
    // Use POP_TYPE_MAC on first boot (fctry NVS partition is empty before
    // claiming). After the user claims the device via RainMaker app, the
    // cloud pushes a random PoP into fctry and subsequent calls can use
    // POP_TYPE_RANDOM. For development, POP_TYPE_MAC is sufficient.
    ESP_RETURN_ON_ERROR(
        app_network_set_custom_mfg_data(MFG_DATA_DEVICE_TYPE_MATTER_CONTROLLER,
                                        MFG_DATA_DEVICE_SUBTYPE_MATTER_CONTROLLER),
        TAG, "Failed to set custom manufacturing data"
    );
    esp_err_t net_err = app_network_start(POP_TYPE_MAC);
    if (net_err != ESP_OK) {
        ESP_LOGW(TAG, "Network provisioning start returned: %s — continuing anyway", esp_err_to_name(net_err));
    }

    {
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        s_runtime_state.rainmaker_started = true;
    }
    return ESP_OK;
}

std::expected<void, std::string> ensure_pairing_payload_locked()
{
    if (!s_pairing_qr_code.empty() && !s_pairing_manual_code.empty() && !s_pairing_setup_pin.empty() &&
            s_pairing_qr_image != nullptr) {
        return {};
    }

    chip::PayloadContents payload;
    if (const auto err = GetPayloadContents(payload, get_rendezvous_flags()); err != CHIP_NO_ERROR) {
        return std::unexpected("Failed to build Matter payload contents");
    }

    char qr_buffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1] = {};
    chip::MutableCharSpan qr_span(qr_buffer);
    if (const auto err = GetQRCode(qr_span, payload); err != CHIP_NO_ERROR) {
        return std::unexpected("Failed to generate Matter QR string");
    }

    char manual_buffer[64] = {};
    chip::MutableCharSpan manual_span(manual_buffer);
    if (const auto err = GetManualPairingCode(manual_span, payload); err != CHIP_NO_ERROR) {
        return std::unexpected("Failed to generate Matter manual pairing code");
    }

    auto qr_image = render_qr_image(std::string_view(qr_span.data(), qr_span.size()));
    if (!qr_image) {
        return std::unexpected(qr_image.error());
    }

    s_pairing_qr_code.assign(qr_span.data(), qr_span.size());
    s_pairing_manual_code.assign(manual_span.data(), manual_span.size());
    s_pairing_setup_pin = format_setup_pin(payload.setUpPINCode);
    s_pairing_qr_image = std::move(*qr_image);

    ESP_LOGI(TAG, "Matter onboarding payload ready");
    ESP_LOGI(TAG, "SetupQRCode: %s", s_pairing_qr_code.c_str());
    ESP_LOGI(TAG, "ManualPairingCode: %s", s_pairing_manual_code.c_str());
    return {};
}

esp_err_t attribute_update_cb(
    esp_matter::attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
    esp_matter_attr_val_t *val, void *priv_data)
{
    (void)type;
    (void)endpoint_id;
    (void)cluster_id;
    (void)attribute_id;
    (void)val;
    (void)priv_data;
    return ESP_OK;
}

esp_err_t identification_cb(
    esp_matter::identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
    uint8_t effect_variant, void *priv_data)
{
    (void)type;
    (void)endpoint_id;
    (void)effect_id;
    (void)effect_variant;
    (void)priv_data;
    return ESP_OK;
}

void event_callback(const ChipDeviceEvent *event, intptr_t arg)
{
    (void)arg;
    if (event == nullptr) {
        return;
    }

    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Matter commissioning session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Matter commissioning session stopped");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Matter commissioning window opened");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Matter commissioning window closed");
        break;
    case chip::DeviceLayer::DeviceEventType::PublicEventTypes::kCommissioningComplete:
        ESP_LOGI(TAG, "Matter commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Matter commissioning fail-safe timer expired — commissioning failed");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Matter fabric committed");
        break;
    case chip::DeviceLayer::DeviceEventType::kWiFiConnectivityChange:
        ESP_LOGI(TAG, "Matter WiFi connectivity changed");
        break;
    default:
        break;
    }
}

} // namespace

namespace esp_brookesia::app::matter_controller {

std::expected<void, std::string> ensure_matter_runtime_started()
{
    bool need_matter_node = false;
    bool need_rainmaker = false;
    {
        // The start mutex only protects the flag snapshot. It must be
        // released before calling ensure_rainmaker_agent(), which
        // internally blocks on app_network_start(). Holding it across
        // the blocking call would deadlock the UI thread if it calls
        // get_matter_pairing_payload() during provisioning.
        std::lock_guard<std::mutex> start_lock(s_runtime_start_mutex);
        std::lock_guard<std::mutex> lock(s_runtime_mutex);
        need_matter_node = !s_runtime_state.matter_started;
        need_rainmaker = !s_runtime_state.rainmaker_started;
    }

    if (need_rainmaker) {
        if (const auto err = ensure_rainmaker_agent(); err != ESP_OK) {
            ESP_LOGW(TAG, "RainMaker agent unavailable: %s", esp_err_to_name(err));
        }
    }

    if (need_matter_node) {
        if (const auto err = esp_matter::start(event_callback); err != ESP_OK) {
            return std::unexpected("Failed to start Matter stack: " + std::string(esp_err_to_name(err)));
        }
        {
            std::lock_guard<std::mutex> lock(s_runtime_mutex);
            s_runtime_state.matter_started = true;
        }

#if CONFIG_ENABLE_CHIP_SHELL
        esp_matter::console::diagnostics_register_commands();
        esp_matter::console::init();
        esp_matter::console::controller_register_commands();
#endif
    }

    return {};
}

MatterRuntimeState get_matter_runtime_state()
{
    std::lock_guard<std::mutex> lock(s_runtime_mutex);
    return s_runtime_state;
}

std::expected<MatterPairingPayload, std::string> get_matter_pairing_payload()
{
    // Return the cached RainMaker provisioning QR (generated when
    // APP_NETWORK_EVENT_QR_DISPLAY fires). Do NOT call
    // ensure_matter_runtime_started() here — it would deadlock if the
    // setup task is currently blocked inside app_network_start().
    std::lock_guard<std::mutex> lock(s_runtime_mutex);

    if (s_pairing_qr_code.empty()) {
        return std::unexpected("Provisioning QR not yet available — waiting for BLE advertising to start");
    }

    if (s_pairing_qr_image == nullptr) {
        auto qr_image = render_qr_image(s_pairing_qr_code);
        if (!qr_image) {
            return std::unexpected(qr_image.error());
        }
        s_pairing_qr_image = std::move(*qr_image);
        ESP_LOGI(TAG, "Provisioning QR image rendered on demand (%dx%d, RGB565, %u bytes)",
                 QR_IMAGE_SIZE, QR_IMAGE_SIZE,
                 static_cast<unsigned>(s_pairing_qr_image->pixels.size()));
    }

    return MatterPairingPayload{
        .available = true,
        .qr_code = s_pairing_qr_code,
        .manual_pairing_code = s_pairing_manual_code,
        .setup_pin_code = s_pairing_setup_pin,
        .qr_native_src = reinterpret_cast<uintptr_t>(&s_pairing_qr_image->descriptor),
        .qr_width = QR_IMAGE_SIZE,
        .qr_height = QR_IMAGE_SIZE,
    };
}

void release_matter_pairing_qr_image()
{
    std::lock_guard<std::mutex> lock(s_runtime_mutex);
    if (s_pairing_qr_image != nullptr) {
        ESP_LOGI(TAG, "Releasing provisioning QR image (%u bytes)",
                 static_cast<unsigned>(s_pairing_qr_image->pixels.size()));
        s_pairing_qr_image.reset();
    }
}

std::expected<void, std::string> request_matter_factory_reset()
{
    const auto start_result = ensure_matter_runtime_started();
    if (!start_result) {
        return std::unexpected(start_result.error());
    }

    // Wipe the Wi-Fi station credentials on the co-processor before scheduling
    // the Matter factory reset. On esp32_p4_function_ev the SSID/password live
    // in the on-board ESP32-C6 NVS (forwarded via esp_wifi_remote/esp_hosted),
    // and `idf.py erase-flash` only clears the P4 flash. Without this call,
    // a "Settings -> Reset" would still leave the device auto-reconnecting to
    // the previous network on the next boot.
    if (const auto err = esp_wifi_restore(); err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_restore() before factory reset returned: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Wi-Fi station credentials cleared on co-processor");
    }

    if (const auto err = esp_matter::factory_reset(); err != ESP_OK) {
        return std::unexpected("Failed to schedule Matter factory reset: " + std::string(esp_err_to_name(err)));
    }
    return {};
}

} // namespace esp_brookesia::app::matter_controller

#else

namespace esp_brookesia::app::matter_controller {

std::expected<void, std::string> ensure_matter_runtime_started()
{
    return std::unexpected("Matter controller runtime is unavailable in the current build");
}

MatterRuntimeState get_matter_runtime_state()
{
    return {};
}

std::expected<MatterPairingPayload, std::string> get_matter_pairing_payload()
{
    return std::unexpected("Matter pairing payload is unavailable in the current build");
}

void release_matter_pairing_qr_image()
{}

std::expected<void, std::string> request_matter_factory_reset()
{
    return std::unexpected("Matter factory reset is unavailable in the current build");
}

} // namespace esp_brookesia::app::matter_controller

#endif
