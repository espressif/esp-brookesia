include_guard(GLOBAL)

function(brookesia_service_storage_init_pc_defaults)
    if(NOT DEFINED BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR)
        set(
            BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR
            "${CMAKE_BINARY_DIR}/.brookesia"
            CACHE PATH "Default Brookesia PC system directory"
        )
    endif()
    if(NOT DEFINED BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR)
        set(
            BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR
            "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR}/kv"
            CACHE PATH "Default Brookesia PC key-value directory"
        )
    endif()
    if(NOT DEFINED BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT)
        set(
            BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT
            "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR}/fs"
            CACHE PATH "Default Brookesia PC file-system root"
        )
    endif()
    if(NOT DEFINED BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR)
        set(
            BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR
            "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_SYSTEM_DIR}/rootfs"
            CACHE PATH "Default Brookesia PC rootfs directory reserved for launch helpers"
        )
    endif()
    if(NOT DEFINED BROOKESIA_SERVICE_STORAGE_PC_PROOT_EXECUTABLE)
        find_program(_brookesia_service_storage_pc_proot_executable NAMES proot)
        if(_brookesia_service_storage_pc_proot_executable)
            set(_brookesia_service_storage_pc_proot_default "${_brookesia_service_storage_pc_proot_executable}")
        else()
            set(_brookesia_service_storage_pc_proot_default "proot")
        endif()
        set(
            BROOKESIA_SERVICE_STORAGE_PC_PROOT_EXECUTABLE
            "${_brookesia_service_storage_pc_proot_default}"
            CACHE FILEPATH "PRoot executable used by Brookesia PC launchers"
        )
    endif()
endfunction()

function(brookesia_service_storage_generate_proot_launcher)
    cmake_parse_arguments(
        LAUNCHER
        ""
        "OUTPUT;TARGET_NAME;WORKING_DIRECTORY;KV_DIR;FS_ROOT;ROOTFS_DIR;LITTLEFS_SOURCE_DIR"
        ""
        ${ARGN}
    )

    if("${LAUNCHER_OUTPUT}" STREQUAL "")
        message(FATAL_ERROR "brookesia_service_storage_generate_proot_launcher requires OUTPUT")
    endif()
    if("${LAUNCHER_TARGET_NAME}" STREQUAL "")
        message(FATAL_ERROR "brookesia_service_storage_generate_proot_launcher requires TARGET_NAME")
    endif()

    brookesia_service_storage_init_pc_defaults()

    if("${LAUNCHER_WORKING_DIRECTORY}" STREQUAL "")
        set(LAUNCHER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()
    if("${LAUNCHER_KV_DIR}" STREQUAL "")
        set(LAUNCHER_KV_DIR "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_KV_DIR}")
    endif()
    if("${LAUNCHER_FS_ROOT}" STREQUAL "")
        set(LAUNCHER_FS_ROOT "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_FS_ROOT}")
    endif()
    if("${LAUNCHER_ROOTFS_DIR}" STREQUAL "")
        set(LAUNCHER_ROOTFS_DIR "${BROOKESIA_SERVICE_STORAGE_PC_DEFAULT_ROOTFS_DIR}")
    endif()

    if("${LAUNCHER_LITTLEFS_SOURCE_DIR}" STREQUAL "")
        set(_launcher_prepare_littlefs_dir "")
    else()
        string(CONCAT _launcher_prepare_littlefs_dir
            "rm -rf \"$fs_root/littlefs\"\n"
            "ln -sfn \"${LAUNCHER_LITTLEFS_SOURCE_DIR}\" \"$fs_root/littlefs\""
        )
    endif()

    set(_launcher_content [=[
#!/usr/bin/env bash
set -euo pipefail

proot_executable="@BROOKESIA_SERVICE_STORAGE_PC_PROOT_EXECUTABLE@"
kv_dir="@LAUNCHER_KV_DIR@"
fs_root="@LAUNCHER_FS_ROOT@"
rootfs_dir="@LAUNCHER_ROOTFS_DIR@"
working_directory="@LAUNCHER_WORKING_DIRECTORY@"

if [ -x "$proot_executable" ]; then
    :
elif command -v "$proot_executable" >/dev/null 2>&1; then
    proot_executable="$(command -v "$proot_executable")"
else
    echo "proot is required to run @LAUNCHER_TARGET_NAME@ with transparent absolute mount paths" >&2
    exit 127
fi

mkdir -p "$kv_dir" "$fs_root" "$rootfs_dir"
mkdir -p "$fs_root/spiffs" "$fs_root/fatfs" "$fs_root/sdcard"
@_launcher_prepare_littlefs_dir@
if [ ! -e "$fs_root/littlefs" ]; then
    mkdir -p "$fs_root/littlefs"
fi

export BROOKESIA_HAL_LINUX_KV_DIR="$kv_dir"
export BROOKESIA_HAL_LINUX_FS_ROOT="$fs_root"

cd "$working_directory"
exec "$proot_executable" \
    -b "$fs_root/spiffs:/spiffs" \
    -b "$fs_root/littlefs:/littlefs" \
    -b "$fs_root/fatfs:/fatfs" \
    -b "$fs_root/sdcard:/sdcard" \
    "$<TARGET_FILE:@LAUNCHER_TARGET_NAME@>" "$@"
]=])
    string(CONFIGURE "${_launcher_content}" _launcher_content @ONLY)
    file(GENERATE OUTPUT "${LAUNCHER_OUTPUT}" CONTENT "${_launcher_content}")
endfunction()
