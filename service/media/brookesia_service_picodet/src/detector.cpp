/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_picodet/detector.hpp"

#include <utility>

#if !BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE
#include <cstdio>

#include "esp_heap_caps.h"
#include "dl_detect_base.hpp"
#include "dl_detect_define.hpp"
#include "dl_detect_espdet_postprocessor.hpp"
#include "dl_image_define.hpp"
#include "dl_image_preprocessor.hpp"
#include "dl_model_base.hpp"
#include "fbs_model.hpp"
#endif

namespace esp_brookesia::service::picodet {

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE

namespace {

std::expected<dl::image::pix_type_t, std::string> to_pix_type(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RGB888:   return dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    case PixelFormat::BGR888:   return dl::image::DL_IMAGE_PIX_TYPE_BGR888;
    case PixelFormat::RGB565LE: return dl::image::DL_IMAGE_PIX_TYPE_RGB565LE;
    case PixelFormat::RGB565BE: return dl::image::DL_IMAGE_PIX_TYPE_RGB565BE;
    case PixelFormat::BGR565LE: return dl::image::DL_IMAGE_PIX_TYPE_BGR565LE;
    case PixelFormat::BGR565BE: return dl::image::DL_IMAGE_PIX_TYPE_BGR565BE;
    case PixelFormat::GRAY:     return dl::image::DL_IMAGE_PIX_TYPE_GRAY;
    case PixelFormat::YUYV:     return dl::image::DL_IMAGE_PIX_TYPE_YUYV;
    case PixelFormat::UYVY:     return dl::image::DL_IMAGE_PIX_TYPE_UYVY;
    default:                    return std::unexpected(std::string("Unsupported input pixel format"));
    }
}

// Read the whole model into a 16-byte-aligned buffer owned by the detector.
std::expected<uint8_t *, std::string> read_model_file(const std::string &path, size_t &out_size)
{
    FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return std::unexpected("Cannot open model file: " + path);
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return std::unexpected("Model file is empty: " + path);
    }
    std::fseek(file, 0, SEEK_SET);

    const size_t aligned_size = (static_cast<size_t>(size) + 15U) & ~static_cast<size_t>(15U);
    auto *data = static_cast<uint8_t *>(
                     heap_caps_aligned_alloc(16, aligned_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                 );
    if (data == nullptr) {
        data = static_cast<uint8_t *>(heap_caps_aligned_alloc(16, aligned_size, MALLOC_CAP_DEFAULT));
    }
    if (data == nullptr) {
        std::fclose(file);
        std::string error = "Failed to allocate " + std::to_string(aligned_size) + " bytes for model: " + path;
        return std::unexpected(std::move(error));
    }

    const size_t read_size = std::fread(data, 1, static_cast<size_t>(size), file);
    std::fclose(file);
    if (read_size != static_cast<size_t>(size)) {
        heap_caps_free(data);
        return std::unexpected("Short read on model file: " + path);
    }

    out_size = static_cast<size_t>(size);
    return data;
}

class EspDetPipeline : public dl::detect::DetectImpl {
public:
    EspDetPipeline(const PicoDetConfig &config, dl::Model *model)
    {
        m_model = model;
        m_model->minimize();

        m_image_preprocessor =
            new dl::image::ImagePreprocessor(m_model, config.mean, config.std, config.rgb_swap);
        m_image_preprocessor->enable_letterbox(config.letterbox_color);

        std::vector<dl::detect::anchor_point_stage_t> stages;
        stages.reserve(config.strides.size());
        for (const auto &s : config.strides) {
            stages.push_back({s[0], s[1], s[2], s[3]});
        }

        m_postprocessor = new dl::detect::ESPDetPostProcessor(
            m_model, m_image_preprocessor, config.score_thr, config.nms_thr, config.top_k, stages
        );
    }
};

} // namespace

struct PicoDetDetector::Impl {
    std::unique_ptr<EspDetPipeline> pipeline;
    uint8_t *model_data = nullptr;
    size_t model_size = 0;

    void release()
    {
        pipeline.reset();
        if (model_data != nullptr) {
            heap_caps_free(model_data);
            model_data = nullptr;
            model_size = 0;
        }
    }

    ~Impl()
    {
        release();
    }
};

#else // !BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE

struct PicoDetDetector::Impl {};

#endif

