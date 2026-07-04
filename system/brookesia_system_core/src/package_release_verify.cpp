/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/package.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"

#include "zlib.h"

#if defined(ESP_PLATFORM) && ESP_PLATFORM && defined(BROOKESIA_SYSTEM_CORE_HAVE_PACKAGE_RELEASE_VERIFY) && \
    BROOKESIA_SYSTEM_CORE_HAVE_PACKAGE_RELEASE_VERIFY
#define BROOKESIA_SYSTEM_CORE_IMPL_PACKAGE_RELEASE_VERIFY 1
extern "C" {
#include "mbedtls/error.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
}
#else
#define BROOKESIA_SYSTEM_CORE_IMPL_PACKAGE_RELEASE_VERIFY 0
#endif

namespace esp_brookesia::system::core {
#if BROOKESIA_SYSTEM_CORE_IMPL_PACKAGE_RELEASE_VERIFY
namespace detail {

namespace fs = std::filesystem;

constexpr const char *DIGEST_SCHEMA_META_INF = "esp-brookesia-meta-inf-v1";
constexpr const char *LEGACY_DIGEST_SCHEMA_DETACHED = "esp-brookesia-detached-v1";
constexpr const char *META_INF_HASH_JSON = "META-INF/hash.json";
constexpr const char *META_INF_SIGNATURE = "META-INF/signature.sig";

constexpr uint32_t ZIP_EOCD_SIGNATURE = 0x06054b50U;
constexpr uint32_t ZIP_CENTRAL_FILE_SIGNATURE = 0x02014b50U;
constexpr uint32_t ZIP_LOCAL_FILE_SIGNATURE = 0x04034b50U;
constexpr uint16_t ZIP_METHOD_STORE = 0;
constexpr uint16_t ZIP_METHOD_DEFLATE = 8;

struct ZipEntry {
    std::string name;
    uint16_t compression_method = 0;
    uint16_t flags = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint32_t local_header_offset = 0;
    bool is_directory = false;
};

std::expected<std::vector<uint8_t>, std::string> read_binary_file(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Failed to open file: " + path.string());
    }
    file.seekg(0, std::ios::end);
    const std::streamoff end_pos = file.tellg();
    if (end_pos < 0) {
        return std::unexpected("Failed to get file size: " + path.string());
    }
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(end_pos));
    if (!data.empty()) {
        file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!file) {
            return std::unexpected("Failed to read file: " + path.string());
        }
    }
    return data;
}

bool check_range(size_t offset, size_t size, size_t total_size)
{
    return (offset <= total_size) && (size <= (total_size - offset));
}

uint16_t read_u16_le(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8U);
}

uint32_t read_u32_le(const std::vector<uint8_t> &data, size_t offset)
{
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8U) |
           (static_cast<uint32_t>(data[offset + 2]) << 16U) |
           (static_cast<uint32_t>(data[offset + 3]) << 24U);
}

std::optional<size_t> find_eocd_offset(const std::vector<uint8_t> &zip_data)
{
    if (zip_data.size() < 22) {
        return std::nullopt;
    }
    const size_t min_offset = zip_data.size() > (22 + 0xFFFF) ? (zip_data.size() - (22 + 0xFFFF)) : 0;
    for (size_t offset = zip_data.size() - 22; offset >= min_offset; --offset) {
        if (!check_range(offset, 4, zip_data.size())) {
            return std::nullopt;
        }
        if (read_u32_le(zip_data, offset) == ZIP_EOCD_SIGNATURE) {
            return offset;
        }
        if (offset == 0) {
            break;
        }
    }
    return std::nullopt;
}

