#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#

"""
Apply a unified diff patch to a target directory using the python 'patch' package.
Usage:
    python3 apply_patch.py --patch <patch_file> --target <target_dir>
    python3 apply_patch.py -p <patch_file> -t <target_dir>
"""
import os
import sys
import argparse

try:
    import patch
except ImportError:
    print("Error: Python 'patch' package is not installed.")
    print('Please install it using: pip install patch')
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description='Apply a unified diff patch to a target directory using python-patch',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 apply_patch.py --patch tools/patch/lvgl.patch --target managed_components/lvgl__lvgl
  python3 apply_patch.py -p my.patch -t my_dir
        """
    )
    parser.add_argument('--patch', '-p', required=True, help='Path to the patch file to apply')
    parser.add_argument('--target', '-t', required=True, help='Target directory to apply the patch to')
    args = parser.parse_args()

    patch_file = os.path.abspath(args.patch)
    target_dir = os.path.abspath(args.target)

    if not os.path.isfile(patch_file):
        print(f"Error: Patch file '{patch_file}' does not exist.")
        sys.exit(1)
    if not os.path.isdir(target_dir):
        print(f"Error: Target directory '{target_dir}' does not exist.")
        sys.exit(1)

    print(f"Applying patch '{patch_file}' to directory '{target_dir}'...")
    pset = patch.fromfile(patch_file)
    if not pset:
        print('Error: Failed to parse patch file.')
        sys.exit(1)

    # Try to apply the patch
    success = pset.apply(root=target_dir, strip=0)
    if success:
        print('Patch applied successfully!')
        sys.exit(0)
    else:
        print('Patch application failed!')
        sys.exit(1)

if __name__ == '__main__':
    main()
