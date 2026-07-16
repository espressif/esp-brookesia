/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_picodet/service.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <utility>

#include "boost/thread.hpp"
#include "brookesia/service_picodet/macro_configs.h"
#if !BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/thread_config.hpp"

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
#include "esp_heap_caps.h"
#include "dl_image_bmp.hpp"
#include "dl_image_define.hpp"
#include "dl_image_draw.hpp"
#include "dl_image_jpeg.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_manager/service/manager.hpp"
#endif

namespace esp_brookesia::service {

namespace {

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return s;
}

bool ends_with(const std::string &s, std::string_view suffix)
{
    return (s.size() >= suffix.size()) && (s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
}
#endif

// Filesystem reads can run with flash cache disabled; use an internal-stack thread when needed.
std::expected<void, std::string> run_with_cache_safe_stack(const std::function<void()> &func)
{
    if (lib_utils::ThreadConfig::check_stack_cache_safe()) {
        func();
        return {};
    }

    BROOKESIA_THREAD_CONFIG_GUARD({
        .name = "PicoDetFileIO",
        .stack_size = static_cast<size_t>(BROOKESIA_SERVICE_PICODET_FILE_IO_THREAD_STACK_SIZE),
        .stack_in_ext = false,
    });
    try {
        boost::thread(func).join();
    } catch (const std::exception &e) {
        return std::unexpected(std::string("Failed to run file IO on internal-stack thread: ") + e.what());
    }
    return {};
}

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
const std::vector<uint8_t> PIPELINE_BOX_COLOR_RGB565 = {0x00, 0xF8};
const std::vector<uint8_t> PIPELINE_BOX_COLOR_RGB888 = {0xFF, 0x00, 0x00};
constexpr uint8_t PIPELINE_BOX_LINE_WIDTH = 2;
constexpr uint32_t PIPELINE_PRESENT_TIMEOUT_MS = 100;

struct PipelineFrameFormat {
    picodet::PixelFormat detector_pixel_format;
    Display::PixelFormat display_pixel_format;
    dl::image::pix_type_t draw_pixel_format;
    const std::vector<uint8_t> *box_color;
};

std::optional<Display::PixelFormat> to_display_pixel_format(std::string_view format)
{
    if ((format == "RGB565") || (format == "RGB565LE") || (format == "RGB565_LE")) {
        return Display::PixelFormat::RGB565;
    }
    if (format == "RGB888") {
        return Display::PixelFormat::RGB888;
    }
    return std::nullopt;
}

std::optional<PipelineFrameFormat> to_pipeline_frame_format(Display::PixelFormat format)
{
    switch (format) {
    case Display::PixelFormat::RGB565:
        return PipelineFrameFormat{
            .detector_pixel_format = picodet::PixelFormat::RGB565LE,
            .display_pixel_format = Display::PixelFormat::RGB565,
            .draw_pixel_format = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
            .box_color = &PIPELINE_BOX_COLOR_RGB565,
        };
    case Display::PixelFormat::RGB888:
        return PipelineFrameFormat{
            .detector_pixel_format = picodet::PixelFormat::RGB888,
            .display_pixel_format = Display::PixelFormat::RGB888,
            .draw_pixel_format = dl::image::DL_IMAGE_PIX_TYPE_RGB888,
            .box_color = &PIPELINE_BOX_COLOR_RGB888,
        };
    default:
        return std::nullopt;
    }
}
#endif

} // namespace

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE

// Frame-event subscription plus optional display presentation state.
struct PicoDet::Pipeline {
    static constexpr const char *DISPLAY_SOURCE_NAME = "PicoDet";

    ServiceBinding display_binding;
    uint32_t source_id = 0;
    std::string output_name;
    Display::PixelFormat output_pixel_format = Display::PixelFormat::RGB565;
    uint32_t output_width = 0;
    uint32_t output_height = 0;
    uint32_t display_present_warning_count = 0;

    uint32_t detect_every_n_frames = 1;
    uint32_t frame_count = 0;
    std::vector<picodet::Box> cached_boxes;
    bool warned_bad_format = false;

    // Destroyed first, before display teardown.
    lib_utils::scoped_connection frame_connection;

