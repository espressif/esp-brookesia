/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {
;

Runtime::Runtime(std::unique_ptr<IBackend> backend)
    : impl_(std::make_unique<Impl>(std::move(backend)))
{}

Runtime::Runtime(std::unique_ptr<IBackend> backend, RuntimeTaskConfig task_config)
    : impl_(std::make_unique<Impl>(std::move(backend), std::move(task_config)))
{}

Runtime::~Runtime() = default;

std::expected<DocumentId, std::string> Runtime::load(
    std::string_view root_path, const Document &document, const Environment &environment)
{
    return load(root_path, document, environment, DocumentLoadOptions{});
}

std::expected<DocumentId, std::string> Runtime::load(
    std::string_view root_path,
    const Document &document,
    const Environment &environment,
    const DocumentLoadOptions &options)
{
    return impl_->load(root_path, document, environment, false, {}, options);
}

std::expected<DocumentId, std::string> Runtime::load_json(
    std::string_view root_path, std::string_view json, std::string_view base_dir, const Environment &environment)
{
    return load_json(root_path, json, base_dir, environment, DocumentLoadOptions{});
}

std::expected<DocumentId, std::string> Runtime::load_json(
    std::string_view root_path,
    std::string_view json,
    std::string_view base_dir,
    const Environment &environment,
    const DocumentLoadOptions &options)
{
    const auto parse_environment = impl_->make_parse_environment(environment);
    auto document = parse_document(json, base_dir, parse_environment);
    if (!document) {
        return std::unexpected(document.error());
    }
    return impl_->load(root_path, std::move(*document), environment, false, {}, options);
}

std::expected<DocumentId, std::string> Runtime::load_file(std::string_view path, const Environment &environment)
{
    return load_file(path, environment, DocumentLoadOptions{});
}

std::expected<DocumentId, std::string> Runtime::load_file(
    std::string_view path, const Environment &environment, const DocumentLoadOptions &options)
{
    const std::string path_string(path);
    const auto total_start = RuntimeProfileClock::now();
    auto stage_start = total_start;
    const auto parse_environment = impl_->make_parse_environment(environment);
    auto parsed_document = parse_document_file_with_metadata(path, parse_environment);
    auto stage_end = RuntimeProfileClock::now();
    if (!parsed_document) {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load file profile: file(%1%), stage(parse_document_failed), elapsed_ms(%2%), "
            "total_ms(%3%)",
            path_string,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        return std::unexpected(parsed_document.error());
    }

    const auto dependency_count = parsed_document->dependency_files.size();
    const auto image_count = parsed_document->document.images.size();
    const auto screen_count = parsed_document->document.screens.size();
    const auto template_count = parsed_document->document.templates.size();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime load file profile: file(%1%), stage(parse_document), dependencies(%2%), images(%3%), "
        "screens(%4%), templates(%5%), elapsed_ms(%6%), total_ms(%7%)",
        path_string,
        dependency_count,
        image_count,
        screen_count,
        template_count,
        runtime_profile_elapsed_ms(stage_start, stage_end),
        runtime_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = RuntimeProfileClock::now();
    auto result = impl_->load(
                      path,
                      std::move(parsed_document->document),
                      environment,
                      true,
                      std::move(parsed_document->dependency_files),
                      options
                  );
    stage_end = RuntimeProfileClock::now();
    if (!result) {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load file profile: file(%1%), stage(load_document_failed), elapsed_ms(%2%), "
            "total_ms(%3%)",
            path_string,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        return result;
    }

    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime load file profile: file(%1%), doc(%2%), stage(load_document), elapsed_ms(%3%), total_ms(%4%)",
        path_string,
        result->value(),
        runtime_profile_elapsed_ms(stage_start, stage_end),
        runtime_profile_elapsed_ms(total_start, stage_end)
    );
    return result;
}

void Runtime::request_idle_screen_warm(DocumentId id)
{
    impl_->request_idle_screen_warm(id);
}

