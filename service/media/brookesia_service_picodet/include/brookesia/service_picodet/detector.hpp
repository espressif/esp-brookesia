/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/service_picodet/macro_configs.h"

namespace esp_brookesia::service::picodet {

/**
 * @brief Pixel layout of an input frame handed to the detector.
 */
enum class PixelFormat : uint8_t {
    RGB888,
    BGR888,
    RGB565LE,
    RGB565BE,
    BGR565LE,
    BGR565BE,
    GRAY,
    YUYV,
    UYVY,
    Max,
};

/**
 * @brief Borrowed input frame for one `detect()` call.
 */
struct Image {
    const void *data = nullptr;   ///< Pixel buffer.
    uint16_t    width = 0;        ///< Width in pixels.
    uint16_t    height = 0;       ///< Height in pixels.
    PixelFormat format = PixelFormat::Max; ///< Pixel layout.
};

/**
 * @brief One detected object.
 */
struct Box {
    int   category = 0;   ///< Class index; human name is `PicoDetConfig::labels[category]` when present.
    float score = 0.0f;   ///< Confidence in [0, 1].
    std::array<int, 4> box{};
};

/**
 * @brief All parameters needed to build one PicoDet (ESPDet-Pico) inference pipeline.
 */
struct PicoDetConfig {
    std::string model_path;                        ///< Absolute filesystem path to the .espdl file.

    int input_width = 0;
    int input_height = 0;

    std::array<float, 3>   mean{0.0f, 0.0f, 0.0f};
    std::array<float, 3>   std{255.0f, 255.0f, 255.0f};
    bool                   rgb_swap = false;       ///< true if the model consumes BGR order.
    std::array<uint8_t, 3> letterbox_color{114, 114, 114};

    float score_thr = 0.6f;
    float nms_thr = 0.7f;
    int   top_k = 10;
    std::vector<std::array<int, 4>> strides;

    std::vector<std::string> labels;
};

/**
 * @brief A PicoDet (ESPDet-Pico) object detector wrapping the esp-dl inference pipeline.
 *
 * @note Not thread-safe. `load()`/`unload()`/`detect()` must not run concurrently; the caller
 *       serializes them.
 */
class PicoDetDetector {
public:
    /**
     * @brief Parse `<model_dir>/manifest.json` into a config.
     */
    static std::expected<PicoDetConfig, std::string> load_manifest(std::string_view model_dir);

    /**
     * @brief Construct a detector.
     */
    explicit PicoDetDetector(PicoDetConfig config, bool lazy_load = false);
    ~PicoDetDetector();

    PicoDetDetector(const PicoDetDetector &) = delete;
    PicoDetDetector &operator=(const PicoDetDetector &) = delete;

    /**
     * @brief Load the model and build the pipeline.
     */
    std::expected<void, std::string> load();

    /**
     * @brief Release the model and pipeline, reclaiming PSRAM.
     */
    void unload();

    /**
     * @brief Whether the model is currently loaded and ready.
     */
    bool is_loaded() const;

    /**
     * @brief Run inference on one frame.
     */
    std::expected<std::vector<Box>, std::string> detect(const Image &image);

    /**
     * @brief Access the active configuration (e.g. to resolve labels).
     */
    const PicoDetConfig &config() const
    {
        return config_;
    }

    /**
     * @brief Override detection thresholds on the resident model without reloading.
     */
    void set_score_threshold(float score_thr);
    void set_nms_threshold(float nms_thr);

private:
    struct Impl;

    PicoDetConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::service::picodet
