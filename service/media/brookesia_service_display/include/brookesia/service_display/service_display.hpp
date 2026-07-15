/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json/array.hpp"
#include "boost/json/object.hpp"
#include "brookesia/lib_utils/signal.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/service_display/macro_configs.h"
#include "brookesia/service_display/types.hpp"
#include "brookesia/service_helper/media/display.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/common.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

class Display: public ServiceBase {
public:
    using Helper = helper::Display;
    using PixelFormat = display::PixelFormat;
    using OutputSlot = display::OutputSlot;
    using OutputInfo = display::OutputInfo;
    using OutputTouchCapability = display::OutputTouchCapability;
    using OutputBacklightCapability = display::OutputBacklightCapability;
    using SourceInfo = display::SourceInfo;
    using TouchInfo = display::TouchInfo;
    using SourceState = display::SourceState;
    using FrameInfo = display::FrameInfo;
    using PresentResult = display::PresentResult;
    using PresentSubmitState = display::PresentSubmitState;
    using AsyncSubmitResult = display::AsyncSubmitResult;
    using TouchPoint = display::TouchPoint;
    using TouchSnapshot = display::TouchSnapshot;
    using TouchGestureEventType = display::TouchGestureEventType;
    using TouchGestureDirection = display::TouchGestureDirection;
    using TouchGestureArea = display::TouchGestureArea;
    using TouchGestureThreshold = display::TouchGestureThreshold;
    using TouchGestureConfig = display::TouchGestureConfig;
    using TouchGestureInfo = display::TouchGestureInfo;
    using CompletionCallback = std::function<void(uint32_t frame_id, PresentResult result)>;
    struct BufferOutputConfig {
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
        PixelFormat pixel_format = PixelFormat::RGB565;
        RawBuffer buffer;
        size_t stride_bytes = 0;
    };
    struct BufferOutputView {
        OutputInfo info;
        RawBuffer buffer;
        size_t stride_bytes = 0;
    };
    using SourceStateChangedSignal = esp_brookesia::lib_utils::signal<void(
                                         const std::string &source_name, const std::string &output_name, SourceState state
                                     )>;
    using ActiveSourceChangedSignal = esp_brookesia::lib_utils::signal<void(
                                          const std::string &output_name, const std::string &source_name
                                      )>;
    using OutputRegisteredSignal = esp_brookesia::lib_utils::signal<void(const OutputInfo &info)>;
    using OutputUnregisteredSignal = esp_brookesia::lib_utils::signal<void(const std::string &output_name)>;
    using FramePresentedSignal = esp_brookesia::lib_utils::signal<void(const std::string &output_name, const FrameInfo &frame)>;
    using TouchUpdatedSignal = esp_brookesia::lib_utils::signal<void(
                                   const std::string &output_name, const TouchSnapshot &snapshot
                               )>;
    using TouchGestureSignal = esp_brookesia::lib_utils::signal<void(
                                   const std::string &output_name, const TouchGestureInfo &info
                               )>;
    using BufferOutputWriter = std::function<bool(BufferOutputView &view)>;

    static Display &get_instance()
    {
        static Display instance;
        return instance;
    }