std::expected<void, std::string> Runtime::load_theme(const ThemeAsset &theme)
{
    return impl_->load_theme(theme);
}

std::expected<void, std::string> Runtime::load_theme_json(std::string_view json, std::string_view base_dir)
{
    return impl_->load_theme_json(json, base_dir);
}

std::expected<void, std::string> Runtime::load_theme_file(std::string_view path)
{
    return impl_->load_theme_file(path);
}

std::vector<std::string> Runtime::list_supported_themes() const
{
    return impl_->list_supported_themes();
}

std::expected<void, std::string> Runtime::set_theme(std::string_view theme_id)
{
    return impl_->set_theme(this, theme_id);
}

std::expected<void, std::string> Runtime::set_theme(std::string_view theme_id, bool reapply_loaded_documents)
{
    return impl_->set_theme(this, theme_id, reapply_loaded_documents);
}

std::string Runtime::get_theme() const
{
    return impl_->get_theme();
}

std::expected<void, std::string> Runtime::register_font(const RuntimeFontResource &resource)
{
    return impl_->register_font(resource);
}

bool Runtime::unregister_font(std::string_view id)
{
    return impl_->unregister_font(id);
}

std::expected<void, std::string> Runtime::register_font_json(std::string_view json, std::string_view base_dir)
{
    return impl_->register_font_json(json, base_dir);
}

std::expected<void, std::string> Runtime::register_font_file(std::string_view path)
{
    return impl_->register_font_file(path);
}

std::vector<std::string> Runtime::list_supported_fonts(std::string_view language) const
{
    return impl_->list_supported_fonts(language);
}

std::vector<std::string> Runtime::list_supported_languages() const
{
    return impl_->list_supported_languages();
}

std::vector<std::string> Runtime::list_supported_languages(std::string_view font_id) const
{
    return impl_->list_supported_languages(font_id);
}

std::expected<void, std::string> Runtime::set_language(std::string_view language)
{
    return impl_->set_language(this, language);
}

std::expected<void, std::string> Runtime::set_language(std::string_view language, bool reapply_loaded_documents)
{
    return impl_->set_language(this, language, reapply_loaded_documents);
}

std::string Runtime::get_language() const
{
    return impl_->get_language();
}

std::expected<void, std::string> Runtime::set_default_font_for_language(
    std::string_view language,
    std::string_view font_id)
{
    return impl_->set_default_font_for_language(language, font_id);
}

std::optional<std::string> Runtime::get_default_font_for_language(std::string_view language) const
{
    return impl_->get_default_font_for_language(language);
}

std::expected<void, std::string> Runtime::register_image(const RuntimeImageResource &resource)
{
    return impl_->register_image(resource);
}

bool Runtime::unregister_image(std::string_view id)
{
    return impl_->unregister_image(id);
}

std::expected<void, std::string> Runtime::register_image_json(std::string_view json, std::string_view base_dir)
{
    return impl_->register_image_json(json, base_dir);
}

std::expected<void, std::string> Runtime::register_image_file(std::string_view path)
{
    return impl_->register_image_file(path);
}

std::expected<void, std::string> Runtime::preload_image(DocumentId id, std::string_view image_id)
{
    return preload_images(id, {std::string(image_id)});
}

std::expected<void, std::string> Runtime::preload_images(DocumentId id, const std::vector<std::string> &image_ids)
{
    return impl_->preload_images(id, image_ids);
}

std::expected<void, std::string> Runtime::release_preloaded_image(DocumentId id, std::string_view image_id)
{
    return release_preloaded_images(id, {std::string(image_id)});
}

std::expected<void, std::string> Runtime::release_preloaded_images(
    DocumentId id,
    const std::vector<std::string> &image_ids)
{
    return impl_->release_preloaded_images(id, image_ids);
}

void Runtime::process_backend()
{
    if (impl_->backend != nullptr) {
        impl_->backend->process_timers();
    }
}

