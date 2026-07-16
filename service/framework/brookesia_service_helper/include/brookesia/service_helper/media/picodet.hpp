/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Schema/type definitions for the PicoDet object-detection service.
 */
class PicoDet : public Base<PicoDet> {
public:
    /**
     * @brief Parameters for `Open` (passed as one JSON object).
     */
    struct OpenConfig {
        std::string model_dir;          ///< Directory holding manifest.json + the .espdl file.
        double score_thr = -1.0;        ///< Optional score-threshold override; < 0 keeps the manifest value.
        double nms_thr = -1.0;          ///< Optional NMS-threshold override; < 0 keeps the manifest value.
    };

    /**
     * @brief One detected object returned by `Detect`.
     */
    struct Box {
        int category = 0;               ///< Class index.
        std::string label;              ///< Resolved class name (empty if the model has no labels).
        double score = 0.0;             ///< Confidence in [0, 1].
        std::vector<int> box;           ///< {left_up_x, left_up_y, right_down_x, right_down_y}.
    };

    /**
     * @brief Loaded-model info returned by `Open` and `GetInfo`.
     */
    struct Info {
        bool opened = false;            ///< Whether a model is currently loaded.
        int width = 0;                  ///< Model input width in pixels.
        int height = 0;                 ///< Model input height in pixels.
        std::vector<std::string> labels; ///< Class names, index-aligned with `Box::category`.
    };

    /**
     * @brief Parameters for `Attach` (passed as one JSON object; all fields optional).
     */
    struct AttachConfig {
        std::string frame_source = "VideoEncoder0";        ///< Service publishing the frame event.
        std::string frame_event = "StreamSinkFrameReady";  ///< Frame event name on that service.
        std::string display_output;     ///< Display output to present annotated frames to; empty = detection-only.
        int detect_every_n_frames = 5;  ///< Inference decimation; 1 = detect on every frame.
    };

    enum class FunctionId {
        Open,       ///< Load a model from the filesystem (resident until Close).
        Detect,     ///< Run inference on one image file, return boxes.
        Close,      ///< Unload the model.
        GetInfo,    ///< Query loaded state / input size / labels.
        Attach,     ///< Subscribe to a frame event and optionally present annotated frames.
        Detach,     ///< Unwire the detector from the pipeline.
        Max,
    };

    enum class EventId {
        DetectionUpdated, ///< Published after each inference while attached, with the fresh boxes.
        Max,
    };

    enum class FunctionOpenParam { Config };
    enum class FunctionDetectParam { ImagePath };
    enum class FunctionAttachParam { Config };
    enum class EventDetectionUpdatedParam { Boxes };

    static constexpr std::string_view get_name()
    {
        return "PicoDet";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_open(),
                function_schema_detect(),
                function_schema_close(),
                function_schema_get_info(),
                function_schema_attach(),
                function_schema_detach(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_detection_updated(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

private:
    static FunctionSchema function_schema_open()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Open),
            .description = (boost::format("Load a PicoDet model. Example config: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((OpenConfig{
                .model_dir = "/littlefs/models/picodet_cat", .score_thr = 0.6, .nms_thr = 0.7
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionOpenParam::Config),
                    .description = "Model load configuration. 'model_dir' is required; thresholds are optional overrides.",
                    .type = FunctionValueType::Object,
                },
            },
            .default_timeout_ms = 5000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Loaded model info. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Info{
                    .opened = true, .width = 224, .height = 224, .labels = {"cat"}
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_detect()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Detect),
            .description = "Run detection on one image file and return the detected boxes.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionDetectParam::ImagePath),
                    .description = "Filesystem path to a .jpg/.jpeg or .bmp image.",
                    .type = FunctionValueType::String,
                },
            },
            .default_timeout_ms = 5000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Array of boxes. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Box{
                    .category = 0, .label = "cat", .score = 0.87, .box = {12, 34, 120, 210}
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_close()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Close),
            .description = "Unload the model and reclaim memory.",
        };
    }

    static FunctionSchema function_schema_get_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetInfo),
            .description = "Query loaded state, model input size and labels.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Loaded model info. Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Info{
                    .opened = true, .width = 224, .height = 224, .labels = {"cat"}
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_attach()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Attach),
            .description = (boost::format(
                                "Wire the detector into the pipeline: subscribe to 'frame_event' on the "
                                "'frame_source' service (the caller owns that service's lifecycle) and run "
                                "inference every N frames, publishing DetectionUpdated. When 'display_output' "
                                "is set, also draw the boxes onto each frame and present it to that display "
                                "output. Requires Open first. Example config: %1%"
                            )
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((AttachConfig{
                .frame_source = "VideoEncoder0", .frame_event = "StreamSinkFrameReady",
                .display_output = "Output0", .detect_every_n_frames = 5
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionAttachParam::Config),
                    .description = "Attach configuration; all fields are optional.",
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(boost::json::object{}),
                },
            },
            .default_timeout_ms = 5000,
        };
    }

    static FunctionSchema function_schema_detach()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Detach),
            .description = "Unwire the detector from the frame source and release the display output.",
            .default_timeout_ms = 5000,
        };
    }

    static EventSchema event_schema_detection_updated()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::DetectionUpdated),
            .description = "Published after each inference while attached, with the fresh detection boxes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventDetectionUpdatedParam::Boxes),
                    .description = (boost::format("Array of boxes. Example item: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Box{
                        .category = 0, .label = "cat", .score = 0.87, .box = {12, 34, 120, 210}
                    }))).str(),
                    .type = EventItemType::Array,
                },
            },
        };
    }
};

BROOKESIA_DESCRIBE_ENUM(PicoDet::FunctionId, Open, Detect, Close, GetInfo, Attach, Detach, Max);
BROOKESIA_DESCRIBE_ENUM(PicoDet::EventId, DetectionUpdated, Max);
BROOKESIA_DESCRIBE_ENUM(PicoDet::FunctionOpenParam, Config);
BROOKESIA_DESCRIBE_ENUM(PicoDet::FunctionDetectParam, ImagePath);
BROOKESIA_DESCRIBE_ENUM(PicoDet::FunctionAttachParam, Config);
BROOKESIA_DESCRIBE_ENUM(PicoDet::EventDetectionUpdatedParam, Boxes);
BROOKESIA_DESCRIBE_STRUCT(PicoDet::OpenConfig, (), (model_dir, score_thr, nms_thr));
BROOKESIA_DESCRIBE_STRUCT(PicoDet::Box, (), (category, label, score, box));
BROOKESIA_DESCRIBE_STRUCT(PicoDet::Info, (), (opened, width, height, labels));
BROOKESIA_DESCRIBE_STRUCT(
    PicoDet::AttachConfig, (),
    (frame_source, frame_event, display_output, detect_every_n_frames)
);

} // namespace esp_brookesia::service::helper
