/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/desktop.hpp"

using esp_brookesia::gui::gsp::desktop_invoke;

// These wrappers are linked only into Linux consumers of brookesia::gui_gsp.
// The official bridge owns the handle; every call is serialized onto its polling thread.
#define DESKTOP_WRAP(name, parameters, arguments) \
    extern "C" esp_gsp_err_t __real_##name parameters; \
    extern "C" esp_gsp_err_t __wrap_##name parameters \
    { \
        return desktop_invoke(gsp, [=] { return __real_##name arguments; }); \
    }

DESKTOP_WRAP(esp_gsp_component_set_text,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, const char *text), (gsp, key, text))
DESKTOP_WRAP(esp_gsp_component_set_value,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, int32_t value), (gsp, key, value))
DESKTOP_WRAP(esp_gsp_component_set_visible,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, bool value), (gsp, key, value))
DESKTOP_WRAP(esp_gsp_component_set_enabled,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, bool value), (gsp, key, value))
DESKTOP_WRAP(esp_gsp_component_set_property,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, gsp_property_key_t property, const gsp_value_t *value),
             (gsp, key, property, value))
DESKTOP_WRAP(esp_gsp_component_set_property_color_rgb888,
             (esp_gsp_handle_t gsp, gsp_component_key_t key, gsp_property_key_t property, uint32_t value),
             (gsp, key, property, value))
DESKTOP_WRAP(esp_gsp_goto_scene,
             (esp_gsp_handle_t gsp, uint16_t scene, esp_gsp_transition_t transition), (gsp, scene, transition))
DESKTOP_WRAP(esp_gsp_set_swipe_enabled, (esp_gsp_handle_t gsp, bool value), (gsp, value))
DESKTOP_WRAP(esp_gsp_flush, (esp_gsp_handle_t gsp, uint32_t timeout), (gsp, timeout))
DESKTOP_WRAP(esp_gsp_on_event,
             (esp_gsp_handle_t gsp, esp_gsp_event_cb_t callback, void *context), (gsp, callback, context))

extern "C" esp_gsp_err_t esp_gsp_component_get_property_info(
    esp_gsp_handle_t gsp, gsp_component_key_t component, gsp_property_key_t property, gsp_property_info_t *info)
{
    return desktop_invoke(gsp, [ = ] {
        return esp_brookesia::gui::gsp::desktop_property_info(gsp, component, property, info);
    });
}
