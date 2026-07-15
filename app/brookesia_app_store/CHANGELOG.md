# ChangeLog

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(cache): cache app metadata, icons, and downloaded packages in per-app directories.
- feat(download): reuse matching cached packages before starting a new download.
- perf(assets): encode opaque app images as high-quality JPEG resources.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.
- fix(download): keep partial packages isolated and clean them up after failures or cancellation.
- fix(refresh): complete app-list refresh before lazy visible-icon updates.
- fix(catalog): keep Store and Local package views separated.
- fix(gui): honor the configured GUI DOM preload setting.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_app_store` with runtime package discovery, installation, and app launch entry points.
- Provides resource-driven App Store UI integrated with System Core package flows.