    std::vector<OutputInfo> get_outputs() const;
    std::vector<SourceInfo> get_sources() const;
    std::expected<BufferOutputView, std::string> get_buffer_output(std::string_view output_name) const;
    std::expected<uint32_t, std::string> register_output(BufferOutputConfig config);
    std::expected<void, std::string> unregister_output(std::string_view output_name);
    std::expected<uint32_t, std::string> register_source(SourceInfo source);
    std::expected<void, std::string> unregister_source(uint32_t source_id);
    std::expected<void, std::string> unregister_source(std::string_view source_name);
    std::expected<void, std::string> request_output(uint32_t source_id, std::string_view output_name);
    std::expected<void, std::string> request_output(std::string_view source_name, std::string_view output_name);
    std::expected<void, std::string> release_output(uint32_t source_id, std::string_view output_name);
    std::expected<void, std::string> release_output(std::string_view source_name, std::string_view output_name);
    std::expected<void, std::string> set_active_source(std::string_view output_name, std::string_view source_name);
    std::expected<std::string, std::string> get_active_source(std::string_view output_name) const;
    std::expected<void, std::string> set_active_source_role(std::string_view output_name, std::string_view role);
    std::expected<std::string, std::string> get_active_role(std::string_view output_name) const;
    std::vector<std::string> get_source_roles() const;
    std::expected<void, std::string> set_touch_gesture_config(uint32_t output_id, TouchGestureConfig config);
    std::expected<TouchGestureConfig, std::string> get_touch_gesture_config(uint32_t output_id) const;
    std::expected<TouchSnapshot, std::string> get_touch_snapshot(std::string_view output_name) const;
    std::expected<hal::display::TouchIface::DriverSpecific, std::string> get_touch_driver_specific(
        std::string_view output_name
    ) const;

    /**
     * @brief Inject synthetic touch points for an output, overriding the hardware snapshot.
     *
     * Synthetic points take precedence over hardware reads in @ref get_touch_snapshot until
     * @ref clear_injected_touch is called. Pass an empty @p points vector to synthesize a release
     * (the override stays active and reports zero points). This drives the same LVGL input pipeline
     * as a real touch, so it can simulate presses/clicks programmatically (e.g. for automated tests).
     *
     * @param[in] output_name Target output name (must have a bound touch).
     * @param[in] points      Synthetic points (empty = released).
     */
    std::expected<void, std::string> inject_touch(std::string_view output_name, std::vector<TouchPoint> points);

    /// Convenience overload: inject a single pressed point at @p x, @p y.
    std::expected<void, std::string> inject_touch(std::string_view output_name, int32_t x, int32_t y);

    /// Remove any synthetic touch override for @p output_name, restoring hardware snapshots.
    std::expected<void, std::string> clear_injected_touch(std::string_view output_name);
    /**
     * @brief Present a frame through the active output synchronously or as a no-wait submit.
     *
     * A non-zero @p timeout_ms waits until the HAL backend has finished using @p data. A zero
     * @p timeout_ms only submits the frame; in that mode @c PresentResult::Presented means submitted.
     */
    PresentResult present_frame_sync(
        uint32_t source_id, std::string_view output_name, const FrameInfo &frame, const RawBuffer &data,
        uint32_t timeout_ms = BROOKESIA_SERVICE_DISPLAY_DRAW_TIMEOUT_MS
    );
    /**
     * @brief Present a buffer output frame by letting the producer write directly into the output buffer.
     *
     * This direct path is only valid for @c OutputSlot::Buffer outputs. It keeps the same source/output/active
     * validation and frame-presented signal as @c present_frame_sync(), while avoiding an extra service-side copy.
     */
    PresentResult present_buffer_frame_sync(
        uint32_t source_id, std::string_view output_name, const FrameInfo &frame, BufferOutputWriter writer
    );
    /**
     * @brief Present a frame asynchronously using a borrowed input buffer.
     *
     * The producer must keep @p data valid until @p on_complete is invoked.
     */
    AsyncSubmitResult present_frame_async(
        uint32_t source_id, std::string_view output_name, const FrameInfo &frame, const RawBuffer &data,
        CompletionCallback on_complete, uint32_t timeout_ms = BROOKESIA_SERVICE_DISPLAY_DRAW_TIMEOUT_MS
    );

