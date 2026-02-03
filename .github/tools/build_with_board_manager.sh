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

# This script builds an ESP-IDF project using esp_board_manager and generates merged binary
#
# Usage: build_with_board_manager.sh [options]
#
# Options:
#   -p, --project <path>      Project directory path (required)
#   -b, --board <name>        Board name (required)
#   -c, --chip <name>         Chip name, e.g., esp32s3, esp32p4 (required)
#   -o, --output <path>       Output directory for merged binary (required)
#   -n, --name <name>         Output binary name prefix (default: project directory name)
#   -v, --version <version>   Version string to include in output filename (optional)
#   -d, --boards-dir <path>   Boards configuration directory (default: ./boards)
#   --clean                   Clean build directory after build (default: false)
#   -h, --help                Show this help message
#
# Example:
#   build_with_board_manager.sh -p examples/service_console -b esp_box_3 -c esp32s3 -o ./images -v 0.7.2

set -e

# Default values
BOARDS_DIR="./boards"
CLEAN_AFTER_BUILD=false
VERSION=""
NAME=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--project)
            PROJECT_PATH="$2"
            shift 2
            ;;
        -b|--board)
            BOARD_NAME="$2"
            shift 2
            ;;
        -c|--chip)
            CHIP_NAME="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--name)
            NAME="$2"
            shift 2
            ;;
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -d|--boards-dir)
            BOARDS_DIR="$2"
            shift 2
            ;;
        --clean)
            CLEAN_AFTER_BUILD=true
            shift
            ;;
        -h|--help)
            head -35 "$0" | tail -30
            exit 0
            ;;
        *)
            echo "Error: Unknown option $1"
            exit 1
            ;;
    esac
done

# Validate required arguments
if [ -z "$PROJECT_PATH" ]; then
    echo "Error: Project path is required (-p, --project)"
    exit 1
fi

if [ -z "$BOARD_NAME" ]; then
    echo "Error: Board name is required (-b, --board)"
    exit 1
fi

if [ -z "$CHIP_NAME" ]; then
    echo "Error: Chip name is required (-c, --chip)"
    exit 1
fi

if [ -z "$OUTPUT_DIR" ]; then
    echo "Error: Output directory is required (-o, --output)"
    exit 1
fi

# Set default name from project directory
if [ -z "$NAME" ]; then
    NAME=$(basename "$PROJECT_PATH")
fi

# Build output filename
if [ -n "$VERSION" ]; then
    OUTPUT_FILENAME="${NAME}_${VERSION}_${BOARD_NAME}.bin"
else
    OUTPUT_FILENAME="${NAME}_${BOARD_NAME}.bin"
fi

echo "============================================"
echo "Build Configuration:"
echo "  Project:     $PROJECT_PATH"
echo "  Board:       $BOARD_NAME"
echo "  Chip:        $CHIP_NAME"
echo "  Output:      $OUTPUT_DIR/$OUTPUT_FILENAME"
echo "  Boards Dir:  $BOARDS_DIR"
echo "============================================"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Save current directory
ORIGINAL_DIR=$(pwd)

# Change to project directory
cd "$PROJECT_PATH"

# Source IDF environment if not already done
if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH is not set. Please source ESP-IDF export.sh first."
    exit 1
fi

# Install pyyaml if needed (for board manager)
pip install pyyaml --quiet 2>/dev/null || true

# Configure board using esp_board_manager
echo "Configuring board: $BOARD_NAME"
idf.py gen-bmgr-config -b "$BOARD_NAME" -c "$BOARDS_DIR"

# Build project
echo "Building project..."
idf.py build

# Merge binary
echo "Merging binary..."
cd build
esptool.py --chip "$CHIP_NAME" merge_bin -o "$OUTPUT_DIR/$OUTPUT_FILENAME" @flash_args
cd ..

echo "Binary created: $OUTPUT_DIR/$OUTPUT_FILENAME"

# Clean if requested
if [ "$CLEAN_AFTER_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf sdkconfig build managed_components dependencies.lock components/gen_bmgr_codes
fi

# Return to original directory
cd "$ORIGINAL_DIR"

echo "Build completed successfully!"
