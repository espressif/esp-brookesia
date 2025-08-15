/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <unistd.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#   include "soc/usb_serial_jtag_reg.h"
#   include "hal/usb_serial_jtag_ll.h"
#endif
#include "esp_private/usb_phy.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"
#include "lvgl.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:USBD_NCM"
#include "esp_lib_utils.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#if (CONFIG_TINYUSB_NET_MODE_NONE != 1)
#include "tinyusb_net.h"
#endif
#include "ui/ui.h"
#include "esp_brookesia_app_usbd_ncm.hpp"

using namespace esp_brookesia::systems::speaker;

#define APP_NAME "UsbdNcm"

using namespace std;
LV_IMG_DECLARE(img_app_usbd_ncm);

// Global flag to protect TinyUSB access
static std::atomic<bool> g_tinyusb_ready{false};

// UI cache critical section macros
#define UI_CACHE_ENTER_CRITICAL()      portENTER_CRITICAL(&_ui_cache_lock)
#define UI_CACHE_EXIT_CRITICAL()       portEXIT_CRITICAL(&_ui_cache_lock)

namespace esp_brookesia::apps {

enum {
    ITF_NUM_NET = 0,
    ITF_NUM_NET_DATA,
    ITF_NUM_TOTAL
};

enum {
    EP_EMPTY = 0,
    EPNUM_NET_NOTIF,
    EPNUM_NET_DATA,
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

static tusb_desc_device_t ncm_device_descriptor = {
    .bLength = sizeof(ncm_device_descriptor),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
#if CFG_TUD_CDC
    STRID_CDC_INTERFACE,
#endif

#if CFG_TUD_MSC
    STRID_MSC_INTERFACE,
#endif

#if CFG_TUD_NCM
    STRID_NET_INTERFACE,
    STRID_MAC,
#endif

#if CFG_TUD_VENDOR
    STRID_VENDOR_INTERFACE,
#endif
};

static uint8_t const ncm_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 64, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 64, CFG_TUD_NET_MTU),
};

constexpr esp_brookesia::systems::base::App::Config CORE_DATA = {
    .name = APP_NAME,
    .launcher_icon = gui::StyleImage::IMAGE(&img_app_usbd_ncm),
    .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
    .flags = {
        .enable_default_screen = 0,
        .enable_recycle_resource = 1,
        .enable_resize_visual_area = 1,
    },
};
constexpr speaker::App::Config APP_DATA = {
    .app_launcher_page_index = 0,
    .flags = {
        .enable_navigation_gesture = 1,
    },
};

UsbdNcm *UsbdNcm::_instance = nullptr;

static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    bool *is_wifi_connected = (bool *)ctx;
    if (*is_wifi_connected) {
        // Count uplink traffic (USB → WiFi)
        UsbdNcm::addUplinkBytes(len);
        esp_wifi_internal_tx(WIFI_IF_STA, buffer, len);
    }
    return ESP_OK;
}

static void wifi_pkt_free(void *eb, void *ctx)
{
    esp_wifi_internal_free_rx_buffer(eb);
}

static esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb)
{
    if (!g_tinyusb_ready.load()) {
        // TinyUSB is not ready or being shutdown, free the buffer and return
        esp_wifi_internal_free_rx_buffer(eb);
        return ESP_FAIL;
    }

    if (tinyusb_net_send_sync(buffer, len, eb, portMAX_DELAY) != ESP_OK) {
        esp_wifi_internal_free_rx_buffer(eb);
        return ESP_FAIL;
    }

    // Count downlink traffic (WiFi → USB), only after successful send
    UsbdNcm::addDownlinkBytes(len);

    return ESP_OK;
}

UsbdNcm *UsbdNcm::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new UsbdNcm();
    }
    return _instance;
}

