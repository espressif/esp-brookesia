/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_picodet/detector.hpp"

#include <fstream>
#include <sstream>

#include "boost/json.hpp"
#if !BROOKESIA_SERVICE_PICODET_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

namespace esp_brookesia::service::picodet {

namespace {

namespace json = boost::json;

std::string join_path(std::string_view dir, std::string_view name)
{
    std::string out(dir);
    if (!out.empty() && out.back() != '/') {
        out.push_back('/');
    }
    out.append(name);
    return out;
}

std::expected<std::string, std::string> read_file(const std::string &path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return std::unexpected("Cannot open manifest: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

const json::value *find(const json::object &obj, std::string_view key)
{
    auto it = obj.find(key);
    return (it == obj.end()) ? nullptr : &it->value();
}

template <typename T>
bool read_number(const json::object &obj, std::string_view key, T &out)
{
    const json::value *v = find(obj, key);
    if (v == nullptr || !v->is_number()) {
        return false;
    }
    out = static_cast<T>(v->to_number<double>());
    return true;
}

template <std::size_t N, typename T>
bool read_array(const json::value &v, std::array<T, N> &out)
{
    if (!v.is_array()) {
        return false;
    }
    const json::array &arr = v.as_array();
    if (arr.size() != N) {
        return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
        if (!arr[i].is_number()) {
            return false;
        }
        out[i] = static_cast<T>(arr[i].to_number<double>());
    }
    return true;
}

} // namespace

std::expected<PicoDetConfig, std::string> PicoDetDetector::load_manifest(std::string_view model_dir)
{
    const std::string manifest_path = join_path(model_dir, "manifest.json");
    auto text = read_file(manifest_path);
    if (!text) {
        return std::unexpected(text.error());
    }

    json::value root;
    try {
        root = json::parse(text.value());
    } catch (const std::exception &e) {
        return std::unexpected(std::string("Invalid manifest JSON: ") + e.what());
    }
    if (!root.is_object()) {
        return std::unexpected(std::string("Manifest root is not a JSON object"));
    }
    const json::object &obj = root.as_object();

    PicoDetConfig config;

    const json::value *model_file = find(obj, "model_file");
    if (model_file == nullptr || !model_file->is_string()) {
        return std::unexpected(std::string("Manifest missing string field 'model_file'"));
    }
    config.model_path = join_path(model_dir, model_file->as_string().c_str());

    const json::value *input = find(obj, "input");
    if (input == nullptr || !input->is_object()) {
        return std::unexpected(std::string("Manifest missing object field 'input'"));
    }
    if (!read_number(input->as_object(), "width", config.input_width) ||
            !read_number(input->as_object(), "height", config.input_height) ||
            config.input_width <= 0 || config.input_height <= 0) {
        return std::unexpected(std::string("Manifest 'input.width'/'input.height' are invalid"));
    }

    if (const json::value *pre = find(obj, "preprocess"); pre != nullptr && pre->is_object()) {
        const json::object &p = pre->as_object();
        if (const json::value *m = find(p, "mean"); m != nullptr && !read_array(*m, config.mean)) {
            return std::unexpected(std::string("Manifest 'preprocess.mean' must be 3 numbers"));
        }
        if (const json::value *s = find(p, "std"); s != nullptr && !read_array(*s, config.std)) {
            return std::unexpected(std::string("Manifest 'preprocess.std' must be 3 numbers"));
        }
        if (const json::value *rs = find(p, "rgb_swap"); rs != nullptr && rs->is_bool()) {
            config.rgb_swap = rs->as_bool();
        }
        if (const json::value *lb = find(p, "letterbox_color");
                lb != nullptr && !read_array(*lb, config.letterbox_color)) {
            return std::unexpected(std::string("Manifest 'preprocess.letterbox_color' must be 3 numbers"));
        }
    }

    const json::value *post = find(obj, "postprocess");
    if (post == nullptr || !post->is_object()) {
        return std::unexpected(std::string("Manifest missing object field 'postprocess'"));
    }
    const json::object &pp = post->as_object();
    read_number(pp, "score_thr", config.score_thr);
    read_number(pp, "nms_thr", config.nms_thr);
    read_number(pp, "top_k", config.top_k);

    const json::value *strides = find(pp, "strides");
    if (strides == nullptr || !strides->is_array() || strides->as_array().empty()) {
        return std::unexpected(std::string("Manifest 'postprocess.strides' must be a non-empty array"));
    }
    for (const json::value &stage : strides->as_array()) {
        std::array<int, 4> s{};
        if (!read_array(stage, s)) {
            return std::unexpected(
                       std::string("Each 'postprocess.strides' entry must be [stride_y, stride_x, offset_y, offset_x]")
                   );
        }
        config.strides.push_back(s);
    }

    if (const json::value *labels = find(obj, "labels"); labels != nullptr && labels->is_array()) {
        for (const json::value &l : labels->as_array()) {
            if (l.is_string()) {
                config.labels.emplace_back(l.as_string().c_str());
            }
        }
    }

    return config;
}

} // namespace esp_brookesia::service::picodet