PicoDetDetector::PicoDetDetector(PicoDetConfig config, bool lazy_load)
    : config_(std::move(config))
    , impl_(std::make_unique<Impl>())
{
    if (!lazy_load) {
        (void)load();
    }
}

PicoDetDetector::~PicoDetDetector() = default;

#if BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE

std::expected<void, std::string> PicoDetDetector::load()
{
    if (impl_->pipeline) {
        return {};
    }

    if (config_.model_path.empty()) {
        return std::unexpected(std::string("Model path is empty"));
    }
    if ((config_.input_width <= 0) || (config_.input_height <= 0)) {
        return std::unexpected(std::string("Model input size is invalid"));
    }
    if (config_.strides.empty()) {
        return std::unexpected(std::string("Model strides are empty"));
    }

    auto data = read_model_file(config_.model_path, impl_->model_size);
    if (!data) {
        return std::unexpected(data.error());
    }
    impl_->model_data = data.value();

    try {
        auto *model = new dl::Model(
            reinterpret_cast<const char *>(impl_->model_data), fbs::MODEL_LOCATION_IN_FLASH_RODATA
        );
        if (model->get_fbs_model() == nullptr) {
            delete model;
            impl_->release();
            return std::unexpected("Failed to parse model (invalid .espdl?): " + config_.model_path);
        }
        impl_->pipeline = std::make_unique<EspDetPipeline>(config_, model);
    } catch (const std::exception &e) {
        impl_->release();
        return std::unexpected(std::string("Failed to load model: ") + e.what());
    }
    return {};
}

void PicoDetDetector::unload()
{
    impl_->release();
}

bool PicoDetDetector::is_loaded() const
{
    return impl_->pipeline != nullptr;
}

std::expected<std::vector<Box>, std::string> PicoDetDetector::detect(const Image &image)
{
    if (!impl_->pipeline) {
        auto loaded = load();
        if (!loaded) {
            return std::unexpected(loaded.error());
        }
    }

    if ((image.data == nullptr) || (image.width == 0) || (image.height == 0)) {
        return std::unexpected(std::string("Invalid input image"));
    }

    auto pix_type = to_pix_type(image.format);
    if (!pix_type) {
        return std::unexpected(pix_type.error());
    }

    dl::image::img_t img{};
    img.data = const_cast<void *>(image.data);
    img.width = image.width;
    img.height = image.height;
    img.pix_type = pix_type.value();

    std::vector<Box> results;
    try {
        auto &raw = impl_->pipeline->run(img);
        results.reserve(raw.size());
        for (const auto &r : raw) {
            Box b;
            b.category = r.category;
            b.score = r.score;
            b.box = {
                r.box.size() > 0 ? r.box[0] : 0,
                r.box.size() > 1 ? r.box[1] : 0,
                r.box.size() > 2 ? r.box[2] : 0,
                r.box.size() > 3 ? r.box[3] : 0,
            };
            results.push_back(std::move(b));
        }
    } catch (const std::exception &e) {
        return std::unexpected(std::string("Inference failed: ") + e.what());
    }
    return results;
}

void PicoDetDetector::set_score_threshold(float score_thr)
{
    config_.score_thr = score_thr;
    if (impl_->pipeline) {
        impl_->pipeline->set_score_thr(score_thr);
    }
}

void PicoDetDetector::set_nms_threshold(float nms_thr)
{
    config_.nms_thr = nms_thr;
    if (impl_->pipeline) {
        impl_->pipeline->set_nms_thr(nms_thr);
    }
}

#else // !BROOKESIA_SERVICE_PICODET_ENABLE_INFERENCE

namespace {
constexpr const char *UNSUPPORTED = "picodet inference is not supported on this platform";
} // namespace

std::expected<void, std::string> PicoDetDetector::load()
{
    return std::unexpected(std::string(UNSUPPORTED));
}

void PicoDetDetector::unload() {}

bool PicoDetDetector::is_loaded() const
{
    return false;
}

std::expected<std::vector<Box>, std::string> PicoDetDetector::detect(const Image &)
{
    return std::unexpected(std::string(UNSUPPORTED));
}

void PicoDetDetector::set_score_threshold(float score_thr)
{
    config_.score_thr = score_thr;
}

void PicoDetDetector::set_nms_threshold(float nms_thr)
{
    config_.nms_thr = nms_thr;
}

#endif

} // namespace esp_brookesia::service::picodet
