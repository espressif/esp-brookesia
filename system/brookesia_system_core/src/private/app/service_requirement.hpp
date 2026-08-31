/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::service {

class ServiceManager;

} // namespace esp_brookesia::service

namespace esp_brookesia::system::core::detail {

struct ServiceVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    bool operator==(const ServiceVersion &) const = default;
};

std::expected<ServiceVersion, std::string> parse_service_version(std::string_view version);

bool is_service_version_compatible(const ServiceVersion &required, const ServiceVersion &local);

std::expected<bool, std::string> is_service_version_compatible(
    std::string_view required,
    std::string_view local
);

struct ServiceRequirementFailure {
    AppManifestService requirement;
    bool registered = false;
    std::string local_version;
    std::string reason;
};

std::optional<ServiceRequirementFailure> evaluate_service_requirement(
    const AppManifestService &requirement,
    const service::ServiceManager &manager
);

std::vector<ServiceRequirementFailure> evaluate_service_requirements(
    const AppManifest &manifest,
    const service::ServiceManager &manager
);

std::string format_service_requirement_failures(
    const std::vector<ServiceRequirementFailure> &failures
);

} // namespace esp_brookesia::system::core::detail