void Runtime::set_view_debug_enabled(bool enabled)
{
    impl_->set_view_debug_enabled(enabled);
}

bool Runtime::is_view_debug_enabled() const
{
    return impl_->is_view_debug_enabled();
}

std::expected<void, std::string> Runtime::enable_live_preview(DocumentId id, const LivePreviewOptions &options)
{
    return impl_->enable_live_preview(id, options);
}

bool Runtime::disable_live_preview(DocumentId id)
{
    return impl_->disable_live_preview(id);
}

void Runtime::poll_live_preview()
{
    impl_->poll_live_preview(this);
}

bool Runtime::unload(DocumentId id)
{
    return impl_->unload(id);
}

SubscriptionId Runtime::subscribe_event_action_with_id(
    DocumentId id,
    std::string_view action,
    ActionHandler handler)
{
    return impl_->subscribe_event_action_with_id(this, id, action, std::move(handler));
}

bool Runtime::unsubscribe_subscription(SubscriptionId subscription_id)
{
    return impl_->unsubscribe_subscription(subscription_id);
}

ScopedConnection Runtime::subscribe_event_action(
    DocumentId id,
    std::string_view action,
    ActionHandler handler)
{
    auto connection = impl_->connect_event_action_signal(this, id, action, std::move(handler));
    if (!connection) {
        return {};
    }
    return ScopedConnection(*connection);
}

std::vector<GuiLayer> Runtime::list_layers() const
{
    return impl_->list_layers();
}

std::vector<GuiDisplayInfo> Runtime::list_displays() const
{
    return impl_->list_displays();
}

std::expected<View, std::string> Runtime::mount_screen(
    DocumentId id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    return impl_->mount_screen(this, id, absolute_path, target);
}

bool Runtime::unmount_screen(DocumentId id, std::string_view absolute_path)
{
    return impl_->unmount_screen(id, absolute_path);
}

std::expected<TransientMountId, std::string> Runtime::push_transient_screen(
    DocumentId id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    return impl_->push_transient_screen(this, id, absolute_path, target);
}

bool Runtime::pop_transient_screen(TransientMountId id)
{
    return impl_->pop_transient_screen(id);
}

std::expected<void, std::string> Runtime::start_screen_flow(
    DocumentId id,
    std::string_view flow_id,
    const MountTarget &target)
{
    return impl_->start_screen_flow(this, id, flow_id, target);
}

std::expected<void, std::string> Runtime::trigger_screen_flow(
    DocumentId id,
    std::string_view flow_id,
    std::string_view action)
{
    return impl_->trigger_screen_flow(this, id, flow_id, action);
}

bool Runtime::stop_screen_flow(DocumentId id, std::string_view flow_id)
{
    return impl_->stop_screen_flow(id, flow_id);
}

bool Runtime::has_screen_flow(DocumentId id, std::string_view flow_id) const
{
    return impl_->has_screen_flow(id, flow_id);
}

std::optional<std::string> Runtime::get_screen_flow_state(DocumentId id, std::string_view flow_id) const
{
    return impl_->get_screen_flow_state(id, flow_id);
}

View Runtime::find_view(DocumentId id, std::string_view absolute_path) const
{
    return impl_->find_view(const_cast<Runtime *>(this), id, absolute_path);
}

std::expected<View, std::string> Runtime::create_view(
    DocumentId id, std::string_view template_id, std::string_view parent_absolute_path, std::string_view instance_id)
{
    return impl_->create_view(this, id, template_id, parent_absolute_path, instance_id);
}

bool Runtime::destroy_view(DocumentId id, std::string_view absolute_path)
{
    return impl_->destroy_view(id, absolute_path);
}

std::expected<void, std::string> Runtime::update(DocumentId id, std::string_view file_path, const Environment &environment)
{
    return impl_->update(this, id, file_path, environment);
}

std::expected<void, std::string> Runtime::reapply_styles(DocumentId id)
{
    return impl_->reapply_styles(id);
}