    ~Pipeline()
    {
        frame_connection.disconnect();
        if (source_id != 0) {
            auto &display_service = Display::get_instance();
            if (!output_name.empty()) {
                (void)display_service.release_output(source_id, output_name);
            }
            (void)display_service.unregister_source(source_id);
            source_id = 0;
        }
        display_binding.release();
    }
};

#else

struct PicoDet::Pipeline {};

#endif

PicoDet::PicoDet()
    : ServiceBase({
    .name = helper::PicoDet::get_name().data(),
    .description = "Run PicoDet object detection on images and attached frame streams.",
    .version = make_version(
        BROOKESIA_SERVICE_PICODET_VER_MAJOR, BROOKESIA_SERVICE_PICODET_VER_MINOR,
        BROOKESIA_SERVICE_PICODET_VER_PATCH
    ),
})
{}

PicoDet::~PicoDet()
{
    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_.reset();
    detector_.reset();
}

bool PicoDet::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_PICODET_VER_MAJOR, BROOKESIA_SERVICE_PICODET_VER_MINOR,
        BROOKESIA_SERVICE_PICODET_VER_PATCH
    );
    return true;
}

void PicoDet::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_.reset();
    detector_.reset();
}

boost::json::object PicoDet::build_info_locked() const
{
    Helper::Info info;
    info.opened = (detector_ != nullptr) && detector_->is_loaded();
    if (detector_ != nullptr) {
        const auto &cfg = detector_->config();
        info.width = cfg.input_width;
        info.height = cfg.input_height;
        info.labels = cfg.labels;
    }
    return BROOKESIA_DESCRIBE_TO_JSON(info).as_object();
}

std::expected<boost::json::object, std::string> PicoDet::function_open(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    std::lock_guard<std::mutex> lock(mutex_);

    if (pipeline_ != nullptr) {
        return std::unexpected(std::string("Pipeline is attached, call Detach before Open"));
    }

    Helper::OpenConfig open_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, open_config)) {
        return std::unexpected(
                   "Failed to parse open config: " + std::string(BROOKESIA_DESCRIBE_TO_STR(config))
               );
    }
    if (open_config.model_dir.empty()) {
        return std::unexpected(std::string("Config requires a non-empty string field 'model_dir'"));
    }

    std::unique_ptr<picodet::PicoDetDetector> detector;
    std::expected<void, std::string> load_result;
    auto io_result = run_with_cache_safe_stack([&]() {
        auto parsed = picodet::PicoDetDetector::load_manifest(open_config.model_dir);
        if (!parsed) {
            load_result = std::unexpected(parsed.error());
            return;
        }
        picodet::PicoDetConfig cfg = std::move(parsed.value());

        if (open_config.score_thr >= 0.0) {
            cfg.score_thr = static_cast<float>(open_config.score_thr);
        }
        if (open_config.nms_thr >= 0.0) {
            cfg.nms_thr = static_cast<float>(open_config.nms_thr);
        }

        detector = std::make_unique<picodet::PicoDetDetector>(std::move(cfg), /*lazy_load=*/true);
        load_result = detector->load();
    });
    if (!io_result) {
        return std::unexpected(io_result.error());
    }
    if (!load_result) {
        return std::unexpected(load_result.error());
    }

    detector_ = std::move(detector);
    BROOKESIA_LOGI("Opened PicoDet model from %1%", open_config.model_dir);
    return build_info_locked();
}

