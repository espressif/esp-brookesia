# ChangeLog

## v0.8.0 - 2026-06-28

### Enhancements:

- feat(repo): move Device service into the system service category for grouped component publishing
- feat(deps): align first-party Brookesia dependencies with the v0.8 release train

### Documentation:

- docs(readme): refresh component README links for the reorganized programming guide

## v0.7.1 - 2026-05-28

### Enhancements:

- feat(storage): expose filesystem directory support metadata in `GetStorageFileSystems`
- feat(storage): add `GetStorageFileSystemCapacity` for dynamic total, used, and free byte queries
- feat(test): add storage filesystem metadata and capacity query coverage

## v0.7.0 - 2026-04-30

### Initial Release

- feat(repo): add `brookesia_service_device` component as the application-layer service gateway to HAL device capabilities
- feat(api): add capability discovery, board information, display backlight, audio player, storage file system, and power battery service functions
- feat(data): persist selected display and audio state through NVS when available
- feat(test): add board-aware test app coverage for display, audio, storage, board information, power battery, and capability queries
