/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_audio/service_audio.hpp"

namespace esp_brookesia::service {

// Settings needs playback only; encoder and decoder services are not registered.
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, AudioPlayback, AudioPlayback::get_instance().get_attributes().name, AudioPlayback::get_instance(),
    settings_audio_playback_symbol
);

} // namespace esp_brookesia::service
