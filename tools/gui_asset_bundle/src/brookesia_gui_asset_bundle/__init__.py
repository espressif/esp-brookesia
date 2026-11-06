# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Packer and reader for the Brookesia GUI asset bundle."""

from .bundle import (
    DEFAULT_PATTERNS,
    MAGIC,
    VERSION,
    Bundle,
    BundleEntry,
    BundleError,
    BundleStats,
    collect_assets,
    pack_assets,
    read_bundle,
    verify_bundle,
)

__all__ = [
    "DEFAULT_PATTERNS",
    "MAGIC",
    "VERSION",
    "Bundle",
    "BundleEntry",
    "BundleError",
    "BundleStats",
    "collect_assets",
    "pack_assets",
    "read_bundle",
    "verify_bundle",
]
