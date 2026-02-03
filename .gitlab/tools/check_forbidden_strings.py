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
#      python check_forbidden_strings.py -d /path/to/directory -s "token,secret,password,api_key" -p ".*\.(py|js)$"
#
#   2. Check multiple directories using a file containing directory paths:
#      python check_forbidden_strings.py -d /path/to/directories.txt -s /path/to/forbidden_strings.txt -p ".*\.(cpp|hpp)$"
#
#   3. Check multiple directories (comma-separated):
#      python check_forbidden_strings.py -d "/path/to/dir1,/path/to/dir2" -s "token,secret" -p ".*"
#
#   4. Check only specific file types in a single directory:
#      python check_forbidden_strings.py -d /path/to/directory -s "password,key" -p ".*\.(txt|md|yml)$"
#
#   5. Check specific files directly (ignores -d and -p):
#      python check_forbidden_strings.py -f /path/to/file1.cpp /path/to/file2.hpp -s "token,secret"
#
#   6. Check specific files using a file list:
#      python check_forbidden_strings.py -f /path/to/files.txt -s /path/to/forbidden_strings.txt
#
#   7. Use with pre-commit (files are automatically appended):
#      entry: ./check_forbidden_strings.py -s ./forbidden_strings.txt -f
#
#   8. Enable debug mode to see all arguments and processing details:
#      python check_forbidden_strings.py -f file1.cpp file2.hpp -s "token,secret" --debug
#
# Arguments:
#   -d, --directories: Directory path(s) to check (recursively). Can be:
#                      - A single directory path
#                      - Comma-separated directory paths
#                      - A file path containing directory paths (one per line or comma-separated)
#   -s, --strings: Either comma-separated strings or a file path containing strings (one per line)
#   -p, --pattern: Regular expression pattern for matching filenames to check
#   -f, --files: Specific file(s) to check (ignores -d and -p when specified). Can be:
#                - Multiple file paths as separate arguments (e.g., -f file1.cpp file2.hpp)
#                - A single file containing file paths (one per line)
#                - Used with pre-commit, files are automatically appended after -f
#   --debug: Enable debug mode to print all arguments and processing details
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


def parse_file_list(file_list_arg: str) -> List[Path]:
    """
    Parse the file list argument.
    Supports both comma-separated file paths and a file path containing file paths (one per line or comma-separated).

    Args:
        file_list_arg: Either comma-separated file paths or a file path containing file paths

    Returns:
        List of Path objects to check
    """
    files = []

    # Check if it's a file containing a list of files
    if os.path.isfile(file_list_arg):
        # Try to read it as a file list
        with open(file_list_arg, 'r', encoding='utf-8') as f:
            content = f.read()
        # Split by newlines first, then by commas for each line
        for line in content.split('\n'):
            line = line.strip()
            if line and not line.startswith('#'):  # Skip comment lines
                # Also split by comma in case the line contains comma-separated values
                for item in line.split(','):
                    item = item.strip()
                    if item:
                        path = Path(item)
                        if path.is_file():
                            files.append(path)
        # If no valid files found from content, treat the argument itself as a file to check
        if not files:
            files.append(Path(file_list_arg))
    else:
        # Treat as comma-separated file paths
        for item in file_list_arg.split(','):
            item = item.strip()
            if item:
                path = Path(item)
                if path.is_file():
                    files.append(path)

    return files


