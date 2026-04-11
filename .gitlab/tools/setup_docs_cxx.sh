#!/bin/bash

# Prepare a modern C++ compiler for docs helper-schema export.
# This script is intended to be sourced by GitLab CI jobs.

set -e

install_modern_gxx() {
    if ! command -v apt-get >/dev/null 2>&1; then
        return
    fi

    export DEBIAN_FRONTEND=noninteractive
    apt-get update

    # On Ubuntu 20.04, g++-12/13 are usually not in the default repos.
    # NOTE: apt treats "g++-12" as a regex if no exact package exists, so use anchored regex with escaped '+'.
    if ! apt-cache show '^g\+\+-13$' >/dev/null 2>&1 && ! apt-cache show '^g\+\+-12$' >/dev/null 2>&1; then
        apt-get install -y --no-install-recommends software-properties-common ca-certificates gnupg
        add-apt-repository -y ppa:ubuntu-toolchain-r/test
        apt-get update
    fi

    if apt-cache show '^g\+\+-13$' >/dev/null 2>&1; then
        # Install exact package name to avoid regex fallback.
        apt-get install -y --no-install-recommends '^g\+\+-13$'
        return
    fi

    if apt-cache show '^g\+\+-12$' >/dev/null 2>&1; then
        # Install exact package name to avoid regex fallback.
        apt-get install -y --no-install-recommends '^g\+\+-12$'
        return
    fi
}

supports_required_standard() {
    local compiler="$1"
    local test_src='int main() { return 0; }'

    if printf '%s\n' "${test_src}" | "${compiler}" -std=c++23 -x c++ - -fsyntax-only >/dev/null 2>&1; then
        return 0
    fi
    if printf '%s\n' "${test_src}" | "${compiler}" -std=c++2b -x c++ - -fsyntax-only >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

if ! command -v g++-13 >/dev/null 2>&1 && ! command -v g++-12 >/dev/null 2>&1; then
    install_modern_gxx
fi

if command -v g++-13 >/dev/null 2>&1; then
    export CXX="$(command -v g++-13)"
elif command -v g++-12 >/dev/null 2>&1; then
    export CXX="$(command -v g++-12)"
elif command -v g++ >/dev/null 2>&1; then
    export CXX="$(command -v g++)"
else
    echo "No C++ compiler found on PATH." >&2
    exit 1
fi

echo "Using CXX=${CXX}"
"${CXX}" --version

if ! supports_required_standard "${CXX}"; then
    echo "Selected compiler does not support -std=c++23/-std=c++2b: ${CXX}" >&2
    echo "Please provide g++-12 or newer (recommended g++-13)." >&2
    exit 1
fi
