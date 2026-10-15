/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <span>
#include <vector>
#include "boost/json.hpp"
#include "brookesia/gui_gsp/desktop_display.hpp"
#include "esp_gsp.h"

namespace esp_brookesia::gui::gsp {

struct DesktopBundle {
    std::string path;
    std::string sidecar_path;
    std::vector<std::byte> bytes;
    boost::json::object metadata;
};

std::shared_ptr<const DesktopBundle> find_desktop_bundle(std::span<const std::byte> bytes);
uint32_t desktop_name_key(std::string_view name);
esp_gsp_err_t desktop_invoke(esp_gsp_handle_t handle, std::function<esp_gsp_err_t()> operation);
esp_gsp_err_t desktop_property_info(esp_gsp_handle_t handle, gsp_component_key_t component,
                                    gsp_property_key_t property, gsp_property_info_t *info);

class DesktopDisplay::Impl {
public:
    explicit Impl(Config config) : config(std::move(config)) {}
    std::expected<std::string, std::string> start(const DesktopBundle &bundle);
    void stop();
    Config config;
    mutable std::mutex mutex;
    std::string url;
    std::string failure;
    std::string run_dir;
    std::vector<std::shared_ptr<const DesktopBundle>> bundles;
    int pid = -1;
};

} // namespace esp_brookesia::gui::gsp
