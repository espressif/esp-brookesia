# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

### Enhancements:

- feat(gui): add a 480x480 layout variant for the Files app.

## v0.8.2 - 2026-07-27

### Enhancements:

- chore(build): use the lightweight Service Manager function definitions in the file app interface.

## v0.8.1 - 2026-07-13

### Enhancements:

- perf(assets): encode opaque app images as high-quality JPEG resources.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.
- fix(gui): honor the configured GUI DOM preload setting.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_app_files` with storage volume browsing, directory navigation, and file dialogs.
- Provides System Core native app lifecycle integration and localized GUI resources.