UsbdNcm::UsbdNcm() : speaker::App(CORE_DATA, APP_DATA)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    _wifi_event_handler_instance = nullptr;
    _ip_event_handler_instance = nullptr;

    esp_err_t ret = esp_event_handler_instance_register(
                        WIFI_EVENT,
                        ESP_EVENT_ANY_ID,
                        onWiFiEventHandler,
                        this,
                        &_wifi_event_handler_instance
                    );

    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed to register WiFi event handler: %s", esp_err_to_name(ret));
    } else {
        ESP_UTILS_LOGI("WiFi event handler registered successfully");
    }

    ret = esp_event_handler_instance_register(
              IP_EVENT,
              ESP_EVENT_ANY_ID,
              onWiFiEventHandler,
              this,
              &_ip_event_handler_instance
          );

    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed to register IP event handler: %s", esp_err_to_name(ret));
    } else {
        ESP_UTILS_LOGI("IP event handler registered successfully");
    }

}

UsbdNcm::~UsbdNcm()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_wifi_event_handler_instance != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _wifi_event_handler_instance);
        _wifi_event_handler_instance = nullptr;
        ESP_UTILS_LOGI("WiFi event handler unregistered");
    }

    if (_ip_event_handler_instance != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, _ip_event_handler_instance);
        _ip_event_handler_instance = nullptr;
        ESP_UTILS_LOGI("IP event handler unregistered");
    }

    // Delete the ESP timer
    if (_usbd_ncm_timer != nullptr) {
        esp_timer_delete(_usbd_ncm_timer);
        _usbd_ncm_timer = nullptr;
        ESP_UTILS_LOGI("ESP timer deleted");
    }
}

bool UsbdNcm::init()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool UsbdNcm::run()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();
    ui_init();
    _app_opened = true;  // Mark APP as opened

    // Get and update MAC address
    if (esp_read_mac(_mac_addr, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(_mac_str, sizeof(_mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 _mac_addr[0], _mac_addr[1], _mac_addr[2], _mac_addr[3], _mac_addr[4], _mac_addr[5]);
        lv_label_set_text(ui_LabelMacAddr, _mac_str);
        ESP_UTILS_LOGI("Updated MAC address in UI: %s", _mac_str);
    } else {
        ESP_UTILS_LOGE("Failed to read MAC address");
    }

    // Check if cache has content and update UI immediately
    UI_CACHE_ENTER_CRITICAL();
    bool has_ip = strlen(_ui_cache.ip_address) > 0;
    bool has_upload = strlen(_ui_cache.upload_speed) > 0;
    bool has_download = strlen(_ui_cache.download_speed) > 0;
    bool has_status = strlen(_ui_cache.connection_status) > 0;

    if (has_ip) {
        lv_label_set_text(ui_LabelIpAddr, _ui_cache.ip_address);
    }
    if (has_upload) {
        lv_label_set_text(ui_LabelUpLoad, _ui_cache.upload_speed);
    }
    if (has_download) {
        lv_label_set_text(ui_LabelDownLoad, _ui_cache.download_speed);
    }
    if (has_status) {
        lv_label_set_text(ui_LabelStatus, _ui_cache.connection_status);
    }
    UI_CACHE_EXIT_CRITICAL();

    // Log outside critical section
    if (has_ip) {
        ESP_UTILS_LOGI("Updated IP address from cache: %s", _ui_cache.ip_address);
    }
    if (has_upload) {
        ESP_UTILS_LOGI("Updated upload speed from cache: %s", _ui_cache.upload_speed);
    }
    if (has_download) {
        ESP_UTILS_LOGI("Updated download speed from cache: %s", _ui_cache.download_speed);
    }
    if (has_status) {
        ESP_UTILS_LOGI("Updated connection status from cache: %s", _ui_cache.connection_status);
    }

    // Register button click event
    lv_obj_add_event_cb(ui_ButtonConnect, onConnectButtonClick, LV_EVENT_CLICKED, this);

    // Start UI refresh timer
    startUiRefreshTimer();

    return true;
}

bool UsbdNcm::back()
{
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
    return true;
}

bool UsbdNcm::close()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGI("Close requested - showing warning popup");
    showCloseWarningPopup();

    return false;  // Prevent app from closing
}

