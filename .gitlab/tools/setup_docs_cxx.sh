#!/bin/bash

# Prepare a modern C++ compiler for docs helper-schema export.
# This script is intended to be sourced by GitLab CI jobs.

set -e
set -o pipefail

apt_update() {
    apt-get -o Acquire::Retries=3 update
}

apt_install() {
    apt-get -o Acquire::Retries=3 install -y --no-install-recommends "$@"
}

install_modern_gxx() {
    if ! command -v apt-get >/dev/null 2>&1; then
        return 1
    fi

    export DEBIAN_FRONTEND=noninteractive
    apt_update || return 1

    # On Ubuntu 20.04, g++-12/13 are usually not in the default repos.
    # NOTE: apt treats "g++-12" as a regex if no exact package exists, so use anchored regex with escaped '+'.
    if ! apt-cache show '^g\+\+-13$' >/dev/null 2>&1 && ! apt-cache show '^g\+\+-12$' >/dev/null 2>&1; then
        apt_install software-properties-common ca-certificates gnupg || return 1
        add-apt-repository -y ppa:ubuntu-toolchain-r/test || return 1
        apt_update || return 1
    fi

    if apt-cache show '^g\+\+-13$' >/dev/null 2>&1; then
        # Install exact package name to avoid regex fallback.
        apt_install '^g\+\+-13$' || return 1
        return
    fi

    if apt-cache show '^g\+\+-12$' >/dev/null 2>&1; then
        # Install exact package name to avoid regex fallback.
        apt_install '^g\+\+-12$' || return 1
        return
    fi

    return 1
}

install_llvm_clang_libcxx() {
    if ! command -v apt-get >/dev/null 2>&1; then
        return 1
    fi

    export DEBIAN_FRONTEND=noninteractive
    apt_install ca-certificates curl gnupg || return 1

    local llvm_keyring="/etc/apt/keyrings/apt.llvm.org.gpg"
    mkdir -p /etc/apt/keyrings
    rm -f "${llvm_keyring}"
    curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor --yes -o "${llvm_keyring}"
    echo "deb [signed-by=/etc/apt/keyrings/apt.llvm.org.gpg] http://apt.llvm.org/focal/ llvm-toolchain-focal-17 main" \
        > /etc/apt/sources.list.d/llvm-toolchain-focal-17.list

    apt_update || return 1
    apt_install clang-17 libc++-17-dev libc++abi-17-dev || return 1

    cat > /tmp/docs-clangxx-17-libcxx <<'EOF'
#!/bin/sh
exec /usr/bin/clang++-17 -stdlib=libc++ "$@"
EOF
    chmod +x /tmp/docs-clangxx-17-libcxx
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

if ! command -v g++-13 >/dev/null 2>&1 && ! command -v g++-12 >/dev/null 2>&1 \
    && [ ! -x /tmp/docs-clangxx-17-libcxx ]; then
    install_modern_gxx || install_llvm_clang_libcxx || true
fi

if [ -n "${CXX:-}" ]; then
    export CXX
elif command -v g++-13 >/dev/null 2>&1; then
    export CXX="$(command -v g++-13)"
elif command -v g++-12 >/dev/null 2>&1; then
    export CXX="$(command -v g++-12)"
elif [ -x /tmp/docs-clangxx-17-libcxx ]; then
    export CXX="/tmp/docs-clangxx-17-libcxx"
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