std::expected<boost::json::value, std::string> Runtime::get_constant_value(
    DocumentId id,
    std::string_view path
) const
{
    return impl_->get_constant_value(id, path);
}

std::vector<RuntimeFontResource> Runtime::list_font_resources(DocumentId id) const
{
    return impl_->list_font_resources(id);
}

std::vector<RuntimeImageResource> Runtime::list_image_resources(DocumentId id) const
{
    return impl_->list_image_resources(id);
}

std::optional<ViewStateValue> Runtime::get_view_state(const View &view, ViewStateKind kind) const
{
    return impl_->get_view_state_internal(view, kind);
}

void Runtime::set_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value) const
{
    std::array<BindingValueUpdate, 1> updates {{
            BindingValueUpdate{
                .absolute_path = std::string(absolute_path),
                .key = std::string(key),
                .value = std::move(value),
            }
        }};
    impl_->set_binding_values(id, updates);
}

void Runtime::set_binding_values(DocumentId id, const std::vector<BindingValueUpdate> &updates) const
{
    impl_->set_binding_values(id, std::span<const BindingValueUpdate>(updates));
}

std::optional<std::string> Runtime::get_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key) const
{
    return impl_->store == nullptr ? std::nullopt : impl_->store->get_string(id, absolute_path, key);
}

SubscriptionId Runtime::subscribe_binding_value_with_id(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    BindingValueHandler handler) const
{
    if (impl_->store == nullptr || !id.is_valid() || absolute_path.empty() || key.empty() || !handler) {
        return 0;
    }

    const auto public_subscription_id = impl_->next_subscription_id_++;
    auto store_subscription_id = impl_->store->subscribe(
                                     id,
                                     absolute_path,
                                     key,
                                     [absolute_path = std::string(absolute_path), key = std::string(key), handler = std::move(handler)]
    (std::string_view unused_scoped_key, std::string_view value) {
        (void)unused_scoped_key;
        handler(absolute_path, key, value);
    }
                                 );
    if (store_subscription_id == 0) {
        return 0;
    }

    auto store = impl_->store;
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [registry = std::weak_ptr<Runtime::Impl::SubscriptionRegistry>(impl_->subscription_registry_),
                                    public_subscription_id,
                                    store = std::move(store),
             store_subscription_id]() mutable {
        if (store != nullptr)
        {
            store->unsubscribe(store_subscription_id);
        }
        if (auto locked_registry = registry.lock(); locked_registry != nullptr)
        {
            locked_registry->disconnect_handlers.erase(public_subscription_id);
        }
    };
    impl_->subscription_registry_->disconnect_handlers[public_subscription_id] = disconnect_handler;
    impl_->register_document_subscription(public_subscription_id, id);
    return public_subscription_id;
}

ScopedConnection Runtime::subscribe_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    BindingValueHandler handler) const
{
    if (impl_->store == nullptr || !id.is_valid() || absolute_path.empty() || key.empty() || !handler) {
        return {};
    }

    auto store_subscription_id = impl_->store->subscribe(
                                     id,
                                     absolute_path,
                                     key,
                                     [absolute_path = std::string(absolute_path), key = std::string(key), handler = std::move(handler)]
    (std::string_view unused_scoped_key, std::string_view value) {
        (void)unused_scoped_key;
        handler(absolute_path, key, value);
    }
                                 );
    if (store_subscription_id == 0) {
        return {};
    }

    auto store = impl_->store;
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [store = std::move(store), store_subscription_id]() mutable {
        if (store != nullptr)
        {
            store->unsubscribe(store_subscription_id);
        }
    };
    return ScopedConnection(disconnect_handler);
}

SubscriptionId Runtime::start_view_animation_with_id(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation_with_id(find_view(id, absolute_path), animation, std::move(completed_handler));
}

RuntimeAnimationStartResult Runtime::start_view_animation_with_result(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation_with_result(find_view(id, absolute_path), animation, std::move(completed_handler));
}

