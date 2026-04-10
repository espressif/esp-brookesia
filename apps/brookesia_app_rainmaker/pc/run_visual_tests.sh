#!/bin/bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
#
# Visual regression test runner for RainMaker UI
# Runs tests at multiple resolutions and generates comparison reports
#

set -e

# Resolve paths relative to this script so it works when invoked from another directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BASELINE_DIR="${1:-visual_test_baseline}"
OUTPUT_DIR="${2:-visual_test_output}"
DIFF_DIR="${3:-visual_test_diff}"
REPORT_FILE="${4:-visual_test_report.html}"
THRESHOLD="${5:-0.99}"

# Build directory
BUILD_DIR="build"

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    print_error "Build directory not found: $BUILD_DIR"
    echo "Run: cmake -B $BUILD_DIR -S . && cmake --build $BUILD_DIR"
    exit 1
fi

# Check if test executable exists
if [ ! -f "$BUILD_DIR/rainmaker_ui_visual_test" ]; then
    print_error "Test executable not found: $BUILD_DIR/rainmaker_ui_visual_test"
    echo "Run: cmake --build $BUILD_DIR"
    exit 1
fi

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    print_error "Python 3 is required but not installed"
    exit 1
fi

# Check if Pillow is installed
if ! python3 -c "import PIL" 2>/dev/null; then
    print_warning "Pillow not installed. Installing..."
    pip3 install Pillow
fi

print_info "Starting visual regression tests..."
print_info "Baseline directory: $BASELINE_DIR"
print_info "Output directory: $OUTPUT_DIR"
print_info "Threshold: $THRESHOLD"
echo ""

# One invocation: 6 built-in resolutions in the binary; output dir is cleared inside the binary
# before capture (avoids stale files and avoids per-resolution runs wiping previous sizes).
print_info "Running visual tests (default 6 resolutions → $OUTPUT_DIR)..."
echo ""

if ! "$BUILD_DIR/rainmaker_ui_visual_test" -o "$OUTPUT_DIR"; then
    print_error "Visual test run failed"
    exit 1
fi

echo ""
print_success "All screenshots captured"

# Run comparison if baseline exists
if [ -d "$BASELINE_DIR" ] && [ "$(ls -A $BASELINE_DIR)" ]; then
    print_info "Running comparison against baseline..."
    echo ""

    # Run inside `if` so a non-zero exit from Python does not trigger `set -e` and skip open/report
    if python3 visual_test_compare.py \
        "$BASELINE_DIR" \
        "$OUTPUT_DIR" \
        --threshold "$THRESHOLD" \
        --diff-dir "$DIFF_DIR" \
        --report "$REPORT_FILE"; then
        EXIT_CODE=0
    else
        EXIT_CODE=$?
    fi

    echo ""
    if [ $EXIT_CODE -eq 0 ]; then
        print_success "All tests passed!"
    else
        print_warning "Some tests failed. Check $REPORT_FILE for details"
    fi
else
    print_warning "Baseline directory not found or empty: $BASELINE_DIR"
    print_info "Run the following command to create baseline:"
    echo "  $BUILD_DIR/rainmaker_ui_visual_test --create-baseline"
fi

echo ""
print_info "Done!"

# Open HTML report in default browser (path relative to SCRIPT_DIR after cd above)
if [ -f "$REPORT_FILE" ]; then
    print_info "Opening report in browser..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        open "$REPORT_FILE" || print_warning "Could not open report (try: open $REPORT_FILE)"
    elif [[ "$OSTYPE" == linux-gnu* ]] || [[ "$OSTYPE" == linux* ]]; then
        xdg-open "$REPORT_FILE" 2>/dev/null || print_warning "Could not open report (try: xdg-open $REPORT_FILE)"
    else
        print_info "Report: $(pwd)/$REPORT_FILE"
    fi
fi

exit 0