std::expected<std::vector<ZipEntry>, std::string> parse_central_directory(const std::vector<uint8_t> &zip_data)
{
    auto eocd_offset = find_eocd_offset(zip_data);
    if (!eocd_offset) {
        return std::unexpected("ZIP end of central directory not found");
    }
    const size_t eocd = eocd_offset.value();
    if (!check_range(eocd, 22, zip_data.size())) {
        return std::unexpected("Invalid ZIP EOCD size");
    }

    const uint16_t total_entries = read_u16_le(zip_data, eocd + 10);
    const uint32_t central_dir_size = read_u32_le(zip_data, eocd + 12);
    const uint32_t central_dir_offset = read_u32_le(zip_data, eocd + 16);

    if (!check_range(static_cast<size_t>(central_dir_offset), static_cast<size_t>(central_dir_size), zip_data.size())) {
        return std::unexpected("Invalid ZIP central directory range");
    }

    size_t cursor = static_cast<size_t>(central_dir_offset);
    std::vector<ZipEntry> entries;
    entries.reserve(total_entries);

    for (uint16_t index = 0; index < total_entries; ++index) {
        if (!check_range(cursor, 46, zip_data.size())) {
            return std::unexpected("Invalid ZIP central directory header");
        }
        if (read_u32_le(zip_data, cursor) != ZIP_CENTRAL_FILE_SIGNATURE) {
            return std::unexpected("Invalid ZIP central directory signature");
        }

        ZipEntry entry;
        entry.flags = read_u16_le(zip_data, cursor + 8);
        entry.compression_method = read_u16_le(zip_data, cursor + 10);
        entry.compressed_size = read_u32_le(zip_data, cursor + 20);
        entry.uncompressed_size = read_u32_le(zip_data, cursor + 24);
        const uint16_t file_name_len = read_u16_le(zip_data, cursor + 28);
        const uint16_t extra_len = read_u16_le(zip_data, cursor + 30);
        const uint16_t comment_len = read_u16_le(zip_data, cursor + 32);
        entry.local_header_offset = read_u32_le(zip_data, cursor + 42);

        const size_t file_name_offset = cursor + 46;
        if (!check_range(file_name_offset, static_cast<size_t>(file_name_len), zip_data.size())) {
            return std::unexpected("Invalid ZIP file name range");
        }
        entry.name.assign(
            reinterpret_cast<const char *>(zip_data.data() + file_name_offset),
            static_cast<size_t>(file_name_len)
        );
        entry.is_directory = !entry.name.empty() && entry.name.back() == '/';

        const size_t advance = 46 + static_cast<size_t>(file_name_len) + static_cast<size_t>(extra_len) +
                               static_cast<size_t>(comment_len);
        if (!check_range(cursor, advance, zip_data.size())) {
            return std::unexpected("Invalid ZIP central directory entry size");
        }
        cursor += advance;
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::expected<std::vector<uint8_t>, std::string> inflate_raw_deflate(
    const uint8_t *compressed_data, size_t compressed_size, size_t uncompressed_size
)
{
    std::vector<uint8_t> output(uncompressed_size);
    z_stream stream{};
    stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(compressed_data));
    stream.avail_in = static_cast<uInt>(compressed_size);
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    int result = inflateInit2(&stream, -MAX_WBITS);
    if (result != Z_OK) {
        return std::unexpected("zlib inflateInit2 failed");
    }

    result = inflate(&stream, Z_FINISH);
    const bool ok = (result == Z_STREAM_END) && (stream.total_out == output.size());
    inflateEnd(&stream);
    if (!ok) {
        return std::unexpected("zlib inflate failed");
    }
    return output;
}

std::expected<std::vector<uint8_t>, std::string> extract_zip_entry(
    const std::vector<uint8_t> &zip_data, const ZipEntry &entry
)
{
    const size_t local_header = static_cast<size_t>(entry.local_header_offset);
    if (!check_range(local_header, 30, zip_data.size())) {
        return std::unexpected("Invalid ZIP local file header range");
    }
    if (read_u32_le(zip_data, local_header) != ZIP_LOCAL_FILE_SIGNATURE) {
        return std::unexpected("Invalid ZIP local file signature");
    }

    const uint16_t local_file_name_len = read_u16_le(zip_data, local_header + 26);
    const uint16_t local_extra_len = read_u16_le(zip_data, local_header + 28);
    const size_t data_offset = local_header + 30 + static_cast<size_t>(local_file_name_len) +
                               static_cast<size_t>(local_extra_len);

    if (!check_range(data_offset, static_cast<size_t>(entry.compressed_size), zip_data.size())) {
        return std::unexpected("Invalid ZIP compressed data range");
    }

    const uint8_t *compressed_data = zip_data.data() + data_offset;
    switch (entry.compression_method) {
    case ZIP_METHOD_STORE: {
        std::vector<uint8_t> output(static_cast<size_t>(entry.compressed_size));
        if (!output.empty()) {
            std::memcpy(output.data(), compressed_data, output.size());
        }
        return output;
    }
    case ZIP_METHOD_DEFLATE:
        return inflate_raw_deflate(
                   compressed_data, static_cast<size_t>(entry.compressed_size), static_cast<size_t>(entry.uncompressed_size)
               );
    default:
        return std::unexpected("Unsupported ZIP compression method: " + std::to_string(entry.compression_method));
    }
}

std::vector<std::string> ordered_member_paths(const std::vector<ZipEntry> &entries, bool exclude_meta_inf)
{
    std::vector<std::string> all;
    for (const auto &e : entries) {
        if (e.is_directory) {
            continue;
        }
        if (!e.name.empty() && e.name.back() == '/') {
            continue;
        }
        if (exclude_meta_inf && (e.name.rfind("META-INF/", 0) == 0)) {
            continue;
        }
        all.push_back(e.name);
    }

    bool has_manifest = false;
    for (const auto &n : all) {
        if (n == "manifest.json") {
            has_manifest = true;
            break;
        }
    }

    std::vector<std::string> rest;
    for (const auto &n : all) {
        if (n != "manifest.json") {
            rest.push_back(n);
        }
    }
    std::sort(rest.begin(), rest.end());

    if (has_manifest) {
        std::vector<std::string> out;
        out.push_back("manifest.json");
        out.insert(out.end(), rest.begin(), rest.end());
        return out;
    }

    std::sort(all.begin(), all.end());
    return all;
}

const ZipEntry *find_zip_entry(const std::vector<ZipEntry> &entries, std::string_view name)
{
    for (const auto &e : entries) {
        if (!e.is_directory && e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

std::expected<std::vector<uint8_t>, std::string> read_zip_member(
    const std::vector<uint8_t> &zip_data, const std::vector<ZipEntry> &entries, std::string_view name
)
{
    const ZipEntry *entry = find_zip_entry(entries, name);
    if (entry == nullptr) {
        return std::unexpected("ZIP member missing: " + std::string(name));
    }
    return extract_zip_entry(zip_data, *entry);
}

void append_json_escaped(std::string &out, std::string_view s)
{
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20U) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
}

std::expected<std::string, std::string> canonical_digest_utf8(const boost::json::object &root)
{
    auto schema_it = root.find("schema");
    auto files_it = root.find("files");
    if ((schema_it == root.end()) || !schema_it->value().is_string()) {
        return std::unexpected("digest manifest: missing or invalid schema");
    }
    if ((files_it == root.end()) || !files_it->value().is_array()) {
        return std::unexpected("digest manifest: missing or invalid files");
    }

    const std::string_view schema = schema_it->value().as_string();
    const auto &files = files_it->value().as_array();

    std::string out;
    out += "{\"files\":[";
    for (size_t i = 0; i < files.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        if (!files[i].is_object()) {
            return std::unexpected("digest manifest: files entry must be object");
        }
        const auto &obj = files[i].as_object();
        auto path_it = obj.find("path");
        auto sha_it = obj.find("sha256");
        if ((path_it == obj.end()) || !path_it->value().is_string()) {
            return std::unexpected("digest manifest: file entry needs path");
        }
        if ((sha_it == obj.end()) || !sha_it->value().is_string()) {
            return std::unexpected("digest manifest: file entry needs sha256");
        }
        out += "{\"path\":";
        append_json_escaped(out, std::string_view(path_it->value().as_string()));
        out += ",\"sha256\":";
        append_json_escaped(out, std::string_view(sha_it->value().as_string()));
        out += '}';
    }
    out += "],\"schema\":";
    append_json_escaped(out, schema);
    out += '}';
    return out;
}

std::expected<std::array<unsigned char, 32>, std::string> sha256_bytes(const uint8_t *data, size_t size)
{
    std::array<unsigned char, 32> hash {};
    const auto *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == nullptr) {
        return std::unexpected("SHA-256 digest is unavailable");
    }
    const int rc = mbedtls_md(md_info, data, size, hash.data());
    if (rc != 0) {
        char errbuf[160];
        mbedtls_strerror(rc, errbuf, sizeof(errbuf));
        return std::unexpected(std::string("Failed to calculate SHA-256: ") + errbuf);
    }
    return hash;
}

std::expected<std::string, std::string> sha256_hex(const std::vector<uint8_t> &data)
{
    auto hash = sha256_bytes(data.data(), data.size());
    if (!hash) {
        return std::unexpected(hash.error());
    }
    static const char *hex = "0123456789abcdef";
    std::string out(64, ' ');
    for (int i = 0; i < 32; ++i) {
        out[static_cast<size_t>(i * 2)] = hex[hash->at(static_cast<size_t>(i)) >> 4U];
        out[static_cast<size_t>(i * 2 + 1)] = hex[hash->at(static_cast<size_t>(i)) & 0x0FU];
    }
    return out;
}

std::expected<void, std::string> verify_digest_zip_consistency(
    const std::vector<uint8_t> &zip_data,
    const std::vector<ZipEntry> &entries,
    const std::vector<uint8_t> &digest_raw
)
{
    boost::system::error_code ec;
    const std::string digest_str(digest_raw.begin(), digest_raw.end());
    auto parsed = boost::json::parse(digest_str, ec);
    if (ec || !parsed.is_object()) {
        return std::unexpected("Invalid digest manifest JSON");
    }

    const auto &obj = parsed.as_object();
    auto schema_it = obj.find("schema");
    if ((schema_it == obj.end()) || !schema_it->value().is_string()) {
        return std::unexpected("digest manifest schema must be a string");
    }
    const std::string_view schema = schema_it->value().as_string();
    if (schema == LEGACY_DIGEST_SCHEMA_DETACHED) {
        return std::unexpected(
                   "Digest uses removed detached scheme esp-brookesia-detached-v1; "
                   "re-pack with META-INF (tools/app_pack.py pack|sign --private-key …) only"
               );
    }
    if (schema != DIGEST_SCHEMA_META_INF) {
        return std::unexpected("digest manifest schema not supported (expected esp-brookesia-meta-inf-v1)");
    }

    auto canonical = canonical_digest_utf8(obj);
    if (!canonical) {
        return std::unexpected(canonical.error());
    }
    if (canonical->size() != digest_raw.size()) {
        return std::unexpected("digest manifest is not in canonical form");
    }
    if (std::memcmp(canonical->data(), digest_raw.data(), digest_raw.size()) != 0) {
        return std::unexpected("digest manifest is not in canonical form");
    }

    const std::vector<std::string> names_ordered = ordered_member_paths(entries, true);
    std::set<std::string> names_set(names_ordered.begin(), names_ordered.end());

    auto files_it = obj.find("files");
    if ((files_it == obj.end()) || !files_it->value().is_array()) {
        return std::unexpected("digest manifest 'files' must be an array");
    }
    const auto &files = files_it->value().as_array();
    std::set<std::string> listed;
    for (const auto &item : files) {
        if (!item.is_object()) {
            return std::unexpected("digest entry must be object");
        }
        const auto &item_obj = item.as_object();
        auto p_it = item_obj.find("path");
        if ((p_it == item_obj.end()) || !p_it->value().is_string()) {
            return std::unexpected("digest entry needs path");
        }
        listed.insert(std::string(std::string_view(p_it->value().as_string())));
    }

    if (names_set != listed) {
        return std::unexpected("Package member set does not match digest manifest paths");
    }

    for (const auto &item : files) {
        const auto &item_obj = item.as_object();
        const std::string_view path = item_obj.at("path").as_string();
        const std::string_view expect_hex = item_obj.at("sha256").as_string();
        if (expect_hex.size() != 64U) {
            return std::unexpected("digest sha256 must be 64 hex chars");
        }
        for (char ch : expect_hex) {
            if (!std::isxdigit(static_cast<unsigned char>(ch))) {
                return std::unexpected("digest sha256 must be hex");
            }
        }

        const ZipEntry *ze = find_zip_entry(entries, path);
        if (ze == nullptr) {
            return std::unexpected(std::string("digest references missing ZIP member: ") + std::string(path));
        }
        auto data = extract_zip_entry(zip_data, *ze);
        if (!data) {
            return std::unexpected(data.error());
        }
        auto got_hex = sha256_hex(data.value());
        if (!got_hex) {
            return std::unexpected(got_hex.error());
        }
        std::string expect_lower(expect_hex);
        std::transform(expect_lower.begin(), expect_lower.end(), expect_lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (*got_hex != expect_lower) {
            return std::unexpected(
                       std::string("Content mismatch for ") + std::string(path) + ": package was modified after signing"
                   );
        }
    }

    return {};
}

std::expected<void, std::string> mbedtls_verify_rsa_pss_sha256_over_digest_file(
    const std::vector<uint8_t> &digest_body,
    const std::vector<uint8_t> &signature,
    const fs::path &public_pem_path
)
{
    auto pem_data = read_binary_file(public_pem_path);
    if (!pem_data) {
        return std::unexpected(pem_data.error());
    }
    std::vector<unsigned char> pem_buf(pem_data->begin(), pem_data->end());
    pem_buf.push_back('\0');

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    const int parse_rc = mbedtls_pk_parse_public_key(&pk, pem_buf.data(), pem_buf.size());
    if (parse_rc != 0) {
        char errbuf[160];
        mbedtls_strerror(parse_rc, errbuf, sizeof(errbuf));
        mbedtls_pk_free(&pk);
        return std::unexpected(std::string("Failed to parse public key PEM: ") + errbuf);
    }

    auto hash = sha256_bytes(digest_body.data(), digest_body.size());
    if (!hash) {
        mbedtls_pk_free(&pk);
        return std::unexpected(hash.error());
    }

#if defined(MBEDTLS_PK_USE_PSA_RSA_DATA)
    const int verify_rc = mbedtls_pk_verify_ext(
                              MBEDTLS_PK_SIGALG_RSA_PSS,
                              &pk,
                              MBEDTLS_MD_SHA256,
                              hash->data(),
                              hash->size(),
                              signature.data(),
                              signature.size()
                          );
#else
    mbedtls_pk_rsassa_pss_options pss_opts {};
    pss_opts.mgf1_hash_id = MBEDTLS_MD_SHA256;
    /* OpenSSL `rsa_pss_saltlen:digest` → salt length equals digest length (SHA-256). */
    pss_opts.expected_salt_len =
        static_cast<int>(mbedtls_md_get_size_from_type(MBEDTLS_MD_SHA256));

    const int verify_rc = mbedtls_pk_verify_ext(
                              MBEDTLS_PK_RSASSA_PSS,
                              &pss_opts,
                              &pk,
                              MBEDTLS_MD_SHA256,
                              hash->data(),
                              hash->size(),
                              signature.data(),
                              signature.size()
                          );
#endif
    mbedtls_pk_free(&pk);
    if (verify_rc != 0) {
        char errbuf[160];
        mbedtls_strerror(verify_rc, errbuf, sizeof(errbuf));
        return std::unexpected(std::string("Signature verification failed: ") + errbuf);
    }
    return {};
}

struct EmbeddedPresence {
    bool has_hash = false;
    bool has_sig = false;
};

EmbeddedPresence detect_embedded(const std::vector<ZipEntry> &entries)
{
    EmbeddedPresence p;
    p.has_hash = find_zip_entry(entries, META_INF_HASH_JSON) != nullptr;
    p.has_sig = find_zip_entry(entries, META_INF_SIGNATURE) != nullptr;
    return p;
}

} // namespace detail

std::expected<void, std::string> verify_app_package_release(
    std::string_view package_path,
    const AppPackageReleaseVerifyOptions &options
)
{
    if (options.public_key_pem_path.empty()) {
        return std::unexpected("public_key_pem_path is empty");
    }

    const std::filesystem::path package_file{std::string{package_path}};
    auto zip_data = detail::read_binary_file(package_file);
    if (!zip_data) {
        return std::unexpected(zip_data.error());
    }
    auto entries = detail::parse_central_directory(zip_data.value());
    if (!entries) {
        return std::unexpected(entries.error());
    }

    const detail::EmbeddedPresence emb = detail::detect_embedded(entries.value());
    if (emb.has_hash != emb.has_sig) {
        return std::unexpected(
                   "Incomplete release signing: need both META-INF/hash.json and META-INF/signature.sig inside the .bpk"
               );
    }
    if (!emb.has_hash || !emb.has_sig) {
        return std::unexpected("Package is not signed: missing META-INF/hash.json or META-INF/signature.sig");
    }

    auto hash_bytes = detail::read_zip_member(zip_data.value(), entries.value(), detail::META_INF_HASH_JSON);
    if (!hash_bytes) {
        return std::unexpected(hash_bytes.error());
    }
    auto sig_bytes = detail::read_zip_member(zip_data.value(), entries.value(), detail::META_INF_SIGNATURE);
    if (!sig_bytes) {
        return std::unexpected(sig_bytes.error());
    }
    auto pk_ok = detail::mbedtls_verify_rsa_pss_sha256_over_digest_file(
                     hash_bytes.value(), sig_bytes.value(), std::filesystem::path(options.public_key_pem_path)
                 );
    if (!pk_ok) {
        return std::unexpected(pk_ok.error());
    }
    return detail::verify_digest_zip_consistency(zip_data.value(), entries.value(), hash_bytes.value());
}

#elif defined(ESP_PLATFORM) && ESP_PLATFORM

std::expected<void, std::string> verify_app_package_release(
    std::string_view package_path,
    const AppPackageReleaseVerifyOptions &options
)
{
    (void)package_path;
    (void)options;
    return std::unexpected(
               "Release verify disabled at build (enable CONFIG_BROOKESIA_SYSTEM_CORE_ENABLE_PACKAGE_RELEASE_VERIFY)"
           );
}

#else

std::expected<void, std::string> verify_app_package_release(
    std::string_view package_path,
    const AppPackageReleaseVerifyOptions &options
)
{
    (void)package_path;
    (void)options;
    return std::unexpected(
               "verify_app_package_release is only available in ESP-IDF builds (requires mbedTLS)"
           );
}

#endif

} // namespace esp_brookesia::system::core