def check(string_list: str, directory_list: str = None, filename_pattern: str = None, file_list: str = None, debug: bool = False) -> int:
    """
    Main check function.

    Args:
        string_list: String list (comma-separated or file path)
        directory_list: Directory list (comma-separated or file path), ignored if file_list is provided
        filename_pattern: Regular expression pattern for matching filenames, ignored if file_list is provided
        file_list: Specific file list (comma-separated or file path), takes priority over directory_list and filename_pattern
        debug: Enable debug output

    Returns:
        0 if no violations found, 1 otherwise
    """
    try:
        # Parse the string list
        forbidden_strings = parse_string_list(string_list)

        if debug:
            print(f"DEBUG: Parsed {len(forbidden_strings)} forbidden string(s): {forbidden_strings}")

        if not forbidden_strings:
            print("Error: No strings provided in the string list")
            return 1

        all_matching_files = []

        # If file_list is provided, use it directly (ignore directory_list and filename_pattern)
        if file_list:
            if debug:
                print(f"DEBUG: Using file list mode, parsing: {file_list}")
            all_matching_files = parse_file_list(file_list)
            if not all_matching_files:
                print("Error: No valid files provided in the file list")
                return 1
            if debug:
                print(f"DEBUG: Found {len(all_matching_files)} file(s) to check:")
                for f in all_matching_files:
                    print(f"  - {f}")
            source_desc = f"{len(all_matching_files)} specified file(s)"
        else:
            # Use directory_list and filename_pattern
            if not directory_list:
                print("Error: Either -f (files) or -d (directories) must be provided")
                return 1

            if not filename_pattern:
                print("Error: -p (pattern) is required when using -d (directories)")
                return 1

            if debug:
                print(f"DEBUG: Using directory mode")
                print(f"  Pattern: {filename_pattern}")

            # Parse the directory list
            directories = parse_directory_list(directory_list)

            if not directories:
                print("Error: No directories provided in the directory list")
                return 1

            if debug:
                print(f"DEBUG: Searching in {len(directories)} directory(ies):")
                for d in directories:
                    print(f"  - {d}")

            # Collect all matching files from all directories
            for directory in directories:
                try:
                    files = get_files_by_pattern(directory, filename_pattern)
                    if debug and files:
                        print(f"DEBUG: Found {len(files)} file(s) in {directory}")
                    all_matching_files.extend(files)
                except ValueError as e:
                    print(f"Warning: {e}", file=sys.stderr)
                    continue

            if not all_matching_files:
                print(f"No files found matching pattern '{filename_pattern}' in directories: {', '.join(directories)}")
                return 0

            if debug:
                print(f"DEBUG: Total {len(all_matching_files)} file(s) to check")

            source_desc = f"{len(all_matching_files)} matching file(s) across {len(directories)} directory(ies)"

        # Check for violations in filenames
        if debug:
            print(f"DEBUG: Checking filenames for forbidden strings...")
        filename_violations = check_filenames_for_strings(all_matching_files, forbidden_strings)

        # Check for violations in file contents
        if debug:
            print(f"DEBUG: Checking file contents for forbidden strings...")
        content_violations = check_file_contents_for_strings(all_matching_files, forbidden_strings)

        # Combine all violations
        violations = filename_violations + content_violations

        if debug:
            print(f"DEBUG: Found {len(filename_violations)} filename violation(s)")
            print(f"DEBUG: Found {len(content_violations)} content violation(s)")

        if violations:
            print(f"Error: Found {len(violations)} file(s) containing forbidden strings:")
            for file_path, matched_string in violations:
                print(f"  - {file_path}: contains '{matched_string}'")
            return 1
        else:
            print(f"Success: No forbidden strings found in {source_desc}")
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
        '-d', '--directories',
        help='Directory list: either a single directory, comma-separated directories, or a file path containing directories (one per line)'
    )
    parser.add_argument(
        '-s', '--strings',
        required=True,
        help='String list: either comma-separated strings or a file path containing strings (one per line)'
    )
    parser.add_argument(
        '-p', '--pattern',
        help='Regular expression pattern for matching filenames to check (required when using -d)'
    )
    parser.add_argument(
        '-f', '--files',
        nargs='*',
        help='Specific file(s) to check. Can be: multiple file paths, a single file containing file paths, or comma-separated files. When specified, -d and -p are ignored'
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Enable debug mode to print all arguments and processing details'
    )

    args = parser.parse_args()

    # Debug mode: print all arguments
    if args.debug:
        print("=" * 60)
        print("DEBUG MODE - Arguments received:")
        print("=" * 60)
        print(f"  -s/--strings:     {args.strings}")
        print(f"  -d/--directories: {args.directories}")
        print(f"  -p/--pattern:     {args.pattern}")
        print(f"  -f/--files:       {args.files}")
        if args.files:
            print(f"    Number of files: {len(args.files)}")
            for idx, file in enumerate(args.files, 1):
                print(f"      [{idx}] {file}")
        print("=" * 60)
        print()

    # Validate arguments
    if not args.files and not args.directories:
        parser.error("Either -f (--files) or -d (--directories) must be provided")

    if args.directories and not args.pattern and not args.files:
        parser.error("-p (--pattern) is required when using -d (--directories)")

    # Convert files list to comma-separated string for compatibility with parse_file_list
    file_list = ','.join(args.files) if args.files else None

    # If using -f but no files provided (e.g., pre-commit with no matching files), exit successfully
    if args.files is not None and not args.files:
        print("No files to check")
        return 0

    return check(
        string_list=args.strings,
        directory_list=args.directories,
        filename_pattern=args.pattern,
        file_list=file_list,
        debug=args.debug
    )


if __name__ == '__main__':
    sys.exit(main())