void UsbdNcm::onWiFiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto usbd_ncm = static_cast<UsbdNcm *>(arg);
    ESP_UTILS_CHECK_NULL_EXIT(usbd_ncm, "Invalid arg");

    ESP_UTILS_CHECK_FALSE_EXIT(
        usbd_ncm->processWiFiEvent(event_base, event_id, event_data),
        "Process WiFi event failed"
    );
}

bool UsbdNcm::processWiFiEvent(esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD("WiFi event: base=%s, id=%d", event_base, static_cast<int>(event_id));

    switch (event_id) {
    case WIFI_EVENT_STA_CONNECTED: {
        ESP_UTILS_LOGI("WiFi connected");
        _wifi_connected = true;
        UI_CACHE_ENTER_CRITICAL();
        strncpy(_ui_cache.connection_status, "Online", sizeof(_ui_cache.connection_status) - 1);
        _ui_cache.connection_status_updated = true;
        UI_CACHE_EXIT_CRITICAL();
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *data = static_cast<wifi_event_sta_disconnected_t *>(event_data);
        ESP_UTILS_LOGI("WiFi disconnected! SSID: %s, reason: %d", data->ssid, data->reason);
        _wifi_connected = false;
        UI_CACHE_ENTER_CRITICAL();
        strncpy(_ui_cache.connection_status, "Offline", sizeof(_ui_cache.connection_status) - 1);
        _ui_cache.connection_status_updated = true;
        memset(_ui_cache.ip_address, 0, sizeof(_ui_cache.ip_address));
        _ui_cache.ip_address_updated = true;
        UI_CACHE_EXIT_CRITICAL();
        break;
    }
    case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_UTILS_LOGI("WiFi got IP: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));

        // Cache IP address
        UI_CACHE_ENTER_CRITICAL();
        snprintf(_ui_cache.ip_address, sizeof(_ui_cache.ip_address), "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        _ui_cache.ip_address_updated = true;
        UI_CACHE_EXIT_CRITICAL();
        break;
    }
    default:
        break;
    }

    return true;
}

// LVGL timer implementation
void UsbdNcm::startUiRefreshTimer()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_ui_refresh_timer != nullptr) {
        ESP_UTILS_LOGD("UI refresh timer already running");
        return;
    }

    // Create LVGL timer, refresh every 100ms
    _ui_refresh_timer = lv_timer_create(uiRefreshTimerCallback, 100, this);
    if (_ui_refresh_timer == nullptr) {
        ESP_UTILS_LOGE("Failed to create UI refresh timer");
        return;
    }

    // Create and start ESP timer for network speed calculation
    if (_usbd_ncm_timer == nullptr) {
        esp_timer_create_args_t timer_args = {
            .callback = usbd_ncm_timer_callback,
            .arg = this,
            .name = "USBD NCM Timer",
        };
        esp_err_t ret = esp_timer_create(&timer_args, &_usbd_ncm_timer);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Failed to create USBD NCM timer: %s", esp_err_to_name(ret));
            return;
        } else {
            ESP_UTILS_LOGI("USBD NCM timer created successfully");
        }
    }

    // Start ESP timer
    esp_err_t ret = esp_timer_start_periodic(_usbd_ncm_timer, 1 * 1000 * 1000);
    if (ret != ESP_OK) {
        ESP_UTILS_LOGE("Failed to start USBD NCM timer: %s", esp_err_to_name(ret));
    } else {
        ESP_UTILS_LOGI("USBD NCM timer started successfully");
    }

    ESP_UTILS_LOGI("UI refresh timer started");
}

void UsbdNcm::stopUiRefreshTimer()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_ui_refresh_timer != nullptr) {
        lv_timer_del(_ui_refresh_timer);
        _ui_refresh_timer = nullptr;
        ESP_UTILS_LOGI("UI refresh timer stopped");
    }

    // Stop the ESP timer
    if (_usbd_ncm_timer != nullptr) {
        esp_timer_stop(_usbd_ncm_timer);
        ESP_UTILS_LOGI("ESP timer stopped");
    }
}