std::expected<boost::json::array, std::string> PicoDet::function_detect(const std::string &image_path)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: image_path(%1%)", image_path);

    std::lock_guard<std::mutex> lock(mutex_);

    if (detector_ == nullptr) {
        return std::unexpected(std::string("Model is not opened, call Open first"));
    }

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
    const std::string lower = to_lower(image_path);
    const bool is_jpeg = ends_with(lower, ".jpg") || ends_with(lower, ".jpeg");
    const bool is_bmp = ends_with(lower, ".bmp");
    if (!is_jpeg && !is_bmp) {
        return std::unexpected(std::string("Unsupported image type (expect .jpg/.jpeg/.bmp): ") + image_path);
    }

    dl::image::img_t img{};
    std::string io_error;
    auto io_result = run_with_cache_safe_stack([&]() {
        if (is_jpeg) {
            dl::image::jpeg_img_t jpeg = dl::image::read_jpeg(image_path.c_str());
            if (jpeg.data == nullptr) {
                io_error = "Failed to read JPEG: " + image_path;
                return;
            }
            img = dl::image::sw_decode_jpeg(jpeg, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
            heap_caps_free(jpeg.data);
        } else {
            img = dl::image::read_bmp(image_path.c_str());
        }
    });
    if (!io_result) {
        return std::unexpected(io_result.error());
    }
    if (!io_error.empty()) {
        return std::unexpected(io_error);
    }
    if (img.data == nullptr) {
        return std::unexpected("Failed to decode image: " + image_path);
    }

    picodet::PixelFormat format = picodet::PixelFormat::Max;
    switch (img.pix_type) {
    case dl::image::DL_IMAGE_PIX_TYPE_RGB888:   format = picodet::PixelFormat::RGB888;   break;
    case dl::image::DL_IMAGE_PIX_TYPE_BGR888:   format = picodet::PixelFormat::BGR888;   break;
    case dl::image::DL_IMAGE_PIX_TYPE_RGB565LE: format = picodet::PixelFormat::RGB565LE; break;
    case dl::image::DL_IMAGE_PIX_TYPE_RGB565BE: format = picodet::PixelFormat::RGB565BE; break;
    case dl::image::DL_IMAGE_PIX_TYPE_BGR565LE: format = picodet::PixelFormat::BGR565LE; break;
    case dl::image::DL_IMAGE_PIX_TYPE_BGR565BE: format = picodet::PixelFormat::BGR565BE; break;
    case dl::image::DL_IMAGE_PIX_TYPE_GRAY:     format = picodet::PixelFormat::GRAY;     break;
    default:                                    format = picodet::PixelFormat::Max;      break;
    }
    if (format == picodet::PixelFormat::Max) {
        heap_caps_free(img.data);
        return std::unexpected(std::string("Decoded image has an unsupported pixel format"));
    }

    picodet::Image image{
        .data = img.data,
        .width = img.width,
        .height = img.height,
        .format = format,
    };

    auto boxes = detector_->detect(image);
    heap_caps_free(img.data);
    if (!boxes) {
        return std::unexpected(boxes.error());
    }

    const auto &labels = detector_->config().labels;
    boost::json::array result;
    for (const auto &b : boxes.value()) {
        helper::PicoDet::Box out;
        out.category = b.category;
        out.label = (b.category >= 0 && static_cast<size_t>(b.category) < labels.size())
                    ? labels[b.category] : std::string{};
        out.score = b.score;
        out.box = {b.box[0], b.box[1], b.box[2], b.box[3]};
        result.push_back(BROOKESIA_DESCRIBE_TO_JSON(out));
    }
    return result;
#else
    (void)image_path;
    return std::unexpected(std::string("picodet inference is not supported on this platform"));
#endif
}

std::expected<void, std::string> PicoDet::function_close()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_.reset();
    detector_.reset();
    return {};
}

std::expected<boost::json::object, std::string> PicoDet::function_get_info()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);
    return build_info_locked();
}

