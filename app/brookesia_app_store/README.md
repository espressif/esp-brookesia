# ESP-Brookesia App Store App

* [中文版本](./README_CN.md)

## Overview

`brookesia_app_store` is the built-in native app store for runtime package discovery and installation.

HTTP event buffering tracks at most 16 App Store requests, 33 pending events and a 768 KiB payload budget. Unrelated requests are filtered and progress is coalesced. If a response exceeds the budget, the request is canceled and reported as failed so it can be retried.

Lost results are isolated by request ID. If an event burst exceeds the loss-tracking capacity, active requests continue while making progress; a result that remains unavailable beyond its timeout and retry budget is reported as failed.

For more information, see the [ESP-Brookesia Programming Guide](https://docs.espressif.com/projects/esp-brookesia/en/latest/app/store.html).

## How to Use

### Environment Requirements

Please refer to the following documentation:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)

### Add to Your Project

Please refer to [ESP-Brookesia Programming Guide - How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage).
