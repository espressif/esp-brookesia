# ChangeLog

## v0.7.2 - 2026-04-10

### Enhancements:

- feat(docs): add Doxygen documentation to public types and methods in `service_sntp.hpp` and `macro_configs.h`
- feat(build): enable `MINIMAL_BUILD` in test apps `CMakeLists.txt`; add `PRIV_REQUIRES unity esp_psram` to `main/CMakeLists.txt`

### Bug Fixes:

- fix(test): replace `BROOKESIA_CHECK_FALSE_RETURN` with `TEST_ASSERT_TRUE_MESSAGE` for proper Unity test failure reporting

## v0.7.1 - 2026-02-02

### Enhancements:

- feat(Kconfig): add `BROOKESIA_SERVICE_SNTP_ENABLE_AUTO_REGISTER` option for automatic plugin registration
- feat(repo): update `brookesia_service_helper` dependency to `0.7.*`

## v0.7.0 - 2025-12-24

### Initial Release

- feat(repo): Add SNTP (Simple Network Time Protocol) service for ESP-Brookesia ecosystem
