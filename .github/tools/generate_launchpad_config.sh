#!/bin/bash
# Copyright 2022 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script generates the launchpad config based on the application binaries
# present in the current directory
# Sample launchpad config: https://github.com/espressif/esp-launchpad/blob/main/config/config.toml
#
# Usage: generate_launchpad_config.sh <git_remote> [subdir]
# Binary naming format: <APP_NAME>_<VERSION>_<BOARD_NAME>.bin
# Example: service_console_0.7.2_esp_box_3.bin

OUT_FILE=launchpad.toml
git_remote="$1"
subdir="$2"

# Build firmware URL with optional subdir
if [ -n "$subdir" ]; then
    firmware_url="$(echo $git_remote | sed 's|\(.*\)/\(.*\)|https://\1.github.io/\2/|')${subdir}/"
else
    firmware_url="$(echo $git_remote | sed 's|\(.*\)/\(.*\)|https://\1.github.io/\2/|')"
fi

cat <<EOF >> $OUT_FILE
esp_toml_version = 1.0
firmware_images_url = "${firmware_url}"

EOF

# Get unique app names (first part before version)
# Format: app_version_board.bin -> app
get_app_name() {
    echo "$1" | sed 's/^\([^_]*\)_.*/\1/'
}

# Get all unique app names
declare -A APP_NAMES
for bin in *.bin; do
    [ -f "$bin" ] || continue
    app=$(get_app_name "${bin%.bin}")
    APP_NAMES[$app]=1
done

APPS=(${!APP_NAMES[@]})

# Check if any bins found
if [ ${#APPS[@]} -eq 0 ]; then
    echo "Warning: No .bin files found in current directory"
    exit 0
fi

# Build supported applications list
SUPPORTED_APPS="supported_apps = ["
for app in "${APPS[@]}"; do
    SUPPORTED_APPS+="\"$app\","
done
SUPPORTED_APPS+="]"
# Remove the last comma
SUPPORTED_APPS=$(echo $SUPPORTED_APPS | sed 's/\(.*\),/\1/')

echo "$SUPPORTED_APPS" >> $OUT_FILE
echo "" >> $OUT_FILE

# Chip type mapping based on board name
get_chip_from_board() {
    local board="$1"
    case "$board" in
        *p4*|*esp32p4*)
            echo "esp32p4"
            ;;
        *c3*|*esp32c3*)
            echo "esp32c3"
            ;;
        *c6*|*esp32c6*)
            echo "esp32c6"
            ;;
        *s2*|*esp32s2*)
            echo "esp32s2"
            ;;
        *)
            echo "esp32s3"
            ;;
    esac
}

# Build config for each app
for app in "${APPS[@]}"; do
    echo "[$app]" >> $OUT_FILE

    # Find all bins for this app and extract board names and chips
    declare -A CHIP_BINS
    for bin in ${app}_*.bin; do
        [ -f "$bin" ] || continue
        # Extract board name (everything after app_version_ and before .bin)
        # Format: app_version_board.bin
        board=$(echo "${bin%.bin}" | sed "s/^${app}_[^_]*_//")
        chip=$(get_chip_from_board "$board")

        # Group by chip, store bin file
        if [ -z "${CHIP_BINS[$chip]}" ]; then
            CHIP_BINS[$chip]="$bin"
        else
            CHIP_BINS[$chip]="${CHIP_BINS[$chip]}|$bin"
        fi
    done

    # Build chipsets list
    CHIPSETS="chipsets = ["
    for chip in "${!CHIP_BINS[@]}"; do
        chip_upper=$(echo "$chip" | tr 'a-z-' 'A-Z_')
        CHIPSETS+="\"$chip_upper\","
    done
    CHIPSETS+="]"
    CHIPSETS=$(echo $CHIPSETS | sed 's/\(.*\),/\1/')
    echo "$CHIPSETS" >> $OUT_FILE

    # Output image for each chip (use first bin for each chip)
    for chip in "${!CHIP_BINS[@]}"; do
        first_bin=$(echo "${CHIP_BINS[$chip]}" | cut -d'|' -f1)
        echo "image.$chip = \"$first_bin\"" >> $OUT_FILE
    done

    echo "ios_app_url = \"\"" >> $OUT_FILE
    echo "android_app_url = \"\"" >> $OUT_FILE
    echo "" >> $OUT_FILE

    unset CHIP_BINS
done

echo "Generated $OUT_FILE with firmware_url: $firmware_url"