std::expected<void, std::string> PicoDet::function_attach(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: config(%1%)", BROOKESIA_DESCRIBE_TO_STR(config));

    std::lock_guard<std::mutex> lock(mutex_);

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
    if ((detector_ == nullptr) || !detector_->is_loaded()) {
        return std::unexpected(std::string("Model is not opened, call Open first"));
    }
    if (pipeline_ != nullptr) {
        return std::unexpected(std::string("Pipeline is already attached"));
    }

    Helper::AttachConfig attach_config;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(config, attach_config)) {
        return std::unexpected(
                   "Failed to parse attach config: " + std::string(BROOKESIA_DESCRIBE_TO_STR(config))
               );
    }
    if (attach_config.detect_every_n_frames <= 0) {
        return std::unexpected(std::string("'detect_every_n_frames' must be positive"));
    }

    auto pipeline = std::make_unique<Pipeline>();
    pipeline->detect_every_n_frames = static_cast<uint32_t>(attach_config.detect_every_n_frames);

    if (!attach_config.display_output.empty()) {
        auto &display_service = Display::get_instance();
        auto display_binding = ServiceManager::get_instance().bind(Display::Helper::get_name().data());
        if (!display_binding.is_valid()) {
            return std::unexpected(std::string("Failed to bind Display service"));
        }
        pipeline->display_binding = std::move(display_binding);

        const auto outputs = display_service.get_outputs();
        const auto output_it = std::find_if(outputs.begin(), outputs.end(), [&](const auto & output) {
            return output.name == attach_config.display_output;
        });
        if (output_it == outputs.end()) {
            return std::unexpected("Display output is not available: " + attach_config.display_output);
        }
        if (!to_pipeline_frame_format(output_it->pixel_format)) {
            return std::unexpected(
                       "Display output pixel format is not supported: " +
                       std::string(BROOKESIA_DESCRIBE_ENUM_TO_STR(output_it->pixel_format))
                   );
        }

        Display::SourceInfo source_info = {
            .name = Pipeline::DISPLAY_SOURCE_NAME,
            .role = "video",
            .preferred_outputs = {output_it->name},
            .priority = 0,
        };
        auto source_result = display_service.register_source(std::move(source_info));
        if (!source_result) {
            return std::unexpected("Failed to register display source: " + source_result.error());
        }
        pipeline->source_id = source_result.value();
        pipeline->output_name = output_it->name;
        pipeline->output_pixel_format = output_it->pixel_format;
        pipeline->output_width = output_it->width;
        pipeline->output_height = output_it->height;

        auto request_result = display_service.request_output(pipeline->source_id, output_it->name);
        if (!request_result) {
            return std::unexpected("Failed to request display output: " + request_result.error());
        }
        auto active_result = display_service.set_active_source(output_it->name, Pipeline::DISPLAY_SOURCE_NAME);
        if (!active_result) {
            return std::unexpected("Failed to activate display source: " + active_result.error());
        }
    }

    auto frame_source_service = ServiceManager::get_instance().get_service(attach_config.frame_source);
    if (!frame_source_service) {
        return std::unexpected("Frame source service is not available: " + attach_config.frame_source);
    }
    auto *pipeline_ptr = pipeline.get();
    pipeline->frame_connection = frame_source_service->subscribe_event(
                                     attach_config.frame_event,
    [this, pipeline_ptr](const std::string &, const EventItemMap & items) {
        std::unique_lock<std::mutex> frame_lock(mutex_, std::try_to_lock);
        if (!frame_lock.owns_lock() || (pipeline_.get() != pipeline_ptr) || (detector_ == nullptr)) {
            return;
        }
        on_pipeline_frame(*pipeline_ptr, items);
    }
                                 );
    if (!pipeline->frame_connection.connected()) {
        return std::unexpected(
                   "Failed to subscribe to '" + attach_config.frame_event + "' on service '" +
                   attach_config.frame_source + "'"
               );
    }

    pipeline_ = std::move(pipeline);
    BROOKESIA_LOGI(
        "Pipeline attached: display_output='%1%', detect every %2% frame(s)",
        attach_config.display_output, attach_config.detect_every_n_frames
    );
    return {};
#else
    (void)config;
    return std::unexpected(std::string("picodet pipeline is not supported on this platform"));
#endif
}