    esp_brookesia::lib_utils::connection connect_source_state_changed(const SourceStateChangedSignal::slot_type &slot)
    {
        return source_state_changed_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_active_source_changed(const ActiveSourceChangedSignal::slot_type &slot)
    {
        return active_source_changed_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_output_registered(const OutputRegisteredSignal::slot_type &slot)
    {
        return output_registered_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_output_unregistered(const OutputUnregisteredSignal::slot_type &slot)
    {
        return output_unregistered_signal_.connect(slot);
    }

    esp_brookesia::lib_utils::connection connect_frame_presented(
        std::string_view output_name, const FramePresentedSignal::slot_type &slot
    );

    esp_brookesia::lib_utils::connection connect_touch_updated(
        std::string_view output_name, const TouchUpdatedSignal::slot_type &slot
    );
    esp_brookesia::lib_utils::connection connect_touch_gesture(
        std::string_view output_name, const TouchGestureSignal::slot_type &slot
    );

private:
    static std::string get_component_version();

    struct AsyncFrame {
        uint32_t frame_id = 0;
        uint32_t source_id = 0;
        std::string output_name;
        FrameInfo frame;
        RawBuffer data;
        uint32_t timeout_ms = BROOKESIA_SERVICE_DISPLAY_DRAW_TIMEOUT_MS;
        CompletionCallback on_complete;
    };

    struct BufferOutputContext {
        RawBuffer buffer;
        size_t stride_bytes = 0;
    };

    struct OutputDrawTarget {
        OutputInfo info;
        std::shared_ptr<hal::display::PanelIface> panel;
        BufferOutputContext buffer;
    };

    struct OutputContext {
        OutputInfo info;
        hal::InterfaceHandle<hal::display::PanelIface> panel;
        BufferOutputContext buffer;
        std::shared_ptr<std::mutex> draw_mutex;
        uint32_t active_source_id = 0;
        uint32_t touch_id = 0;
        TouchGestureConfig gesture_config;
        lib_utils::TaskScheduler::TaskId gesture_task_id = 0;
        TouchGestureInfo gesture_info;
        uint64_t gesture_touch_start_time_ms = 0;
        uint64_t gesture_release_start_time_ms = 0;
        float gesture_direction_tan_threshold = 1.0F;
        uint8_t gesture_active_track_id = 0;
        bool gesture_detection_started = false;
        bool gesture_has_active_track = false;
        bool gesture_direction_locked = false;
        bool gesture_release_pending = false;
        std::optional<AsyncFrame> pending_frame;
        hal::InterfaceHandle<hal::display::BacklightIface> backlight;
        uint8_t backlight_brightness = 0;
        bool backlight_on = false;
        bool render_scheduled = false;
        uint32_t inflight_frame_id = 0;
        bool dynamic_output = false;
    };

    struct SourceContext {
        SourceInfo info;
        std::set<std::string> requested_outputs;
    };

    struct TouchInterruptBridge;

    struct TouchContext {
        TouchInfo info;
        hal::InterfaceHandle<hal::display::TouchIface> touch;
        TouchSnapshot snapshot;
        std::shared_ptr<TouchInterruptBridge> interrupt_bridge;
        lib_utils::TaskScheduler::TaskId poll_task_id = 0;
        bool read_scheduled = false;
        std::optional<std::vector<TouchPoint>> injected_points; ///< Active synthetic touch override.
        uint32_t injected_sequence = 0; ///< Monotonic sequence so each injection is observed once.
    };

    Display()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .description = "Manage display outputs, touch inputs, and backlight controls.",
        .version = get_component_version(),
        .dependencies = {
            helper::Storage::get_name().data(),
        },
    })
    {}
    ~Display() = default;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;
    void on_deinit() override;

    std::expected<boost::json::array, std::string> function_get_outputs();
    std::expected<boost::json::array, std::string> function_get_sources();
    std::expected<double, std::string> function_register_source(const boost::json::object &source_json);
    std::expected<void, std::string> function_unregister_source(const std::string &source_name);
    std::expected<void, std::string> function_request_output(
        const std::string &source_name, const std::string &output_name
    );
    std::expected<void, std::string> function_release_output(
        const std::string &source_name, const std::string &output_name
    );
    std::expected<void, std::string> function_set_active_source(
        const std::string &output_name, const std::string &source_name
    );
    std::expected<std::string, std::string> function_get_active_source(const std::string &output_name);
    std::expected<void, std::string> function_set_active_source_role(
        const std::string &output_name, const std::string &role
    );
    std::expected<std::string, std::string> function_get_active_role(const std::string &output_name);
    std::expected<boost::json::array, std::string> function_get_source_roles();
    std::expected<void, std::string> function_set_touch_gesture_config(
        double output_id, const boost::json::object &config_json
    );
    std::expected<boost::json::object, std::string> function_get_touch_gesture_config(double output_id);
    std::expected<void, std::string> function_set_backlight_brightness(double output_id, double brightness);
    std::expected<double, std::string> function_get_backlight_brightness(double output_id);
    std::expected<void, std::string> function_set_backlight_on_off(double output_id, bool on);
    std::expected<bool, std::string> function_get_backlight_on_off(double output_id);
    std::expected<void, std::string> function_load_data(double output_id);
    std::expected<void, std::string> function_reset_data(double output_id);

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetOutputs, function_get_outputs()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSources, function_get_sources()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::RegisterSource, boost::json::object,
                function_register_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::UnregisterSource, std::string,
                function_unregister_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::RequestOutput, std::string, std::string,
                function_request_output(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::ReleaseOutput, std::string, std::string,
                function_release_output(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetActiveSource, std::string, std::string,
                function_set_active_source(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetActiveSource, std::string,
                function_get_active_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetActiveSourceRole, std::string, std::string,
                function_set_active_source_role(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetActiveRole, std::string,
                function_get_active_role(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSourceRoles, function_get_source_roles()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetTouchGestureConfig, double, boost::json::object,
                function_set_touch_gesture_config(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetTouchGestureConfig, double,
                function_get_touch_gesture_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetBacklightBrightness, double, double,
                function_set_backlight_brightness(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetBacklightBrightness, double,
                function_get_backlight_brightness(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetBacklightOnOff, double, bool,
                function_set_backlight_on_off(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetBacklightOnOff, double,
                function_get_backlight_on_off(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::LoadData, double,
                function_load_data(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::ResetData, double,
                function_reset_data(PARAM)
            ),
        };
    }

    std::expected<uint32_t, std::string> find_output_id_locked(std::string_view output_name) const;
    std::expected<uint32_t, std::string> validate_output_id_param(double output_id) const;
    std::expected<uint32_t, std::string> find_source_id_locked(std::string_view source_name) const;
    std::expected<uint32_t, std::string> find_touch_id_locked(std::string_view touch_name) const;
    std::expected<void, std::string> validate_source_output_locked(
        uint32_t source_id, std::string_view output_name
    ) const;
    std::expected<uint8_t, std::string> validate_percentage(double value, const char *name) const;
    TouchGestureConfig build_default_touch_gesture_config_locked(const OutputContext &output) const;
    std::expected<TouchGestureConfig, std::string> resolve_touch_gesture_config_locked(
        const OutputContext &output, const TouchGestureConfig &config
    ) const;
    void reset_touch_gesture_state_locked(OutputContext &output);
    bool start_touch_gesture_task(uint32_t output_id);
    void stop_touch_gesture_task(uint32_t output_id);
    void stop_touch_gesture_tasks();
    void process_touch_gesture(uint32_t output_id);
    uint8_t get_touch_gesture_area(const OutputContext &output, int x, int y) const;
    std::optional<TouchPoint> select_touch_gesture_point(
        const OutputContext &output, const TouchSnapshot &snapshot
    ) const;
    TouchPoint normalize_touch_gesture_point(const TouchPoint &point, const OutputContext &output, const TouchInfo &touch)
    const;
    uint32_t allocate_frame_id_locked();
    bool is_frame_valid_for_output(const FrameInfo &frame, const OutputContext &output, size_t data_size) const;
    bool is_buffer_output_valid(const BufferOutputConfig &config, size_t bpp) const;
    PresentResult present_frame_to_output(
        const OutputDrawTarget &target, const FrameInfo &frame, const RawBuffer &data, uint32_t timeout_ms
    ) const;
    size_t bytes_per_pixel(PixelFormat pixel_format) const;
    std::string get_render_task_group() const
    {
        return get_attributes().name + "_render";
    }
    bool schedule_render_output_locked(uint32_t output_id);
    void render_output(uint32_t output_id);
    void drop_pending_frames_locked(std::vector<AsyncFrame> &dropped_frames);
    void complete_async_frame(AsyncFrame frame, PresentResult result);
    void bind_default_touches_locked();
    void refresh_output_capability_locked(OutputContext &output);
    OutputTouchCapability make_output_touch_capability_locked(const TouchContext &touch) const;
    OutputBacklightCapability make_output_backlight_capability(const OutputContext &output) const;
    void refresh_touch_bound_outputs_locked();
    std::string get_touch_task_group() const
    {
        return get_attributes().name + "_touch";
    }
    bool start_touch_tasks();
    bool start_touch_task(uint32_t touch_id);
    void stop_touch_tasks();
    bool schedule_touch_read(uint32_t touch_id);
    void read_touch(uint32_t touch_id);
    void emit_touch_updated(const std::vector<std::string> &output_names, const TouchSnapshot &snapshot);
    void emit_touch_gesture(const TouchGestureInfo &info);
    void emit_output_registered(const OutputInfo &info);
    void emit_output_unregistered(const std::string &output_name);
    void emit_frame_presented(const std::string &output_name, const FrameInfo &frame);
    void emit_source_state_changed(const std::string &source_name, const std::string &output_name, SourceState state);
    void emit_active_source_changed(const std::string &output_name, const std::string &source_name);
    void bind_backlights_to_outputs_locked(std::vector<hal::InterfaceHandle<hal::display::BacklightIface>> backlights);
    bool apply_backlight_state_to_hal(OutputContext &output);
    void load_backlight_data_from_storage_locked(OutputContext &output);
    void save_backlight_brightness_data(const std::string &storage_output_id, uint8_t brightness);
    void save_backlight_on_off_data(const std::string &storage_output_id, bool on);
    void reset_backlight_data_locked(OutputContext &output);
    std::expected<std::vector<uint32_t>, std::string> collect_backlight_output_ids_locked(uint32_t output_id) const;
    std::string get_backlight_storage_output_id(const OutputContext &output) const;
    std::expected<std::string, std::string> make_backlight_storage_key(
        std::string_view storage_output_id, std::string_view key
    ) const;
    std::expected<std::string, std::string> make_backlight_storage_namespace() const;
    bool emit_backlight_brightness_changed(uint32_t output_id, const std::string &output_name, uint8_t brightness);
    bool emit_backlight_on_off_changed(uint32_t output_id, const std::string &output_name, bool on);
    static uint8_t map_percentage_to_hardware(uint8_t percentage, uint8_t min, uint8_t max);

    mutable std::mutex mutex_;
    std::map<uint32_t, OutputContext> outputs_;
    std::map<uint32_t, SourceContext> sources_;
    std::map<uint32_t, TouchContext> touches_;
    uint32_t next_output_id_ = 1;
    uint32_t next_source_id_ = 1;
    uint32_t next_frame_id_ = 1;
    SourceStateChangedSignal source_state_changed_signal_;
    ActiveSourceChangedSignal active_source_changed_signal_;
    OutputRegisteredSignal output_registered_signal_;
    OutputUnregisteredSignal output_unregistered_signal_;
    FramePresentedSignal frame_presented_signal_;
    TouchUpdatedSignal touch_updated_signal_;
    TouchGestureSignal touch_gesture_signal_;
};

} // namespace esp_brookesia::service
