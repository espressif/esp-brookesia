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

# Chip type mapping based on bin name
get_chip_from_bin() {
    local bin_name="$1"
    case "$bin_name" in
        *c3*|*esp32c3*)
            echo "esp32c3"
            ;;
        *c5*|*esp32c5*)
            echo "esp32c5"
            ;;
        *c6*|*esp32c6*)
            echo "esp32c6"
            ;;
        *s2*|*esp32s2*)
            echo "esp32s2"
            ;;
        *s3*|*esp32s3*)
            echo "esp32s3"
            ;;
        *p4*|*esp32p4*)
            echo "esp32p4"
            ;;
        *)
            echo "esp32s3"
            ;;
    esac
}

# Collect all bin files
BIN_FILES=()
for bin in *.bin; do
    [ -f "$bin" ] || continue
    BIN_FILES+=("$bin")
done

# Check if any bins found
if [ ${#BIN_FILES[@]} -eq 0 ]; then
    echo "Warning: No .bin files found in current directory"
    exit 0
fi

# Build supported applications list (each bin is a separate app)
SUPPORTED_APPS="supported_apps = ["
for bin in "${BIN_FILES[@]}"; do
    bin_name="${bin%.bin}"
    SUPPORTED_APPS+="\"$bin_name\","
done
SUPPORTED_APPS+="]"
# Remove the last comma
SUPPORTED_APPS=$(echo $SUPPORTED_APPS | sed 's/\(.*\),/\1/')

echo "$SUPPORTED_APPS" >> $OUT_FILE
echo "" >> $OUT_FILE

# Build config for each bin file (each bin gets its own section)
for bin in "${BIN_FILES[@]}"; do
    bin_name="${bin%.bin}"
    chip=$(get_chip_from_bin "$bin_name")
    chip_upper=$(echo "$chip" | tr 'a-z-' 'A-Z_')

    echo "[$bin_name]" >> $OUT_FILE
    echo "chipsets = [\"$chip_upper\"]" >> $OUT_FILE
    echo "image.$chip = \"$bin\"" >> $OUT_FILE
    echo "ios_app_url = \"\"" >> $OUT_FILE
    echo "android_app_url = \"\"" >> $OUT_FILE
    echo "" >> $OUT_FILE
done

echo "Generated $OUT_FILE with firmware_url: $firmware_url"