void UsbdNcm::updateUiFromCache()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (!_app_opened) {
        return;
    }

    UI_CACHE_ENTER_CRITICAL();
    // Update IP address if changed
    bool ip_updated = _ui_cache.ip_address_updated;
    bool status_updated = _ui_cache.connection_status_updated;
    bool speed_updated = _ui_cache.network_speed_updated;

    if (ip_updated) {
        lv_label_set_text(ui_LabelIpAddr, _ui_cache.ip_address);
        _ui_cache.ip_address_updated = false;
    }

    // Update connection status if changed
    if (status_updated) {
        lv_label_set_text(ui_LabelStatus, _ui_cache.connection_status);
        _ui_cache.connection_status_updated = false;
    }

    // Update network speed if changed
    if (speed_updated) {
        lv_label_set_text(ui_LabelUpLoad, _ui_cache.upload_speed);
        lv_label_set_text(ui_LabelDownLoad, _ui_cache.download_speed);
        _ui_cache.network_speed_updated = false;
    }
    UI_CACHE_EXIT_CRITICAL();

    // Log outside critical section
    if (ip_updated) {
        ESP_UTILS_LOGD("Updated IP address in UI: %s", _ui_cache.ip_address);
    }
    if (status_updated) {
        ESP_UTILS_LOGD("Updated connection status in UI: %s", _ui_cache.connection_status);
    }
}

void UsbdNcm::uiRefreshTimerCallback(lv_timer_t *timer)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto usbd_ncm = static_cast<UsbdNcm *>(lv_timer_get_user_data(timer));
    ESP_UTILS_CHECK_NULL_EXIT(usbd_ncm, "Invalid timer user data");

    usbd_ncm->updateUiFromCache();
}

void UsbdNcm::onConnectButtonClick(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto usbd_ncm = static_cast<UsbdNcm *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(usbd_ncm, "Invalid user data");

    usbd_ncm->handleConnectButtonClick();
}

void UsbdNcm::handleConnectButtonClick()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGI("Connect button clicked, WiFi status: %s, NCM status: %s",
                   _wifi_connected ? "Connected" : "Disconnected",
                   _usbd_ncm_started ? "Started" : "Stopped");

    if (!_wifi_connected) {
        ESP_UTILS_LOGI("WiFi is not connected, cannot start NCM");
        return;
    }

    if (_usbd_ncm_started) {
        // NCM is running, stop it
        ESP_UTILS_LOGI("Stopping NCM...");
        stopUsbdNcm();
        lv_label_set_text(ui_LabelConnect, "Start NCM");
    } else {
        // NCM is not running, start it
        ESP_UTILS_LOGI("Starting NCM...");
        startUsbdNcm();
        lv_label_set_text(ui_LabelConnect, "Stop NCM");
    }
}

void UsbdNcm::startUsbdNcm()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (!_wifi_connected) {
        ESP_UTILS_LOGE("WiFi is not connected");
        return;
    }

    if (_usbd_ncm_started) {
        ESP_UTILS_LOGE("USBD NCM is already started");
        return;
    }

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &ncm_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = ncm_fs_configuration_desc;


    if (tinyusb_driver_install(&tusb_cfg) != ESP_OK) {
        ESP_UTILS_LOGE("Failed to install TinyUSB driver");
        return;
    }

    tinyusb_net_config_t net_config = {
        .on_recv_callback = usb_recv_callback,
        .free_tx_buffer = wifi_pkt_free,
        .user_context = &_wifi_connected
    };
    esp_read_mac(net_config.mac_addr, ESP_MAC_WIFI_STA);
    uint8_t *mac = net_config.mac_addr;
    ESP_UTILS_LOGI("Network interface HW address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (tinyusb_net_init(&net_config) != ESP_OK) {
        ESP_UTILS_LOGE("Failed to initialize TinyUSB NCM driver");
        return;
    }

    esp_wifi_internal_reg_rxcb(WIFI_IF_STA, pkt_wifi2usb);

    // Initialize traffic counters
    portENTER_CRITICAL_SAFE(&_traffic_lock);
    _uplink_counter.bytes = 0;
    _downlink_counter.bytes = 0;
    portEXIT_CRITICAL_SAFE(&_traffic_lock);

    _usbd_ncm_started = true;
    g_tinyusb_ready.store(true);  // Set flag to allow TinyUSB access
    ESP_UTILS_LOGI("USBD NCM started successfully");
}

