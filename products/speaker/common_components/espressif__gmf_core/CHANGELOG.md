# Changelog

## v0.6.1

### Bug Fixes

- Fixed an issue where gmf_task failed to wait for event bits
- Fixed compilation failure when building for P4


## v0.6.0

### Features
- Added GMF element capabilities
- Added GMF port attribution functionality
- Added gmf_cache APIs for GMF payload caching
- Added truncated loop path handling for gmf_task execution
- Added memory alignment support for GMF fifo bus
- Renamed component to `gmf_core`

### Bug Fixes

- Fixed support for NULL configuration in GMF object initialization
- Standardized return values of port and data bus acquire/release APIs to esp_gmf_err_io_t only
- Improved task list output formatting

## v0.5.1

### Bug Fixes

- Fixed an issue where the block data bus returns a valid size when the done flag is set
- Fixed an issue where the block unit test contained incorrect code


## v0.5.0

### Features

- Initial version of `esp-gmf-core`