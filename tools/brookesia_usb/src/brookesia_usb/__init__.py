"""ESP-Brookesia USB host-control CLI."""

from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("brookesia-usb")
except PackageNotFoundError:
    __version__ = "0.0.0"
