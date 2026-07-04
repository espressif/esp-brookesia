/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest FileManagerAppProvider::get_manifest() const
{
    return FileManagerApp().get_manifest();
}

std::shared_ptr<system::core::IApp> FileManagerAppProvider::create_app()
{
    return std::make_shared<FileManagerApp>();
}

BROOKESIA_SYSTEM_CORE_APP_PROVIDER_REGISTER_WITH_SYMBOL(
    FileManagerAppProvider,
    APP_ID,
    app_files_provider_symbol
);
