#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Check if any files in the specified directories contain strings from a given list.
# Checks both filenames and file contents. If any match is found, return an error.
#
# Usage Examples:
#   1. Check Python and JavaScript files for forbidden strings (comma-separated):
#      python check_forbidden_strings.py /path/to/directory "token,secret,password,api_key" ".*\.(py|js)$"
#
#   2. Check multiple directories using a file containing directory paths:
#      python check_forbidden_strings.py /path/to/directories.txt /path/to/forbidden_strings.txt ".*\.(cpp|hpp)$"
#
#   3. Check multiple directories (comma-separated):
#      python check_forbidden_strings.py "/path/to/dir1,/path/to/dir2" "token,secret" ".*"
#
#   4. Check only specific file types in a single directory:
#      python check_forbidden_strings.py /path/to/directory "password,key" ".*\.(txt|md|yml)$"
#
# Arguments:
#   directory_list: Directory path(s) to check (recursively). Can be:
#                   - A single directory path
#                   - Comma-separated directory paths
#                   - A file path containing directory paths (one per line or comma-separated)
#   string_list: Either comma-separated strings or a file path containing strings (one per line)
#   filename_pattern: Regular expression pattern for matching filenames to check
#
# Return Code:
#   0: No violations found
#   1: Violations found or error occurred

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Set


def get_files_by_pattern(directory: str, filename_pattern: str) -> List[Path]:
    """
    Get all files in the directory that match the filename pattern.

    Args:
        directory: The directory to search in
        filename_pattern: Regular expression pattern for matching filenames

    Returns:
        List of Path objects matching the pattern
    """
    directory_path = Path(directory)
    if not directory_path.exists():
        raise ValueError(f"Directory does not exist: {directory}")

    if not directory_path.is_dir():
        raise ValueError(f"Path is not a directory: {directory}")

    pattern = re.compile(filename_pattern)
    matching_files = []

    for root, dirs, files in os.walk(directory):
        for filename in files:
            if pattern.search(filename):
                matching_files.append(Path(root) / filename)

    return matching_files


def check_filenames_for_strings(files: List[Path], forbidden_strings: Set[str]) -> List[tuple]:
    """
    Check if any filenames contain any of the forbidden strings.

    Args:
        files: List of file paths to check
        forbidden_strings: Set of strings that should not appear in filenames

    Returns:
        List of tuples (file_path, matched_string) for files that contain forbidden strings
    """
    violations = []

    for file_path in files:
        filename = file_path.name
        for forbidden_str in forbidden_strings:
            if forbidden_str in filename:
                violations.append((file_path, forbidden_str))

    return violations


def check_file_contents_for_strings(files: List[Path], forbidden_strings: Set[str]) -> List[tuple]:
    """
    Check if any file contents contain any of the forbidden strings.

    Args:
        files: List of file paths to check
        forbidden_strings: Set of strings that should not appear in file contents

    Returns:
        List of tuples (file_path, matched_string) for files that contain forbidden strings
    """
    violations = []

    for file_path in files:
        try:
            # Try to read file as text
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                for forbidden_str in forbidden_strings:
                    if forbidden_str in content:
                        violations.append((file_path, forbidden_str))
                        # Only report first match per file
                        break
        except (UnicodeDecodeError, PermissionError, IOError):
            # Skip binary files or files that can't be read
            continue

    return violations


def parse_string_list(string_list_arg: str) -> Set[str]:
    """
    Parse the string list argument.
    Supports both comma-separated strings and file path containing strings (one per line or comma-separated).

    Args:
        string_list_arg: Either comma-separated strings or a file path

    Returns:
        Set of strings to check for
    """
    # Check if it's a file path
    if os.path.isfile(string_list_arg):
        with open(string_list_arg, 'r', encoding='utf-8') as f:
            content = f.read()
        # Split by newlines first, then by commas for each line
        strings = []
        for line in content.split('\n'):
            line = line.strip()
            if line:
                # Also split by comma in case the line contains comma-separated values
                strings.extend([s.strip() for s in line.split(',') if s.strip()])
        return set(strings)
    else:
        # Treat as comma-separated strings
        strings = [s.strip() for s in string_list_arg.split(',') if s.strip()]
        return set(strings)


def parse_directory_list(directory_list_arg: str) -> List[str]:
    """
    Parse the directory list argument.
    Supports both comma-separated directory paths and file path containing directories (one per line or comma-separated).

    Args:
        directory_list_arg: Either comma-separated directory paths or a file path

    Returns:
        List of directory paths to check
    """
    # Check if it's a file path
    if os.path.isfile(directory_list_arg):
        with open(directory_list_arg, 'r', encoding='utf-8') as f:
            content = f.read()
        # Split by newlines first, then by commas for each line
        directories = []
        for line in content.split('\n'):
            line = line.strip()
            if line and not line.startswith('#'):  # Skip comment lines
                # Also split by comma in case the line contains comma-separated values
                directories.extend([d.strip() for d in line.split(',') if d.strip()])
        return directories
    else:
        # Treat as comma-separated directory paths or single directory
        directories = [d.strip() for d in directory_list_arg.split(',') if d.strip()]
        return directories


def check(directory_list: str, string_list: str, filename_pattern: str) -> int:
    """
    Main check function.

    Args:
        directory_list: Directory list (comma-separated or file path)
        string_list: String list (comma-separated or file path)
        filename_pattern: Regular expression pattern for matching filenames

    Returns:
        0 if no violations found, 1 otherwise
    """
    try:
        # Parse the directory list
        directories = parse_directory_list(directory_list)

        if not directories:
            print("Error: No directories provided in the directory list")
            return 1

        # Parse the string list
        forbidden_strings = parse_string_list(string_list)

        if not forbidden_strings:
            print("Error: No strings provided in the string list")
            return 1

        # Collect all matching files from all directories
        all_matching_files = []
        for directory in directories:
            try:
                files = get_files_by_pattern(directory, filename_pattern)
                all_matching_files.extend(files)
            except ValueError as e:
                print(f"Warning: {e}", file=sys.stderr)
                continue

        if not all_matching_files:
            print(f"No files found matching pattern '{filename_pattern}' in directories: {', '.join(directories)}")
            return 0

        # Check for violations in filenames
        filename_violations = check_filenames_for_strings(all_matching_files, forbidden_strings)

        # Check for violations in file contents
        content_violations = check_file_contents_for_strings(all_matching_files, forbidden_strings)

        # Combine all violations
        violations = filename_violations + content_violations

        if violations:
            print(f"Error: Found {len(violations)} file(s) containing forbidden strings:")
            for file_path, matched_string in violations:
                print(f"  - {file_path}: contains '{matched_string}'")
            return 1
        else:
            print(f"Success: No forbidden strings found in {len(all_matching_files)} matching file(s) across {len(directories)} directory(ies)")
            return 0

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Check if files in directories contain any forbidden strings (checks both filenames and file contents)',
        prog='check_forbidden_strings.py'
    )
    parser.add_argument(
        'directory_list',
        help='Directory list: either a single directory, comma-separated directories, or a file path containing directories (one per line)'
    )
    parser.add_argument(
        'string_list',
        help='String list: either comma-separated strings or a file path containing strings (one per line)'
    )
    parser.add_argument(
        'filename_pattern',
        help='Regular expression pattern for matching filenames to check'
    )

    args = parser.parse_args()

    return check(args.directory_list, args.string_list, args.filename_pattern)


if __name__ == '__main__':
    sys.exit(main())
