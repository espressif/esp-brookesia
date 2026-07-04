#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: install_linux_deps.sh [--minimal|--full|--media|--video|--display|--network|--dry-run]

Options:
  --minimal   Install build, Boost, and OpenSSL dependencies only.
  --full      Install all known Linux HAL dependencies, including the proot launcher helper. This is the default.
  --media     Include FFmpeg, PortAudio, and V4L2 development packages.
  --video     Include FFmpeg and V4L2 development packages.
  --display   Include SDL2 development packages.
  --network   Include NetworkManager and UPower development packages.
  --dry-run   Print the install command without running it.
  -h, --help  Show this help text.
EOF
}

profile="full"
dry_run=0
for arg in "$@"; do
    case "$arg" in
        --minimal)
            profile="minimal"
            ;;
        --full)
            profile="full"
            ;;
        --media)
            profile="media"
            ;;
        --video)
            profile="video"
            ;;
        --display)
            profile="display"
            ;;
        --network)
            profile="network"
            ;;
        --dry-run)
            dry_run=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            usage >&2
            exit 1
            ;;
    esac
done

detect_pm() {
    if command -v apt-get >/dev/null 2>&1; then
        echo apt
    elif command -v dnf >/dev/null 2>&1; then
        echo dnf
    elif command -v pacman >/dev/null 2>&1; then
        echo pacman
    else
        echo unsupported
    fi
}

join_by_space() {
    printf '%s ' "$@"
}

run_or_print() {
    if [ "$dry_run" -eq 1 ]; then
        printf '%q ' "$@"
        echo
        return
    fi
    "$@"
}

pm="$(detect_pm)"

case "$pm" in
    apt)
        base=(build-essential cmake ninja-build pkg-config libboost-system-dev libssl-dev)
        runtime=(proot)
        media=(ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev libavdevice-dev portaudio19-dev libv4l-dev)
        video=(ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev libv4l-dev)
        display=(libsdl2-dev)
        network=(libnm-dev libupower-glib-dev)
        update_cmd=(sudo apt-get update)
        install_cmd=(sudo apt-get install -y)
        ;;
    dnf)
        base=(gcc-c++ cmake ninja-build pkgconf-pkg-config boost-devel openssl-devel)
        runtime=(proot)
        media=(ffmpeg ffmpeg-devel portaudio-devel libv4l-devel)
        video=(ffmpeg ffmpeg-devel libv4l-devel)
        display=(SDL2-devel)
        network=(NetworkManager-libnm-devel upower-devel)
        update_cmd=()
        install_cmd=(sudo dnf install -y)
        ;;
    pacman)
        base=(base-devel cmake ninja pkgconf boost openssl)
        runtime=(proot)
        media=(ffmpeg portaudio v4l-utils)
        video=(ffmpeg v4l-utils)
        display=(sdl2)
        network=(networkmanager upower)
        update_cmd=()
        install_cmd=(sudo pacman -S --needed)
        ;;
    *)
        cat >&2 <<'EOF'
Unsupported package manager. Install the following dependency groups manually:
  base: C++ compiler, cmake, ninja, pkg-config, Boost.System, OpenSSL
  runtime: proot
  media: FFmpeg development libraries, PortAudio, V4L2 headers
  video: FFmpeg development libraries, V4L2 headers
  display: SDL2 development package
  network: NetworkManager libnm and UPower development packages
EOF
        exit 2
        ;;
esac

packages=("${base[@]}")
case "$profile" in
    minimal)
        ;;
    media)
        packages+=("${media[@]}")
        ;;
    video)
        packages+=("${video[@]}")
        ;;
    display)
        packages+=("${display[@]}")
        ;;
    network)
        packages+=("${network[@]}")
        ;;
    full)
        packages+=("${runtime[@]}" "${media[@]}" "${display[@]}" "${network[@]}")
        ;;
esac

if [ "${#update_cmd[@]}" -gt 0 ]; then
    run_or_print "${update_cmd[@]}"
fi
run_or_print "${install_cmd[@]}" "${packages[@]}"

echo "Linux HAL dependency setup completed for profile: $profile"
