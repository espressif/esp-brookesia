#!/bin/bash

# Check parameters
if [ $# -eq 0 ]; then
    echo "Usage: $0 <directory_path>"
    echo "Example: $0 products/speaker"
    exit 1
fi

# Get directory path
target_dir="$1"

# Check if directory exists
if [ ! -d "$target_dir" ]; then
    echo "Error: Directory '$target_dir' does not exist"
    exit 1
fi

# Build sdkconfig.defaults file path
config_file="$target_dir/sdkconfig.defaults"

# Check if file exists
if [ ! -f "$config_file" ]; then
    echo "Error: File '$config_file' does not exist"
    exit 1
fi

# Extract CONFIG_APP_PROJECT_VER value
version=$(grep "^CONFIG_APP_PROJECT_VER=" "$config_file" | cut -d'=' -f2 | sed 's/"//g')

# Check if version information was found
if [ -z "$version" ]; then
    echo "Error: CONFIG_APP_PROJECT_VER configuration not found in file '$config_file'"
    exit 1
fi

# Output version information
# Replace . and - with _
formatted_version=$(echo "$version" | tr '.-' '_')
echo "$formatted_version"