ScopedConnection Runtime::start_view_animation(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation(find_view(id, absolute_path), animation, std::move(completed_handler));
}

SubscriptionId Runtime::start_view_animation_with_id(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation_with_id(view, animation, std::move(completed_handler));
}

RuntimeAnimationStartResult Runtime::start_view_animation_with_result(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation_with_result(view, animation, std::move(completed_handler));
}

ScopedConnection Runtime::start_view_animation(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation(view, animation, std::move(completed_handler));
}

bool Runtime::is_view_valid(const View &view) const
{
    return impl_->is_view_valid(view);
}

std::string Runtime::get_view_id(const View &view) const
{
    return impl_->get_view_id(view);
}

std::string Runtime::get_view_absolute_path(const View &view) const
{
    return view.absolute_path_;
}

std::optional<ViewStateValue> Runtime::get_view_state_internal(const View &view, ViewStateKind kind) const
{
    return impl_->get_view_state_internal(view, kind);
}

std::optional<ViewStateValue> Runtime::get_view_state(DocumentId id, std::string_view absolute_path, ViewStateKind kind) const
{
    return get_view_state(find_view(id, absolute_path), kind);
}

bool Runtime::scroll_view_to_visible(DocumentId id, std::string_view absolute_path, bool animated) const
{
    return scroll_view_to_visible(find_view(id, absolute_path), animated);
}

bool Runtime::scroll_view_to(DocumentId id, std::string_view absolute_path, int32_t x, int32_t y, bool animated) const
{
    return scroll_view_to(find_view(id, absolute_path), x, y, animated);
}

bool Runtime::scroll_view_to(const View &view, int32_t x, int32_t y, bool animated) const
{
    return impl_->scroll_view_to(view, x, y, animated);
}

bool Runtime::scroll_view_to_visible(const View &view, bool animated) const
{
    return impl_->scroll_view_to_visible(view, animated);
}

bool Runtime::set_view_hidden(const View &view, bool hidden) const
{
    return impl_->set_view_hidden(view, hidden);
}

bool Runtime::set_view_text(const View &view, std::string_view text) const
{
    return impl_->set_view_text(view, text);
}

std::string Runtime::get_view_text(const View &view) const
{
    return impl_->get_view_text(view);
}

bool Runtime::set_view_src(const View &view, std::string_view src) const
{
    return impl_->set_view_src(view, src);
}

bool Runtime::set_view_src(DocumentId id, std::string_view absolute_path, std::string_view src) const
{
    return set_view_src(find_view(id, absolute_path), src);
}

std::string Runtime::get_view_src(const View &view) const
{
    return impl_->get_view_src(view);
}

bool Runtime::set_view_value(const View &view, int32_t value) const
{
    return impl_->set_view_value(view, value);
}

int32_t Runtime::get_view_value(const View &view) const
{
    return impl_->get_view_value(view);
}

bool Runtime::set_view_checked(const View &view, bool checked) const
{
    return impl_->set_view_checked(view, checked);
}

bool Runtime::get_view_checked(const View &view) const
{
    return impl_->get_view_checked(view);
}

bool Runtime::set_view_selected_index(const View &view, int32_t index) const
{
    return impl_->set_view_selected_index(view, index);
}

int32_t Runtime::get_view_selected_index(const View &view) const
{
    return impl_->get_view_selected_index(view);
}

bool Runtime::set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text) const
{
    return impl_->set_table_cell_text(view, row, column, text);
}

esp_brookesia::lib_utils::connection Runtime::connect_view_event(
    const View &view, EventType type, View::EventHandler handler) const
{
    return impl_->connect_view_event(view, type, std::move(handler));
}

esp_brookesia::lib_utils::connection Runtime::connect_button_click(const Button &button, Button::ClickHandler handler) const
{
    return impl_->connect_button_click(button, std::move(handler));
}

}
