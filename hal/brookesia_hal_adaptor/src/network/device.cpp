/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor/network/device.hpp"
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
#   include "sntp_client_impl.hpp"
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
#   include "http_client_impl.hpp"
#endif

namespace esp_brookesia::hal {

bool NetworkDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

std::vector<InterfaceSpec> NetworkDevice::get_interface_specs() const
{
    std::vector<InterfaceSpec> specs;
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    specs.push_back({network::SntpClientIface::NAME, SNTP_CLIENT_IFACE_NAME});
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    specs.push_back({network::HttpClientIface::NAME, HTTP_CLIENT_IFACE_NAME});
#endif
    return specs;
}

bool NetworkDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_sntp_client(), false, "Failed to init SNTP client");
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    BROOKESIA_CHECK_FALSE_RETURN(init_http_client(), false, "Failed to init HTTP client");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid network interfaces initialized");

    return true;
}

void NetworkDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
    deinit_sntp_client();
#endif
#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
    deinit_http_client();
#endif
}

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_SNTP_CLIENT_IMPL
bool NetworkDevice::init_sntp_client()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(SNTP_CLIENT_IFACE_NAME)) {
        BROOKESIA_LOGD("SNTP client is already initialized, skip");
        return true;
    }

    std::shared_ptr<SntpClientAdaptorIface> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<SntpClientAdaptorIface>(), false, "Failed to create SNTP client interface"
    );
    interfaces_.emplace(SNTP_CLIENT_IFACE_NAME, iface);

    return true;
}

void NetworkDevice::deinit_sntp_client()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(SNTP_CLIENT_IFACE_NAME)) {
        BROOKESIA_LOGD("SNTP client is not initialized, skip");
        return;
    }

    interfaces_.erase(SNTP_CLIENT_IFACE_NAME);
}
#endif

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
bool NetworkDevice::init_http_client()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(HTTP_CLIENT_IFACE_NAME)) {
        BROOKESIA_LOGD("HTTP client is already initialized, skip");
        return true;
    }

    std::shared_ptr<HttpClientAdaptorIface> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<HttpClientAdaptorIface>(), false, "Failed to create HTTP client interface"
    );
    interfaces_.emplace(HTTP_CLIENT_IFACE_NAME, iface);

    return true;
}

void NetworkDevice::deinit_http_client()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(HTTP_CLIENT_IFACE_NAME)) {
        BROOKESIA_LOGD("HTTP client is not initialized, skip");
        return;
    }

    interfaces_.erase(HTTP_CLIENT_IFACE_NAME);
}
#endif

#if BROOKESIA_HAL_ADAPTOR_ENABLE_NETWORK_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, NetworkDevice, NetworkDevice::DEVICE_NAME, NetworkDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_NETWORK_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
