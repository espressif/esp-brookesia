#!/bin/bash

# Get the 'name' field from all sdkconfig.board.name files in the specified directory
# Usage: ./get_boards_name.sh [directory] [format]
# Format options:
#   - space: space-separated (default)
#   - newline: newline-separated
#   - json: JSON array format
#   - bash: Bash array format

# Set default directory and format
DIR="${1:-.}"
FORMAT="${2:-space}"

# Check if directory exists
if [ ! -d "$DIR" ]; then
    echo "Error: Directory '$DIR' does not exist" >&2
    exit 1
fi

# Find all sdkconfig.board.* files and extract names
board_names=()
while IFS= read -r -d '' file; do
    # Extract board name from filename
    # Filename format: sdkconfig.board.{name}
    filename=$(basename "$file")
    if [[ $filename =~ ^sdkconfig\.ci\.board\.(.+)$ ]]; then
        board_name="${BASH_REMATCH[1]}"
        board_names+=("$board_name")
    fi
done < <(find "$DIR" -maxdepth 1 -name "sdkconfig.ci.board.*" -type f -print0)

# Check if any board configuration files were found
if [ ${#board_names[@]} -eq 0 ]; then
    echo "Warning: No sdkconfig.ci.board.* files found in directory '$DIR'" >&2
    exit 0
fi

# Output results based on specified format
case "$FORMAT" in
    "space")
        # Space-separated format
        echo "${board_names[*]}"
        ;;
    "newline")
        # Newline-separated format
        printf '%s\n' "${board_names[@]}"
        ;;
    "json")
        # JSON array format
        printf '['; printf '"%s"' "${board_names[0]}"; printf ', "%s"' "${board_names[@]:1}"; printf ']\n'
        ;;
    "bash")
        # Bash array format
        echo "(${board_names[*]})"
        ;;
    *)
        echo "Error: Unsupported format '$FORMAT'" >&2
        echo "Supported formats: space, newline, json, bash" >&2
        exit 1
        ;;
esac
