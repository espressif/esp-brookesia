/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <charconv>
#include <system_error>
#include <utility>

#include "brookesia/service_manager/service/manager.hpp"
#include "private/app/service_requirement.hpp"

namespace esp_brookesia::system::core::detail {
namespace {

std::expected<uint32_t, std::string> parse_version_part(std::string_view part);

} // namespace

std::expected<ServiceVersion, std::string> parse_service_version(std::string_view version)
{
    const auto first_dot = version.find('.');
    if (first_dot == std::string_view::npos) {
        return std::unexpected("must use numeric MAJOR.MINOR.PATCH format");
    }
    const auto second_dot = version.find('.', first_dot + 1);
    if ((second_dot == std::string_view::npos) ||
            (version.find('.', second_dot + 1) != std::string_view::npos)) {
        return std::unexpected("must use numeric MAJOR.MINOR.PATCH format");
    }

    auto major = parse_version_part(version.substr(0, first_dot));
    auto minor = parse_version_part(version.substr(first_dot + 1, second_dot - first_dot - 1));
    auto patch = parse_version_part(version.substr(second_dot + 1));
    if (!major || !minor || !patch) {
        return std::unexpected(
                   !major ? major.error() :
                   !minor ? minor.error() :
                   patch.error()
               );
    }

    return ServiceVersion{
        .major = *major,
        .minor = *minor,
        .patch = *patch,
    };
}

bool is_service_version_compatible(const ServiceVersion &required, const ServiceVersion &local)
{
    return (required.major == local.major) &&
           (required.minor == local.minor) &&
           (local.patch >= required.patch);
}

std::expected<bool, std::string> is_service_version_compatible(
    std::string_view required,
    std::string_view local
)
{
    auto required_version = parse_service_version(required);
    if (!required_version) {
        return std::unexpected("Required service version " + required_version.error());
    }
    auto local_version = parse_service_version(local);
    if (!local_version) {
        return std::unexpected("Local service version " + local_version.error());
    }
    return is_service_version_compatible(*required_version, *local_version);
}

std::optional<ServiceRequirementFailure> evaluate_service_requirement(
    const AppManifestService &requirement,
    const service::ServiceManager &manager
)
{
    auto service_info = manager.get_service_info(requirement.name);
    if (!service_info) {
        return ServiceRequirementFailure{
            .requirement = requirement,
            .registered = false,
            .local_version = {},
            .reason = "service is not registered",
        };
    }

    auto compatible = is_service_version_compatible(requirement.version, service_info->version);
    if (compatible && *compatible) {
        return std::nullopt;
    }
    return ServiceRequirementFailure{
        .requirement = requirement,
        .registered = true,
        .local_version = service_info->version,
        .reason = compatible ? "local version is incompatible" : compatible.error(),
    };
}

std::vector<ServiceRequirementFailure> evaluate_service_requirements(
    const AppManifest &manifest,
    const service::ServiceManager &manager
)
{
    std::vector<ServiceRequirementFailure> failures;
    for (const auto &requirement : manifest.services) {
        auto failure = evaluate_service_requirement(requirement, manager);
        if (failure) {
            failures.push_back(std::move(*failure));
        }
    }
    return failures;
}

std::string format_service_requirement_failures(
    const std::vector<ServiceRequirementFailure> &failures
)
{
    std::string text = "Required services are unavailable:";
    for (const auto &failure : failures) {
        text += "\n- " + failure.requirement.name + ": requires " + failure.requirement.version;
        if (!failure.registered) {
            text += ", not registered";
            continue;
        }
        text += ", local " + failure.local_version + " (" + failure.reason + ")";
    }
    return text;
}

namespace {

std::expected<uint32_t, std::string> parse_version_part(std::string_view part)
{
    if (part.empty()) {
        return std::unexpected("must use numeric MAJOR.MINOR.PATCH format");
    }

    uint32_t value = 0;
    const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
    if ((error != std::errc {}) || (end != (part.data() + part.size()))) {
        return std::unexpected("must use numeric MAJOR.MINOR.PATCH format");
    }
    return value;
}

} // namespace

} // namespace esp_brookesia::system::core::detail