std::expected<void, std::string> PicoDet::function_detach()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard<std::mutex> lock(mutex_);
    if (pipeline_ == nullptr) {
        return {};
    }
    pipeline_.reset();
    BROOKESIA_LOGI("Pipeline detached");
    return {};
}

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
void PicoDet::on_pipeline_frame(Pipeline &pipeline, const EventItemMap &items)
{
    const auto frame_it = items.find("Frame");
    if (frame_it == items.end()) {
        if (!pipeline.warned_bad_format) {
            pipeline.warned_bad_format = true;
            BROOKESIA_LOGW("Frame event carries no 'Frame' RawBuffer item");
        }
        return;
    }
    const auto *frame_ptr = std::get_if<RawBuffer>(&frame_it->second);
    if ((frame_ptr == nullptr) || (frame_ptr->data_ptr == nullptr) || (frame_ptr->data_size == 0)) {
        return;
    }
    const RawBuffer &frame = *frame_ptr;

    uint32_t width = 0;
    uint32_t height = 0;
    std::optional<PipelineFrameFormat> frame_format;
    const auto info_it = items.find("SinkInfo");
    if (info_it == items.end()) {
        if (!pipeline.warned_bad_format) {
            pipeline.warned_bad_format = true;
            BROOKESIA_LOGW("Frame event carries no 'SinkInfo' object");
        }
        return;
    }
    const auto *info = std::get_if<boost::json::object>(&info_it->second);
    if (info == nullptr) {
        return;
    }
    if (const auto *format = info->if_contains("format"); (format != nullptr) && format->is_string()) {
        const std::string format_name(format->as_string());
        auto display_format = to_display_pixel_format(format_name);
        if (display_format) {
            frame_format = to_pipeline_frame_format(display_format.value());
        }
        if (!frame_format) {
            if (!pipeline.warned_bad_format) {
                pipeline.warned_bad_format = true;
                BROOKESIA_LOGW("Pipeline requires RGB565/RGB888 frames, got %1%", format_name);
            }
            return;
        }
    }
    if (const auto *w = info->if_contains("width"); (w != nullptr) && w->is_number()) {
        width = w->to_number<uint32_t>();
    }
    if (const auto *h = info->if_contains("height"); (h != nullptr) && h->is_number()) {
        height = h->to_number<uint32_t>();
    }
    if ((width == 0) || (height == 0)) {
        if (!pipeline.warned_bad_format) {
            pipeline.warned_bad_format = true;
            BROOKESIA_LOGW("Frame event carries no usable geometry in 'SinkInfo'");
        }
        return;
    }
    if (!frame_format) {
        if (!pipeline.warned_bad_format) {
            pipeline.warned_bad_format = true;
            BROOKESIA_LOGW("Frame event carries no usable pixel format");
        }
        return;
    }

    pipeline.frame_count++;

    if (((pipeline.frame_count % pipeline.detect_every_n_frames) == 1) ||
            (pipeline.detect_every_n_frames == 1)) {
        const picodet::Image image = {
            .data = frame.data_ptr,
            .width = static_cast<uint16_t>(width),
            .height = static_cast<uint16_t>(height),
            .format = frame_format->detector_pixel_format,
        };
        auto boxes = detector_->detect(image);
        if (boxes) {
            pipeline.cached_boxes = std::move(boxes.value());
            publish_detection_boxes(pipeline.cached_boxes);
        }
    }

    if (pipeline.source_id == 0) {
        return;
    }
    if (pipeline.output_pixel_format != frame_format->display_pixel_format) {
        if (!pipeline.warned_bad_format) {
            pipeline.warned_bad_format = true;
            BROOKESIA_LOGW(
                "Frame format does not match display output: frame=%1%, output=%2%",
                BROOKESIA_DESCRIBE_ENUM_TO_STR(frame_format->display_pixel_format),
                BROOKESIA_DESCRIBE_ENUM_TO_STR(pipeline.output_pixel_format)
            );
        }
        return;
    }

    auto *pixels = const_cast<uint8_t *>(frame.data_ptr);
    if (!pipeline.cached_boxes.empty()) {
        dl::image::img_t img = {
            .data = pixels,
            .width = static_cast<uint16_t>(width),
            .height = static_cast<uint16_t>(height),
            .pix_type = frame_format->draw_pixel_format,
        };
        for (const auto &box : pipeline.cached_boxes) {
            dl::image::draw_hollow_rectangle(
                img, box.box[0], box.box[1], box.box[2], box.box[3],
                *frame_format->box_color, PIPELINE_BOX_LINE_WIDTH
            );
        }
    }

    if ((pipeline.output_width < width) || (pipeline.output_height < height)) {
        return;
    }
    const Display::FrameInfo display_frame = {
        .x = (pipeline.output_width - width) / 2,
        .y = (pipeline.output_height - height) / 2,
        .width = width,
        .height = height,
        .pixel_format = frame_format->display_pixel_format,
    };
    const auto present_result = Display::get_instance().present_frame_sync(
                                    pipeline.source_id, pipeline.output_name, display_frame,
                                    RawBuffer(frame.data_ptr, frame.data_size), PIPELINE_PRESENT_TIMEOUT_MS
                                );
    if ((present_result != Display::PresentResult::Presented) &&
            (present_result != Display::PresentResult::DroppedNotActive) &&
            ((pipeline.display_present_warning_count++ % 30) == 0)) {
        BROOKESIA_LOGW(
            "Failed to present PicoDet frame to Display: %1%",
            BROOKESIA_DESCRIBE_ENUM_TO_STR(present_result)
        );
    }
}
#endif

void PicoDet::publish_detection_boxes(const std::vector<picodet::Box> &boxes)
{
#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
    const auto &labels = detector_->config().labels;
    boost::json::array array;
    for (const auto &b : boxes) {
        Helper::Box out;
        out.category = b.category;
        out.label = (b.category >= 0 && static_cast<size_t>(b.category) < labels.size())
                    ? labels[b.category] : std::string{};
        out.score = b.score;
        out.box = {b.box[0], b.box[1], b.box[2], b.box[3]};
        array.push_back(BROOKESIA_DESCRIBE_TO_JSON(out));
    }
    publish_event(
        BROOKESIA_DESCRIBE_ENUM_TO_STR(Helper::EventId::DetectionUpdated),
        std::vector<EventItem> {std::move(array)}
    );
#else
    (void)boxes;
#endif
}

BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(
    ServiceBase, PicoDet, helper::PicoDet::get_name().data(),
    BROOKESIA_SERVICE_PICODET_PLUGIN_SYMBOL
);

} // namespace esp_brookesia::service
