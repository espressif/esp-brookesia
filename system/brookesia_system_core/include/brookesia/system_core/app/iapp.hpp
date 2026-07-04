/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/system_core/app/context.hpp"

namespace esp_brookesia::system::core {

/**
 * @brief Base interface implemented by native applications hosted by Brookesia System.
 *
 * Applications provide a manifest, optionally expose a GUI descriptor, and receive lifecycle,
 * action, and timer callbacks from the system runtime.
 */
class IApp {
public:
    IApp() = default;
    IApp(const IApp &) = delete;
    IApp &operator=(const IApp &) = delete;
    virtual ~IApp() = default;

    /**
     * @brief Get the static metadata used to identify and present the application.
     *
     * @return Application manifest containing id, name, version, and launch metadata.
     */
    virtual AppManifest get_manifest() const = 0;

    /**
     * @brief Get the optional GUI resource descriptor for applications with a visual surface.
     *
     * @return GUI descriptor; the default descriptor is empty for non-visual apps.
     */
    virtual AppGuiDescriptor get_gui_descriptor() const;

    /**
     * @brief Called when the application package is installed into the system.
     *
     * @param context System context for package storage and service access.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_install(AppContext &context);

    /**
     * @brief Called before the application package is removed from the system.
     *
     * @param context System context for cleanup operations.
     */
    virtual void on_uninstall(AppContext &context);

    /**
     * @brief Called when the application is started.
     *
     * @param context Runtime context for GUI, timers, storage, and services.
     * @return Empty result on success, or an error string that aborts startup.
     */
    virtual std::expected<void, std::string> on_start(AppContext &context);

    /**
     * @brief Called when the application should suspend active work.
     *
     * @param context Runtime context for the running application.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_pause(AppContext &context);

    /**
     * @brief Called when the application should resume after a pause.
     *
     * @param context Runtime context for the running application.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_resume(AppContext &context);

    /**
     * @brief Called when the application is stopped.
     *
     * @param context Runtime context for releasing app resources.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_stop(AppContext &context);

    /**
     * @brief Handle an action emitted by the application GUI or shell.
     *
     * @param context Runtime context for the running application.
     * @param action Action identifier registered by the application GUI.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_action(AppContext &context, std::string_view action);

    /**
     * @brief Handle a timer event previously scheduled through the app context.
     *
     * @param context Runtime context for the running application.
     * @param timer_id Runtime timer identifier.
     * @param name Application-defined timer name.
     * @return Empty result on success, or an error string.
     */
    virtual std::expected<void, std::string> on_timer(
        AppContext &context,
        TimerId timer_id,
        std::string_view name
    );
};

/**
 * @brief Factory interface registered by built-in or dynamically loaded application packages.
 */
class IAppProvider {
public:
    IAppProvider() = default;
    IAppProvider(const IAppProvider &) = delete;
    IAppProvider &operator=(const IAppProvider &) = delete;
    virtual ~IAppProvider() = default;

    /**
     * @brief Get the manifest of the application created by this provider.
     *
     * @return Application manifest used for discovery and installation.
     */
    virtual AppManifest get_manifest() const = 0;

    /**
     * @brief Create an application instance.
     *
     * @return Shared application instance, or nullptr when the provider cannot instantiate it.
     */
    virtual std::shared_ptr<IApp> create_app();
};

/**
 * @brief Registry type used to discover application providers by plugin name.
 */
using AppProviderRegistry = lib_utils::PluginRegistry<IAppProvider>;

} // namespace esp_brookesia::system::core

/**
 * @brief Register an application provider with a stable plugin symbol.
 *
 * @param ProviderType Concrete provider type derived from IAppProvider.
 * @param name Provider name used by the plugin registry.
 * @param symbol_name Exported symbol name used for dynamic provider lookup.
 */
#define BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(ProviderType, name, symbol_name) \
    BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL( \
        ::esp_brookesia::system::core::IAppProvider, ProviderType, name, symbol_name \
    )
