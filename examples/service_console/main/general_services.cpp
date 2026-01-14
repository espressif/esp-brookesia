/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#include "general_services.hpp"

using namespace esp_brookesia;

using NVSHelper = service::helper::NVS;
using SNTPHelper = service::helper::SNTP;

/* Service manager instance */
static service::ServiceManager &service_manager = service::ServiceManager::get_instance();
/* Keep service bindings to avoid frequent start and stop of services */
static std::vector<service::ServiceBinding> service_bindings;

static void services_nvs_init();
static void services_sntp_init();

void general_services_init()
{
    BROOKESIA_CHECK_FALSE_EXIT(service_manager.start(), "Failed to start service manager");

    services_nvs_init();
    services_sntp_init();
}

static void services_nvs_init()
{
    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGW("NVS service is not enabled");
        return;
    }

    auto binding = service_manager.bind(NVSHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind NVS service");
    } else {
        service_bindings.push_back(std::move(binding));
    }
}

static void services_sntp_init()
{
    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not enabled");
        return;
    }

    auto binding = service_manager.bind(SNTPHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind SNTP service");
    } else {
        service_bindings.push_back(std::move(binding));
    }
}
