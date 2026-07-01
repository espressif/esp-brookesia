/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest AppStoreAppProvider::get_manifest() const
{
    return AppStoreApp().get_manifest();
}

std::shared_ptr<system::core::IApp> AppStoreAppProvider::create_app()
{
    return std::make_shared<AppStoreApp>();
}

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    AppStoreAppProvider,
    APP_ID,
    app_store_provider_symbol
);
