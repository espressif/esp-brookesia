/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "boost/thread/recursive_mutex.hpp"
#include "brookesia/app_matter_controller/matter_controller_model.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::app::matter_controller {

class MatterBackend;

struct DeviceInfo {
    std::string path{};
    // Tracks the instantiated card template so an endpoint only needs a
    // replacement view when its UI capability class changes.
    std::string template_id{};
    bool in_use = false;
    uint64_t node_id = 0;
    uint16_t endpoint_id = 0;
};

class MatterControllerApp final: public system::core::IApp {
public:
    ~MatterControllerApp() override;

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_resume(system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context, std::string_view action) override;
    std::expected<void, std::string> on_timer(
        system::core::AppContext &context, system::core::TimerId timer_id,
        std::string_view name) override;

private:
    // Page management
    std::expected<void, std::string> ensure_stage_view_created(
        system::core::AppContext &ctx, std::string_view template_id);
    void destroy_stage_view(system::core::AppContext &ctx, std::string_view template_id);
    void release_page_resources(system::core::AppContext &ctx, std::string_view page);
    std::expected<void, std::string> show_page(system::core::AppContext &ctx, std::string_view page);
    void setup_network_page(system::core::AppContext &ctx);
    void setup_settings_page(system::core::AppContext &ctx);
    void update_nav_highlight(system::core::AppContext &ctx) const;
    void select_device_by_path(std::string_view event_path);

    // Device grid
    void setup_device_grid(system::core::AppContext &ctx);
    void apply_card(system::core::AppContext &ctx, DeviceInfo &card, const ControllerDevice &dev) const;
    void apply_light_controls(
        system::core::AppContext &ctx, std::string_view card_path, const ControllerDevice &dev,
        bool has_color_temp_controls, bool has_hue_controls) const;
    void refresh_device_card(system::core::AppContext &ctx, size_t device_index);
    DeviceInfo *acquire_card();
    void release_card(system::core::AppContext &ctx, DeviceInfo &card);
    void release_all_cards(system::core::AppContext &ctx);

    // Rooms
    void setup_rooms_list(system::core::AppContext &ctx);
    void show_room_modal(system::core::AppContext &ctx);
    void hide_room_modal(system::core::AppContext &ctx);
    void setup_room_modal(system::core::AppContext &ctx);
    void apply_room_device_option(system::core::AppContext &ctx, size_t device_index) const;
    void apply_room_type_options(system::core::AppContext &ctx) const;
    void select_room_device_by_path(std::string_view event_path);

    // Thread
    void setup_thread_map(system::core::AppContext &ctx);
    void clear_thread_nodes(system::core::AppContext &ctx);
    void setup_pairing_page(system::core::AppContext &ctx);

    // Modal
    void show_modal(system::core::AppContext &ctx, std::string_view title_key, std::string_view message_key,
                    std::string_view confirm_key, std::string_view confirm_bg);
    void hide_modal(system::core::AppContext &ctx);

    // Backend
    std::expected<void, std::string> start_backend();
    void stop_backend();
    bool sync_backend_state(system::core::AppContext &ctx, bool force_refresh);
    std::vector<std::pair<uint64_t, uint16_t>> selected_room_device_ids() const;

    // I18N
    std::expected<bool, std::string> refresh_locale_if_needed(system::core::AppContext &ctx, bool force);
    std::expected<void, std::string> apply_pending_locale_updates(
        system::core::AppContext &ctx, std::string_view path_prefix);
    void refresh_visible_content(system::core::AppContext &ctx);
    void refresh_active_modal(system::core::AppContext &ctx);
    std::string tr(std::string_view key) const;
    std::string format_tr(std::string_view key, std::string_view placeholder, std::string_view value) const;
    std::string localized_room_name(std::string_view room_key) const;
    std::string localized_device_status(const ControllerDevice &device) const;
    std::string localized_device_name(const ControllerDevice &device) const;

    // Binding system
    void set_binding(system::core::AppContext &, std::string_view path, std::string_view key,
                     std::string value) const;
    void force_binding(system::core::AppContext &, std::string_view path, std::string_view key,
                       std::string value) const;
    void queue_binding(std::string_view path, std::string_view key, std::string value,
                       bool force) const;
    void invalidate_binding(std::string_view path, std::string_view key) const;
    void forget_binding_prefix(std::string_view path_prefix) const;
    std::expected<void, std::string> flush_bindings(system::core::AppContext &context) const;

    mutable boost::recursive_mutex mutex_;
    mutable std::unordered_map<std::string, std::string> binding_cache_;
    mutable std::unordered_map<std::string, size_t> pending_binding_indexes_;
    mutable std::vector<gui::BindingValueUpdate> pending_binding_updates_;

    system::core::TimerId clock_timer_id_ = system::core::INVALID_TIMER_ID;
    std::vector<gui::ScopedConnection> event_connections_;
    std::unordered_map<std::string, size_t> path_to_device_map_;
    std::unordered_set<std::string> created_stage_views_;

    std::string active_page_{};
    std::string active_stage_view_{};
    std::string modal_action_{};
    std::optional<std::pair<uint64_t, uint16_t>> pending_remove_device_id_;
    std::string active_modal_title_key_{};
    std::string active_modal_message_key_{};
    std::string active_modal_confirm_key_{};
    std::string active_modal_confirm_bg_{};
    std::string current_locale_{};
    std::unordered_map<std::string, std::string> localized_strings_;
    std::vector<gui::BindingValueUpdate> pending_locale_updates_;
    // Device cards are referenced by pointer from active_cards_. A deque keeps
    // those pointers stable when a newly commissioned device grows the pool.
    std::deque<DeviceInfo> card_pool_;
    std::vector<DeviceInfo *> active_cards_;
    std::unique_ptr<MatterBackend> backend_;
    std::vector<ControllerDevice> devices_;
    std::vector<ControllerRoom> rooms_;
    std::vector<std::string> room_row_paths_;
    std::vector<std::string> thread_node_paths_;
    std::vector<std::string> room_option_paths_;
    std::unordered_map<std::string, size_t> room_option_path_to_device_map_;
    std::vector<bool> room_draft_device_selected_;
    std::string room_draft_name_;
    std::string room_draft_error_key_;
    bool room_modal_visible_ = false;
    bool pairing_qr_image_registered_ = false;
    std::optional<int> pending_brightness_value_;
    std::optional<int> pending_cct_value_;
    std::optional<int> pending_hue_value_;
    std::optional<int> pending_saturation_value_;
    size_t selected_device_index_ = 0;
    int ui_brightness_ = 80;
    size_t selected_room_option_index_ = 0;
    size_t selected_room_type_index_ = 0;
};

class MatterControllerAppProvider final: public system::core::IAppProvider {
public:
    system::core::AppManifest get_manifest() const override;
    std::shared_ptr<system::core::IApp> create_app() override;
};

} // namespace esp_brookesia::app::matter_controller