void UsbdNcm::stopUsbdNcm()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (!_usbd_ncm_started) {
        ESP_UTILS_LOGE("USBD NCM is not started");
        return;
    }

    // First, set flag to prevent new TinyUSB access
    g_tinyusb_ready.store(false);

    // Wait a bit for any ongoing TinyUSB operations to complete
    vTaskDelay(pdMS_TO_TICKS(50));

    tinyusb_net_deinit();

    if (tinyusb_driver_uninstall() != ESP_OK) {
        ESP_UTILS_LOGE("Failed to uninstall TinyUSB driver");
        return;
    }

    _usbd_ncm_started = false;

    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
    vTaskDelay(pdMS_TO_TICKS(10));
#if USB_SERIAL_JTAG_LL_EXT_PHY_SUPPORTED
    usb_serial_jtag_ll_phy_enable_external(false);  // Use internal PHY
    usb_serial_jtag_ll_phy_enable_pad(true);        // Enable USB PHY pads
#else // USB_SERIAL_JTAG_LL_EXT_PHY_SUPPORTED
    usb_serial_jtag_ll_phy_set_defaults();          // External PHY not supported. Set default values.
#endif // USB_WRAP_LL_EXT_PHY_SUPPORTED
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLDOWN);
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PAD_PULL_OVERRIDE);

    ESP_UTILS_LOGI("USBD NCM stopped successfully");
}

void UsbdNcm::addTrafficBytes(TrafficCounter &counter, uint32_t bytes)
{
    portENTER_CRITICAL_SAFE(&_traffic_lock);
    counter.bytes += bytes;
    portEXIT_CRITICAL_SAFE(&_traffic_lock);
}

void UsbdNcm::addUplinkBytes(uint32_t bytes)
{
    if (_instance != nullptr) {
        _instance->addTrafficBytes(_instance->_uplink_counter, bytes);
    }
}

void UsbdNcm::addDownlinkBytes(uint32_t bytes)
{
    if (_instance != nullptr) {
        _instance->addTrafficBytes(_instance->_downlink_counter, bytes);
    }
}

void UsbdNcm::calculateAndUpdateNetworkSpeed()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    uint64_t uplink_bytes = 0, downlink_bytes = 0;

    // Read and clear counters
    portENTER_CRITICAL_SAFE(&_traffic_lock);
    uplink_bytes = _uplink_counter.bytes;
    downlink_bytes = _downlink_counter.bytes;
    _uplink_counter.bytes = 0;
    _downlink_counter.bytes = 0;
    portEXIT_CRITICAL_SAFE(&_traffic_lock);

    // Calculate KiB/s (following reference code logic)
    double uplink_kibs = uplink_bytes / 1024.0;
    double downlink_kibs = downlink_bytes / 1024.0;

    // Update UI cache with separate upload and download speeds
    UI_CACHE_ENTER_CRITICAL();
    snprintf(_ui_cache.upload_speed, sizeof(_ui_cache.upload_speed),
             "%.1f KiB/s", uplink_kibs);
    snprintf(_ui_cache.download_speed, sizeof(_ui_cache.download_speed),
             "%.1f KiB/s", downlink_kibs);
    _ui_cache.network_speed_updated = true;
    UI_CACHE_EXIT_CRITICAL();
}

