#!/bin/bash

# Prepare Boost.JSON headers for docs helper-schema export.
# This script is intended to be sourced by GitLab CI jobs.

set -e

find_boost_json_header() {
    local header_path=""
    if [ -f /usr/include/boost/json.hpp ]; then
        header_path="/usr/include/boost/json.hpp"
    elif [ -f /usr/local/include/boost/json.hpp ]; then
        header_path="/usr/local/include/boost/json.hpp"
    else
        header_path="$(find /usr/local/include /usr/include -path '*/boost/json.hpp' -print -quit 2>/dev/null || true)"
    fi

    if [ -n "${header_path}" ]; then
        echo "${header_path}"
        return 0
    fi
    return 1
}

add_boost_include_root() {
    local header_path="$1"
    local include_root="${header_path%/boost/json.hpp}"
    if [ -z "${include_root}" ]; then
        return
    fi

    case ":${CPLUS_INCLUDE_PATH:-}:" in
        *:"${include_root}":*) ;;
        *) export CPLUS_INCLUDE_PATH="${include_root}${CPLUS_INCLUDE_PATH:+:${CPLUS_INCLUDE_PATH}}" ;;
    esac
}

install_boost_json_dev() {
    if ! command -v apt-get >/dev/null 2>&1; then
        return
    fi

    export DEBIAN_FRONTEND=noninteractive
    apt-get update

    local boost_json_pkg=""
    if apt-cache show '^libboost-json-dev$' >/dev/null 2>&1; then
        boost_json_pkg='^libboost-json-dev$'
    else
        boost_json_pkg="$(apt-cache search --names-only '^libboost-json[0-9.]*-dev$' | awk 'NR==1{print $1}')"
    fi

    if [ -n "${boost_json_pkg}" ]; then
        apt-get install -y --no-install-recommends "${boost_json_pkg}"
        return
    fi

    # Fallback: install the latest available Boost meta-dev package from apt cache.
    local boost_meta_pkg=""
    boost_meta_pkg="$(apt-cache search --names-only '^libboost1\.[0-9]+-dev$' | awk '{print $1}' | sort -Vr | head -n1)"
    if [ -n "${boost_meta_pkg}" ]; then
        apt-get install -y --no-install-recommends "${boost_meta_pkg}"
    fi
}

install_boost_headers_from_source() {
    local boost_version="${BOOST_HEADERS_VERSION:-1.87.0}"
    local boost_tag="${boost_version//./_}"
    local tmp_dir
    tmp_dir="$(mktemp -d)"
    local archive_path="${tmp_dir}/boost.tar"
    local extracted_dir=""

    local urls=(
        "https://archives.boost.io/release/${boost_version}/source/boost_${boost_tag}.tar.bz2"
        "https://archives.boost.io/release/${boost_version}/source/boost_${boost_tag}.tar.gz"
        "https://boostorg.jfrog.io/artifactory/main/release/${boost_version}/source/boost_${boost_tag}.tar.bz2"
    )

    if ! command -v curl >/dev/null 2>&1 && command -v apt-get >/dev/null 2>&1; then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends curl
    fi

    local downloaded=0
    local url
    for url in "${urls[@]}"; do
        if curl -fsSL -o "${archive_path}" "${url}"; then
            downloaded=1
            break
        fi
    done

    if [ "${downloaded}" -eq 1 ]; then
        tar -xf "${archive_path}" -C "${tmp_dir}"
        extracted_dir="$(find "${tmp_dir}" -maxdepth 1 -type d -name 'boost_*' -print -quit || true)"
        if [ -n "${extracted_dir}" ] && [ -d "${extracted_dir}/boost" ]; then
            mkdir -p /usr/local/include
            cp -a "${extracted_dir}/boost" /usr/local/include/
        fi
    fi

    rm -rf "${tmp_dir}"
}

if ! find_boost_json_header >/dev/null 2>&1; then
    install_boost_json_dev
fi

if ! find_boost_json_header >/dev/null 2>&1; then
    install_boost_headers_from_source
fi

boost_json_header="$(find_boost_json_header || true)"
if [ -n "${boost_json_header}" ]; then
    add_boost_include_root "${boost_json_header}"
fi

if [ -z "${boost_json_header}" ]; then
    echo "Missing Boost.JSON headers (boost/json.hpp)." >&2
    echo "Please install a Boost version that provides Boost.JSON headers." >&2
    exit 1
fi
