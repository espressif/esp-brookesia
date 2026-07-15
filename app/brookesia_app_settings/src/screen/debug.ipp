/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

namespace {

bool is_platform_thread_debug_supported()
{
#if defined(ESP_PLATFORM)
    return true;
#else
    return false;
#endif
}

enum class DebugSliderId {
    MemorySampleInterval,
    MemoryInternalFreePercent,
    MemoryInternalLargestFreeBlock,
    MemoryExternalFreePercent,
    MemoryExternalLargestFreeBlock,
    ThreadProfilingInterval,
    ThreadSamplingDuration,
    ThreadIdleCpuPercent,
    ThreadStackHighWaterMark,
};

enum class DebugSliderTickUnit {
    Milliseconds,
    Percent,
    Kilobytes,
    Bytes,
};

struct DebugSliderSpec {
    DebugSliderId id;
    std::string_view action;
    std::string_view storage_key;
    std::string_view row_path;
    std::string_view slider_path;
    int min_value;
    int default_value;
    int max_value;
    int step;
    DebugSliderTickUnit tick_unit;
    bool thread_only;
};

struct SettingsDebugStorageEntries {
    std::string nspace;
    std::unordered_set<std::string> keys;
};

static constexpr int DEBUG_THREAD_SAMPLING_DURATION_INTERVAL_GAP_MS = 100;

static constexpr std::array<DebugSliderSpec, 9> DEBUG_SLIDER_SPECS = {{
    {
        DebugSliderId::MemorySampleInterval,
        ACTION_DEBUG_MEMORY_SAMPLE_INTERVAL,
        DEBUG_KEY_MEMORY_SAMPLE_INTERVAL_MS,
        DEBUG_MEMORY_SAMPLE_ROW_PATH,
        DEBUG_MEMORY_SAMPLE_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_MIN_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_DEFAULT_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_SAMPLE_INTERVAL_MAX_MS,
        1,
        DebugSliderTickUnit::Milliseconds,
        false,
    },
    {
        DebugSliderId::MemoryInternalFreePercent,
        ACTION_DEBUG_MEMORY_INTERNAL_FREE_PERCENT,
        DEBUG_KEY_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD,
        DEBUG_MEMORY_INTERNAL_FREE_ROW_PATH,
        DEBUG_MEMORY_INTERNAL_FREE_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MIN,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_FREE_PERCENT_THRESHOLD_MAX,
        1,
        DebugSliderTickUnit::Percent,
        false,
    },
    {
        DebugSliderId::MemoryInternalLargestFreeBlock,
        ACTION_DEBUG_MEMORY_INTERNAL_LARGEST,
        DEBUG_KEY_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_KB,
        DEBUG_MEMORY_INTERNAL_LARGEST_ROW_PATH,
        DEBUG_MEMORY_INTERNAL_LARGEST_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_INTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB,
        1,
        DebugSliderTickUnit::Kilobytes,
        false,
    },
    {
        DebugSliderId::MemoryExternalFreePercent,
        ACTION_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT,
        DEBUG_KEY_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD,
        DEBUG_MEMORY_EXTERNAL_FREE_ROW_PATH,
        DEBUG_MEMORY_EXTERNAL_FREE_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MIN,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_DEFAULT,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_FREE_PERCENT_THRESHOLD_MAX,
        1,
        DebugSliderTickUnit::Percent,
        false,
    },
    {
        DebugSliderId::MemoryExternalLargestFreeBlock,
        ACTION_DEBUG_MEMORY_EXTERNAL_LARGEST,
        DEBUG_KEY_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_KB,
        DEBUG_MEMORY_EXTERNAL_LARGEST_ROW_PATH,
        DEBUG_MEMORY_EXTERNAL_LARGEST_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MIN_KB,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_DEFAULT_KB,
        BROOKESIA_APP_SETTINGS_DEBUG_MEMORY_EXTERNAL_LARGEST_FREE_BLOCK_THRESHOLD_MAX_KB,
        1,
        DebugSliderTickUnit::Kilobytes,
        false,
    },
    {
        DebugSliderId::ThreadProfilingInterval,
        ACTION_DEBUG_THREAD_PROFILING_INTERVAL,
        DEBUG_KEY_THREAD_PROFILING_INTERVAL_MS,
        DEBUG_THREAD_INTERVAL_ROW_PATH,
        DEBUG_THREAD_INTERVAL_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_MIN_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_DEFAULT_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_PROFILING_INTERVAL_MAX_MS,
        1,
        DebugSliderTickUnit::Milliseconds,
        true,
    },
    {
        DebugSliderId::ThreadSamplingDuration,
        ACTION_DEBUG_THREAD_SAMPLING_DURATION,
        DEBUG_KEY_THREAD_SAMPLING_DURATION_MS,
        DEBUG_THREAD_DURATION_ROW_PATH,
        DEBUG_THREAD_DURATION_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MIN_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_DEFAULT_MS,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MAX_MS,
        1,
        DebugSliderTickUnit::Milliseconds,
        true,
    },
    {
        DebugSliderId::ThreadIdleCpuPercent,
        ACTION_DEBUG_THREAD_IDLE_CPU,
        DEBUG_KEY_THREAD_IDLE_CPU_PERCENT_THRESHOLD,
        DEBUG_THREAD_IDLE_ROW_PATH,
        DEBUG_THREAD_IDLE_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MIN,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_DEFAULT,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_IDLE_CPU_PERCENT_THRESHOLD_MAX,
        1,
        DebugSliderTickUnit::Percent,
        true,
    },
    {
        DebugSliderId::ThreadStackHighWaterMark,
        ACTION_DEBUG_THREAD_STACK_HWM,
        DEBUG_KEY_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_BYTES,
        DEBUG_THREAD_STACK_ROW_PATH,
        DEBUG_THREAD_STACK_SLIDER_PATH,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MIN_BYTES,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_DEFAULT_BYTES,
        BROOKESIA_APP_SETTINGS_DEBUG_THREAD_STACK_HIGH_WATER_MARK_THRESHOLD_MAX_BYTES,
        1,
        DebugSliderTickUnit::Bytes,
        true,
    },
}};

int clamp_debug_int(int value, int min_value, int max_value)
{
    return std::clamp(value, min_value, max_value);
}

const DebugSliderSpec *find_debug_slider_spec_by_id(DebugSliderId id)
{
    for (const auto &spec : DEBUG_SLIDER_SPECS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}

const DebugSliderSpec *find_debug_slider_spec_by_action(std::string_view action)
{
    for (const auto &spec : DEBUG_SLIDER_SPECS) {
        if (spec.action == action) {
            return &spec;
        }
    }
    return nullptr;
}

int get_thread_sampling_duration_effective_max(int profiling_interval_ms)
{
    const int max_by_interval = profiling_interval_ms - DEBUG_THREAD_SAMPLING_DURATION_INTERVAL_GAP_MS;
    return clamp_debug_int(
               max_by_interval,
               BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MIN_MS,
               BROOKESIA_APP_SETTINGS_DEBUG_THREAD_SAMPLING_DURATION_MAX_MS
           );
}

int get_debug_slider_effective_max(const DebugSliderSpec &spec, int profiling_interval_ms)
{
    if (spec.id == DebugSliderId::ThreadSamplingDuration) {
        return get_thread_sampling_duration_effective_max(profiling_interval_ms);
    }
    return spec.max_value;
}

int clamp_debug_slider_value(const DebugSliderSpec &spec, int value, int profiling_interval_ms)
{
    return clamp_debug_int(value, spec.min_value, get_debug_slider_effective_max(spec, profiling_interval_ms));
}

std::string format_debug_slider_tick(int value, DebugSliderTickUnit unit)
{
    switch (unit) {
    case DebugSliderTickUnit::Milliseconds:
        return std::to_string(value) + "ms";
    case DebugSliderTickUnit::Percent:
        return std::to_string(value) + "%";
    case DebugSliderTickUnit::Kilobytes:
        return std::to_string(value) + "K";
    case DebugSliderTickUnit::Bytes:
        if (value >= 1024 && value % 1024 == 0) {
            return std::to_string(value / 1024) + "K";
        }
        return std::to_string(value) + "B";
    }
    return std::to_string(value);
}

int make_debug_slider_tick_value(int min_value, int max_value, size_t tick_index)
{
    if (tick_index >= 4) {
        return max_value;
    }
    const int span = max_value - min_value;
    return min_value + static_cast<int>((span * static_cast<int>(tick_index) + 2) / 4);
}

void add_debug_slider_binding_updates(
    std::vector<gui::BindingValueUpdate> &updates,
    const DebugSliderSpec &spec,
    int value,
    int profiling_interval_ms
)
{
    const int effective_max = get_debug_slider_effective_max(spec, profiling_interval_ms);
    add_binding_update(updates, std::string(spec.slider_path), "min", std::to_string(spec.min_value));
    add_binding_update(updates, std::string(spec.slider_path), "max", std::to_string(effective_max));
    add_binding_update(updates, std::string(spec.slider_path), "step", std::to_string(spec.step));
    add_binding_update(updates, std::string(spec.slider_path), "value", std::to_string(value));

    const auto row_path = std::string(spec.row_path);
    for (size_t index = 0; index < 5; ++index) {
        const auto tick_name = "tick" + std::to_string(index);
        const int tick_value = make_debug_slider_tick_value(spec.min_value, effective_max, index);
        add_binding_update(
            updates,
            row_path + "/ticks/" + tick_name,
            tick_name,
            format_debug_slider_tick(tick_value, spec.tick_unit)
        );
    }
}

std::optional<SettingsDebugStorageEntries> list_debug_storage_entries()
{
    if (!StorageHelper::is_running()) {
        return std::nullopt;
    }

    auto nspace = make_settings_kv_namespace(SETTINGS_STORAGE_NAMESPACE);
    if (!nspace) {
        BROOKESIA_LOGD("Failed to resolve Settings debug namespace: %1%", nspace.error());
        return std::nullopt;
    }

    auto result = StorageHelper::call_function_sync<boost::json::array>(
                      StorageHelper::FunctionId::KVList,
                      *nspace,
                      service::helper::Timeout(SETTINGS_STORAGE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGD("Failed to list Settings debug preferences; use defaults: %1%", result.error());
        return SettingsDebugStorageEntries{
            .nspace = std::move(*nspace),
            .keys = {},
        };
    }

    std::vector<StorageHelper::EntryInfo> entries;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(result.value(), entries)) {
        BROOKESIA_LOGD("Failed to parse Settings debug preference list; use defaults");
        return SettingsDebugStorageEntries{
            .nspace = std::move(*nspace),
            .keys = {},
        };
    }

    SettingsDebugStorageEntries storage_entries{
        .nspace = std::move(*nspace),
        .keys = {},
    };
    storage_entries.keys.reserve(entries.size());
    for (const auto &entry : entries) {
        storage_entries.keys.insert(entry.key);
    }
    return storage_entries;
}

template <typename T>
T load_debug_storage_value(
    std::string_view raw_key,
    T default_value,
    const std::optional<SettingsDebugStorageEntries> &storage_entries
)
{
    if (!storage_entries.has_value()) {
        return default_value;
    }

    auto key = make_settings_kv_key(raw_key);
    if (!key) {
        BROOKESIA_LOGD("Failed to resolve Settings debug key '%1%': %2%", raw_key, key.error());
        return default_value;
    }
    if (storage_entries->keys.find(*key) == storage_entries->keys.end()) {
        return default_value;
    }

    auto result = StorageHelper::get_key_value<T>(
                      storage_entries->nspace,
                      *key,
                      SETTINGS_STORAGE_TIMEOUT_MS
                  );
    if (!result) {
        BROOKESIA_LOGD("Settings debug key '%1%' is not stored; use default: %2%", raw_key, result.error());
        return default_value;
    }
    return *result;
}

} // namespace

void SettingsApp::bind_utils_debug_service()
{
    if (utils_debug_service_binding_.is_valid()) {
        return;
    }

    if (!UtilsHelper::is_available()) {
        if (!utils_debug_unavailable_logged_) {
            BROOKESIA_LOGD("Utils service is unavailable; Settings debug changes will be applied later");
            utils_debug_unavailable_logged_ = true;
        }
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(UtilsHelper::get_name().data());
    if (!binding.is_valid()) {
        if (!utils_debug_unavailable_logged_) {
            BROOKESIA_LOGD("Utils service is available but failed to bind");
            utils_debug_unavailable_logged_ = true;
        }
        return;
    }

    utils_debug_service_binding_ = std::move(binding);
    utils_debug_unavailable_logged_ = false;
}

void SettingsApp::release_utils_debug_service()
{
    utils_debug_service_binding_.release();
    utils_debug_capabilities_ = UtilsHelper::DebugCapabilities{};
}

void SettingsApp::refresh_utils_debug_capabilities()
{
    utils_debug_capabilities_ = UtilsHelper::DebugCapabilities{};
    bind_utils_debug_service();
    if (!utils_debug_service_binding_.is_valid()) {
        return;
    }

    auto result = UtilsHelper::call_function_sync<boost::json::object>(
                      UtilsHelper::FunctionId::GetDebugCapabilities,
                      service::helper::Timeout(UTILS_DEBUG_SERVICE_TIMEOUT_MS)
                  );
    if (!result) {
        BROOKESIA_LOGD("Failed to get Utils capabilities: %1%", result.error());
        return;
    }
    if (!BROOKESIA_DESCRIBE_FROM_JSON(result.value(), utils_debug_capabilities_)) {
        BROOKESIA_LOGD("Failed to parse Utils capabilities");
        utils_debug_capabilities_ = UtilsHelper::DebugCapabilities{};
    }
    utils_debug_capabilities_.thread_debug_supported =
        utils_debug_capabilities_.thread_debug_supported && is_platform_thread_debug_supported();
}

std::expected<void, std::string> SettingsApp::push_debug_config_to_utils()
{
    bind_utils_debug_service();
    if (!utils_debug_service_binding_.is_valid()) {
        return std::unexpected("Utils service is unavailable");
    }

    auto config = UtilsHelper::DebugConfig{
        .memory_sample_interval_ms = static_cast<uint32_t>(debug_ui_state_.memory_sample_interval_ms),
        .memory_internal_free_percent_threshold =
            static_cast<uint32_t>(debug_ui_state_.memory_internal_free_percent_threshold),
        .memory_internal_largest_free_block_threshold_kb =
            static_cast<uint32_t>(debug_ui_state_.memory_internal_largest_free_block_threshold_kb),
        .memory_external_free_percent_threshold =
            static_cast<uint32_t>(debug_ui_state_.memory_external_free_percent_threshold),
        .memory_external_largest_free_block_threshold_kb =
            static_cast<uint32_t>(debug_ui_state_.memory_external_largest_free_block_threshold_kb),
        .thread_profiling_interval_ms = static_cast<uint32_t>(debug_ui_state_.thread_profiling_interval_ms),
        .thread_sampling_duration_ms = static_cast<uint32_t>(debug_ui_state_.thread_sampling_duration_ms),
        .thread_idle_cpu_percent_threshold =
            static_cast<uint32_t>(debug_ui_state_.thread_idle_cpu_percent_threshold),
        .thread_stack_high_water_mark_threshold_bytes =
            static_cast<uint32_t>(debug_ui_state_.thread_stack_high_water_mark_threshold_bytes),
    };
    auto result = UtilsHelper::set_debug_config(config, UTILS_DEBUG_SERVICE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(result.error());
    }

    result = debug_ui_state_.memory_enabled
        ? UtilsHelper::start_memory_debug(UTILS_DEBUG_SERVICE_TIMEOUT_MS)
        : UtilsHelper::stop_memory_debug(UTILS_DEBUG_SERVICE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(result.error());
    }

    const bool thread_enabled = debug_ui_state_.thread_enabled &&
        utils_debug_capabilities_.thread_debug_supported && is_platform_thread_debug_supported();
    result = thread_enabled
        ? UtilsHelper::start_thread_debug(UTILS_DEBUG_SERVICE_TIMEOUT_MS)
        : UtilsHelper::stop_thread_debug(UTILS_DEBUG_SERVICE_TIMEOUT_MS);
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

SettingsApp::DebugUiState SettingsApp::load_debug_preferences()
{
    bind_storage_service();

    const auto storage_entries = list_debug_storage_entries();
    DebugUiState state;
    state.memory_enabled = load_debug_storage_value(
                               DEBUG_KEY_MEMORY_ENABLED,
                               state.memory_enabled,
                               storage_entries
                           );
    state.thread_enabled = load_debug_storage_value(
                               DEBUG_KEY_THREAD_ENABLED,
                               state.thread_enabled,
                               storage_entries
                           );
    state.gui_view_debug_enabled = load_debug_storage_value(
                                       DEBUG_KEY_GUI_VIEW_DEBUG_ENABLED,
                                       state.gui_view_debug_enabled,
                                       storage_entries
                                   );

    auto set_slider_value = [&state](DebugSliderId id, int value) {
        switch (id) {
        case DebugSliderId::MemorySampleInterval:
            state.memory_sample_interval_ms = value;
            break;
        case DebugSliderId::MemoryInternalFreePercent:
            state.memory_internal_free_percent_threshold = value;
            break;
        case DebugSliderId::MemoryInternalLargestFreeBlock:
            state.memory_internal_largest_free_block_threshold_kb = value;
            break;
        case DebugSliderId::MemoryExternalFreePercent:
            state.memory_external_free_percent_threshold = value;
            break;
        case DebugSliderId::MemoryExternalLargestFreeBlock:
            state.memory_external_largest_free_block_threshold_kb = value;
            break;
        case DebugSliderId::ThreadProfilingInterval:
            state.thread_profiling_interval_ms = value;
            break;
        case DebugSliderId::ThreadSamplingDuration:
            state.thread_sampling_duration_ms = value;
            break;
        case DebugSliderId::ThreadIdleCpuPercent:
            state.thread_idle_cpu_percent_threshold = value;
            break;
        case DebugSliderId::ThreadStackHighWaterMark:
            state.thread_stack_high_water_mark_threshold_bytes = value;
            break;
        }
    };

    for (const auto &spec : DEBUG_SLIDER_SPECS) {
        const int loaded_value = load_debug_storage_value(spec.storage_key, spec.default_value, storage_entries);
        set_slider_value(spec.id, clamp_debug_int(loaded_value, spec.min_value, spec.max_value));
    }

    if (const auto *duration_spec = find_debug_slider_spec_by_id(DebugSliderId::ThreadSamplingDuration);
            duration_spec != nullptr) {
        state.thread_sampling_duration_ms = clamp_debug_slider_value(
                                                *duration_spec,
                                                state.thread_sampling_duration_ms,
                                                state.thread_profiling_interval_ms
                                            );
    }
    return state;
}

std::expected<void, std::string> SettingsApp::save_debug_preference(std::string_view key, bool value)
{
    bind_storage_service();
    if (!storage_service_binding_.is_valid()) {
        return std::unexpected("Storage service is unavailable");
    }

    auto kv_name = make_settings_kv_name(SETTINGS_STORAGE_NAMESPACE, key);
    if (!kv_name) {
        return std::unexpected("Failed to resolve Settings debug key: " + kv_name.error());
    }

    if (!StorageHelper::save_key_value_async(kv_name->nspace, kv_name->key, value)) {
        return std::unexpected("Failed to submit Settings debug preference save request");
    }
    return {};
}

std::expected<void, std::string> SettingsApp::save_debug_preference(std::string_view key, int value)
{
    bind_storage_service();
    if (!storage_service_binding_.is_valid()) {
        return std::unexpected("Storage service is unavailable");
    }

    auto kv_name = make_settings_kv_name(SETTINGS_STORAGE_NAMESPACE, key);
    if (!kv_name) {
        return std::unexpected("Failed to resolve Settings debug key: " + kv_name.error());
    }

    if (!StorageHelper::save_key_value_async(kv_name->nspace, kv_name->key, value)) {
        return std::unexpected("Failed to submit Settings debug preference save request");
    }
    return {};
}

std::expected<void, std::string> SettingsApp::refresh_debug_state(system::core::AppContext &context)
{
    const bool thread_supported = utils_debug_capabilities_.thread_debug_supported &&
                                  is_platform_thread_debug_supported();

    auto get_slider_value = [this](DebugSliderId id) {
        switch (id) {
        case DebugSliderId::MemorySampleInterval:
            return debug_ui_state_.memory_sample_interval_ms;
        case DebugSliderId::MemoryInternalFreePercent:
            return debug_ui_state_.memory_internal_free_percent_threshold;
        case DebugSliderId::MemoryInternalLargestFreeBlock:
            return debug_ui_state_.memory_internal_largest_free_block_threshold_kb;
        case DebugSliderId::MemoryExternalFreePercent:
            return debug_ui_state_.memory_external_free_percent_threshold;
        case DebugSliderId::MemoryExternalLargestFreeBlock:
            return debug_ui_state_.memory_external_largest_free_block_threshold_kb;
        case DebugSliderId::ThreadProfilingInterval:
            return debug_ui_state_.thread_profiling_interval_ms;
        case DebugSliderId::ThreadSamplingDuration:
            return debug_ui_state_.thread_sampling_duration_ms;
        case DebugSliderId::ThreadIdleCpuPercent:
            return debug_ui_state_.thread_idle_cpu_percent_threshold;
        case DebugSliderId::ThreadStackHighWaterMark:
            return debug_ui_state_.thread_stack_high_water_mark_threshold_bytes;
        }
        return 0;
    };

    std::vector<gui::BindingValueUpdate> updates;
    add_binding_update(
        updates,
        DEBUG_MEMORY_SWITCH_PATH,
        "checked",
        debug_ui_state_.memory_enabled ? "true" : "false"
    );
    add_binding_update(
        updates,
        DEBUG_THREAD_SWITCH_PATH,
        "checked",
        debug_ui_state_.thread_enabled && thread_supported ? "true" : "false"
    );
    add_binding_update(
        updates,
        DEBUG_GUI_SWITCH_PATH,
        "checked",
        debug_ui_state_.gui_view_debug_enabled ? "true" : "false"
    );
    for (const auto &spec : DEBUG_SLIDER_SPECS) {
        add_debug_slider_binding_updates(
            updates,
            spec,
            get_slider_value(spec.id),
            debug_ui_state_.thread_profiling_interval_ms
        );
    }
    add_control_state_updates(updates, DEBUG_THREAD_SWITCH_ROW_PATH, DEBUG_THREAD_SWITCH_PATH, thread_supported);
    add_control_state_updates(
        updates,
        DEBUG_THREAD_INTERVAL_ROW_PATH,
        DEBUG_THREAD_INTERVAL_SLIDER_PATH,
        thread_supported
    );
    add_control_state_updates(
        updates,
        DEBUG_THREAD_DURATION_ROW_PATH,
        DEBUG_THREAD_DURATION_SLIDER_PATH,
        thread_supported
    );
    add_control_state_updates(updates, DEBUG_THREAD_IDLE_ROW_PATH, DEBUG_THREAD_IDLE_SLIDER_PATH, thread_supported);
    add_control_state_updates(updates, DEBUG_THREAD_STACK_ROW_PATH, DEBUG_THREAD_STACK_SLIDER_PATH, thread_supported);
    return context.gui().set_binding_values(updates);
}

void SettingsApp::handle_debug_device_name_click(system::core::AppContext &context)
{
    if (current_page_ != PAGE_DEVICE) {
        reset_debug_entry_click_state();
        return;
    }

    const auto now = SteadyClock::now();
    if (debug_entry_click_count_ == 0 ||
            elapsed_ms_since(debug_entry_last_click_at_, now) <= DEBUG_ENTRY_CLICK_TIMEOUT_MS) {
        ++debug_entry_click_count_;
    } else {
        debug_entry_click_count_ = 1;
    }
    debug_entry_last_click_at_ = now;

    if (debug_entry_click_count_ < DEBUG_ENTRY_CLICK_COUNT) {
        return;
    }

    reset_debug_entry_click_state();
    auto result = context.gui().trigger_screen_flow(CONTENT_FLOW_ID, ACTION_OPEN_DEBUG);
    if (!result) {
        BROOKESIA_LOGW("Failed to open Settings Debug page: %1%", result.error());
        return;
    }
    current_page_ = PAGE_DEBUG;
    refresh_utils_debug_capabilities();
    debug_ui_state_ = load_debug_preferences();
    if (auto gui_debug_result = context.gui().set_view_debug_enabled(debug_ui_state_.gui_view_debug_enabled);
            !gui_debug_result) {
        BROOKESIA_LOGW("Failed to restore GUI debug preference: %1%", gui_debug_result.error());
    }
    if (auto push_result = push_debug_config_to_utils(); !push_result) {
        BROOKESIA_LOGD("Failed to push Settings debug preferences after opening Debug page: %1%", push_result.error());
    }
    if (auto refresh_result = refresh_debug_state(context); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh Settings Debug page: %1%", refresh_result.error());
    }
    if (auto header_result = refresh_header(context); !header_result) {
        BROOKESIA_LOGW("Failed to refresh Settings header after opening Debug page: %1%", header_result.error());
    }
}

void SettingsApp::reset_debug_entry_click_state()
{
    debug_entry_click_count_ = 0;
    debug_entry_last_click_at_ = {};
}

void SettingsApp::handle_debug_switch_event(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }

    auto checked = event.get_bool("checked");
    if (!checked) {
        BROOKESIA_LOGW("Ignore Settings debug switch event without checked value");
        return;
    }

    std::expected<void, std::string> save_result;
    if (event.action == ACTION_DEBUG_MEMORY_TOGGLE) {
        debug_ui_state_.memory_enabled = *checked;
        save_result = save_debug_preference(DEBUG_KEY_MEMORY_ENABLED, debug_ui_state_.memory_enabled);
    } else if (event.action == ACTION_DEBUG_THREAD_TOGGLE) {
        if (!utils_debug_capabilities_.thread_debug_supported || !is_platform_thread_debug_supported()) {
            debug_ui_state_.thread_enabled = false;
            (void)refresh_debug_state(*context_);
            return;
        }
        debug_ui_state_.thread_enabled = *checked;
        save_result = save_debug_preference(DEBUG_KEY_THREAD_ENABLED, debug_ui_state_.thread_enabled);
    } else if (event.action == ACTION_DEBUG_GUI_TOGGLE) {
        debug_ui_state_.gui_view_debug_enabled = *checked;
        if (auto gui_debug_result =
                context_->gui().set_view_debug_enabled(debug_ui_state_.gui_view_debug_enabled);
                !gui_debug_result) {
            BROOKESIA_LOGW("Failed to update GUI debug state: %1%", gui_debug_result.error());
            debug_ui_state_.gui_view_debug_enabled = !*checked;
            (void)refresh_debug_state(*context_);
            return;
        }
        save_result = save_debug_preference(
                          DEBUG_KEY_GUI_VIEW_DEBUG_ENABLED,
                          debug_ui_state_.gui_view_debug_enabled
                      );
    } else {
        return;
    }

    if (!save_result) {
        BROOKESIA_LOGW("Failed to save Settings debug switch: %1%", save_result.error());
    }
    if (auto push_result = push_debug_config_to_utils(); !push_result) {
        BROOKESIA_LOGD("Failed to push Settings debug switch: %1%", push_result.error());
    }
    if (auto refresh_result = refresh_debug_state(*context_); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh Settings Debug switch state: %1%", refresh_result.error());
    }
}

void SettingsApp::handle_debug_slider_event(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }

    const auto *spec = find_debug_slider_spec_by_action(event.action);
    if (spec == nullptr) {
        return;
    }
    if (spec->thread_only &&
            (!utils_debug_capabilities_.thread_debug_supported || !is_platform_thread_debug_supported())) {
        (void)refresh_debug_state(*context_);
        return;
    }

    auto value = event.get_double("value");
    if (!value) {
        BROOKESIA_LOGW("Ignore Settings debug slider event without numeric value");
        return;
    }
    const int rounded_value = static_cast<int>(std::lround(*value));
    const int clamped_value = clamp_debug_slider_value(
                                  *spec,
                                  rounded_value,
                                  debug_ui_state_.thread_profiling_interval_ms
                              );

    auto set_slider_value = [this](DebugSliderId id, int value) {
        switch (id) {
        case DebugSliderId::MemorySampleInterval:
            debug_ui_state_.memory_sample_interval_ms = value;
            break;
        case DebugSliderId::MemoryInternalFreePercent:
            debug_ui_state_.memory_internal_free_percent_threshold = value;
            break;
        case DebugSliderId::MemoryInternalLargestFreeBlock:
            debug_ui_state_.memory_internal_largest_free_block_threshold_kb = value;
            break;
        case DebugSliderId::MemoryExternalFreePercent:
            debug_ui_state_.memory_external_free_percent_threshold = value;
            break;
        case DebugSliderId::MemoryExternalLargestFreeBlock:
            debug_ui_state_.memory_external_largest_free_block_threshold_kb = value;
            break;
        case DebugSliderId::ThreadProfilingInterval:
            debug_ui_state_.thread_profiling_interval_ms = value;
            break;
        case DebugSliderId::ThreadSamplingDuration:
            debug_ui_state_.thread_sampling_duration_ms = value;
            break;
        case DebugSliderId::ThreadIdleCpuPercent:
            debug_ui_state_.thread_idle_cpu_percent_threshold = value;
            break;
        case DebugSliderId::ThreadStackHighWaterMark:
            debug_ui_state_.thread_stack_high_water_mark_threshold_bytes = value;
            break;
        }
    };

    std::vector<std::expected<void, std::string>> save_results;
    set_slider_value(spec->id, clamped_value);
    save_results.push_back(save_debug_preference(spec->storage_key, clamped_value));

    if (spec->id == DebugSliderId::ThreadProfilingInterval) {
        const int previous_duration = debug_ui_state_.thread_sampling_duration_ms;
        if (const auto *duration_spec = find_debug_slider_spec_by_id(DebugSliderId::ThreadSamplingDuration);
                duration_spec != nullptr) {
            debug_ui_state_.thread_sampling_duration_ms = clamp_debug_slider_value(
                                                            *duration_spec,
                                                            debug_ui_state_.thread_sampling_duration_ms,
                                                            debug_ui_state_.thread_profiling_interval_ms
                                                        );
            if (debug_ui_state_.thread_sampling_duration_ms != previous_duration) {
                save_results.push_back(save_debug_preference(
                    duration_spec->storage_key,
                    debug_ui_state_.thread_sampling_duration_ms
                ));
            }
        }
    }

    for (const auto &result : save_results) {
        if (!result) {
            BROOKESIA_LOGW("Failed to save Settings debug slider: %1%", result.error());
        }
    }
    if (auto push_result = push_debug_config_to_utils(); !push_result) {
        BROOKESIA_LOGD("Failed to push Settings debug slider: %1%", push_result.error());
    }
    if (auto refresh_result = refresh_debug_state(*context_); !refresh_result) {
        BROOKESIA_LOGW("Failed to refresh Settings Debug slider state: %1%", refresh_result.error());
    }
}