void UsbdNcm::usbd_ncm_timer_callback(void *arg)
{
    auto usbd_ncm = static_cast<UsbdNcm *>(arg);
    ESP_UTILS_CHECK_NULL_EXIT(usbd_ncm, "Invalid arg");

    // Only perform statistics when NCM is started and app is opened
    if (usbd_ncm->_usbd_ncm_started && usbd_ncm->_app_opened) {
        usbd_ncm->calculateAndUpdateNetworkSpeed();
    }
}

void UsbdNcm::showCloseWarningPopup()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_popup_container != nullptr) {
        ESP_UTILS_LOGD("Popup already showing");
        return;
    }

    // Create popup container
    _popup_container = lv_obj_create(lv_scr_act());
    if (!_popup_container) {
        ESP_UTILS_LOGE("Failed to create popup container");
        return;
    }

    // Set popup container style
    lv_obj_set_size(_popup_container, 280, 180);
    lv_obj_set_style_bg_color(_popup_container, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_bg_opa(_popup_container, LV_OPA_90, 0);
    lv_obj_set_style_radius(_popup_container, 12, 0);
    lv_obj_set_style_border_width(_popup_container, 2, 0);
    lv_obj_set_style_border_color(_popup_container, lv_color_hex(0x4A4A4A), 0);
    lv_obj_set_style_pad_all(_popup_container, 20, 0);
    lv_obj_set_style_shadow_width(_popup_container, 10, 0);
    lv_obj_set_style_shadow_opa(_popup_container, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(_popup_container, lv_color_black(), 0);
    lv_obj_align(_popup_container, LV_ALIGN_CENTER, 0, 0);

    // Create warning label
    _popup_label = lv_label_create(_popup_container);
    if (!_popup_label) {
        ESP_UTILS_LOGE("Failed to create popup label");
        lv_obj_del(_popup_container);
        _popup_container = nullptr;
        return;
    }

    lv_label_set_text(_popup_label, "Warning\n\nThis app can only be closed\nby restarting the device.\n\nPlease restart to exit.");
    lv_obj_set_style_text_color(_popup_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_popup_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(_popup_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_popup_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_popup_label, 240);
    lv_obj_align(_popup_label, LV_ALIGN_TOP_MID, 0, 10);

    // Create OK button
    _popup_button = lv_btn_create(_popup_container);
    if (!_popup_button) {
        ESP_UTILS_LOGE("Failed to create popup button");
        lv_obj_del(_popup_container);
        _popup_container = nullptr;
        _popup_label = nullptr;
        return;
    }

    lv_obj_set_size(_popup_button, 80, 32);
    lv_obj_set_style_bg_color(_popup_button, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_radius(_popup_button, 6, 0);
    lv_obj_align_to(_popup_button, _popup_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_obj_add_event_cb(_popup_button, onPopupButtonClick, LV_EVENT_CLICKED, this);

    // Create button label
    lv_obj_t *button_label = lv_label_create(_popup_button);
    lv_label_set_text(button_label, "OK");
    lv_obj_set_style_text_color(button_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_14, 0);
    lv_obj_center(button_label);

    ESP_UTILS_LOGI("Close warning popup shown");
}

void UsbdNcm::hideCloseWarningPopup()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (_popup_container != nullptr) {
        lv_obj_del(_popup_container);
        _popup_container = nullptr;
        _popup_label = nullptr;
        _popup_button = nullptr;
        ESP_UTILS_LOGI("Close warning popup hidden");
    }
}

void UsbdNcm::onPopupButtonClick(lv_event_t *e)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto usbd_ncm = static_cast<UsbdNcm *>(lv_event_get_user_data(e));
    ESP_UTILS_CHECK_NULL_EXIT(usbd_ncm, "Invalid user data");

    usbd_ncm->hideCloseWarningPopup();
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, UsbdNcm, APP_NAME, []()
{
    return std::shared_ptr<UsbdNcm>(UsbdNcm::requestInstance(), [](UsbdNcm * p) {});
})

} // namespace esp_brookesia::apps
