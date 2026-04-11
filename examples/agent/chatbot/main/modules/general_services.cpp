/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "sdkconfig.h"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_helper.hpp"
#include "private/utils.hpp"
#include "general_services.hpp"

using namespace esp_brookesia;

using SNTPHelper = esp_brookesia::service::helper::SNTP;
using AudioHelper = esp_brookesia::service::helper::Audio;
using NVSHelper = esp_brookesia::service::helper::NVS;
using WifiHelper = esp_brookesia::service::helper::Wifi;

bool GeneralServices::init(std::shared_ptr<esp_brookesia::lib_utils::TaskScheduler> task_scheduler)
{
    BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Task scheduler is not available");

    if (is_initialized()) {
        BROOKESIA_LOGW("General services is already initialized");
        return true;
    }

    BROOKESIA_LOGI("Initializing service manager...");

    auto &service_manager = service::ServiceManager::get_instance();
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    task_scheduler_ = task_scheduler;

    BROOKESIA_LOGI("Service manager started successfully");

    return true;
}

void GeneralServices::start_sntp()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not available");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(SNTPHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind SNTP service");
    } else {
        service_bindings_.push_back(std::move(binding));
    }
}

void GeneralServices::start_nvs()
{
    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "General services is not initialized");

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGW("NVS service is not available");
        return;
    }

    auto &service_manager = service::ServiceManager::get_instance();
    auto binding = service_manager.bind(NVSHelper::get_name().data());
    if (!binding.is_valid()) {
        BROOKESIA_LOGE("Failed to bind NVS service");
    } else {
        service_bindings_.push_back(std::move(binding));
    }
}
