/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/gui_interface.hpp"
#include "brookesia/gui_lvgl/macro_configs.h"

typedef struct _lv_display_t lv_display_t;

namespace esp_brookesia::gui::lvgl {

void lock_thread();
void unlock_thread();

class BackendImpl;

struct EspFontMountConfig {
    std::string partition_label;
    char fs_letter = 'F';
    int max_files = 0;
    uint32_t checksum = 0;
};

struct EspFontRegistrationConfig {
    std::string font_id;
    char fs_letter = 'F';
    std::string file_name;
    std::vector<std::string> languages;
    std::vector<std::string> fallback_ids;
};

struct FontRegistrationConfig {
    std::string font_id;
    std::string primary_src;
    uintptr_t native_src = 0;
    int32_t native_size = 0;
    std::vector<std::string> languages;
    std::vector<std::string> fallback_ids;
};

struct EspImageMountConfig {
    std::string partition_label;
    char fs_letter = 'I';
    int max_files = 0;
    uint32_t checksum = 0;
};

BROOKESIA_DESCRIBE_STRUCT(EspFontMountConfig, (), (partition_label, fs_letter, max_files, checksum))
BROOKESIA_DESCRIBE_STRUCT(EspFontRegistrationConfig, (), (font_id, fs_letter, file_name, languages, fallback_ids))
BROOKESIA_DESCRIBE_STRUCT(FontRegistrationConfig, (), (font_id, primary_src, native_src, native_size, languages, fallback_ids))
BROOKESIA_DESCRIBE_STRUCT(EspImageMountConfig, (), (partition_label, fs_letter, max_files, checksum))

class Backend final: public IBackend {
public:
    Backend();
    Backend(const Backend &) = delete;
    Backend &operator=(const Backend &) = delete;
    ~Backend() override;

    void set_event_sink(EventSink sink) override;
    std::optional<ThreadGuard> get_thread_guard() const override;
    BackendHandle create_node(
        const Node &node,
        BackendHandle parent,
        std::string_view parent_path,
        std::string_view scope_root_absolute_path
    ) override;
    void destroy_node(BackendHandle handle) override;
    void apply_props(
        BackendHandle handle, const Node &node, PropsApplyMask mask = PropsApplyMask::All
    ) override;
    void apply_layout(
        BackendHandle handle, const Layout &layout, LayoutApplyMask mask = LayoutApplyMask::All
    ) override;
    void apply_placement(
        BackendHandle handle, const Placement &placement, PlacementApplyMask mask = PlacementApplyMask::All
    ) override;
    void apply_style(
        BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask = StyleApplyMask::All
    ) override;
    void apply_debug_visual(BackendHandle handle, bool enabled) override;
    void apply_animations(BackendHandle handle, const std::vector<Animation> &animations) override;
    std::optional<BackendAnimationStartResult> start_animation(
        BackendHandle handle,
        const Animation &animation,
        std::function<void()> completed_handler = {}) override;
    void bind_events(BackendHandle handle, const std::vector<EventBinding> &events) override;
    std::vector<GuiDisplayInfo> list_displays() const override;
    std::vector<GuiLayer> list_layers() const override;
    bool mount_screen(BackendHandle handle, const MountTarget &target) override;
    bool unmount_screen(BackendHandle handle) override;
    bool register_font_resource(const RuntimeFontResource &resource) override;
    std::vector<RuntimeFontResource> list_font_resources() const override;
    std::expected<RuntimeImageResource, std::string> resolve_image_resource(
        RuntimeImageResource resource
    ) const override;
    bool requires_preloaded_image_resource(const RuntimeImageResource &resource) const override;
    std::expected<void, std::string> preload_image_resource(const RuntimeImageResource &resource) override;
    void release_image_resource(const RuntimeImageResource &resource) override;
    void process_timers() override;
    std::optional<ViewFrame> get_node_frame(BackendHandle handle) const override;
    bool scroll_node_to_visible(BackendHandle handle, bool animated) override;

    bool mount_font_assets(const EspFontMountConfig &config);
    bool unmount_font_assets(char fs_letter);
    bool register_font_resource_from_file(const EspFontRegistrationConfig &config);
    bool register_font_resource(const FontRegistrationConfig &config);
    bool mount_image_assets(const EspImageMountConfig &config);
    bool unmount_image_assets(char fs_letter);
    bool register_display(std::string id, lv_display_t *display, bool set_default = false);

private:
    std::unique_ptr<BackendImpl> impl_;
};

} // namespace esp_brookesia::gui::lvgl
