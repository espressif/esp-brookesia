# ChangeLog

## v0.8.4 - 2026-10-27

### Enhancements:

- feat(gui): add a 480x480 layout variant for the Settings app.

## v0.8.3 - 2026-08-05

### Bug Fixes:

- fix(wifi): remove stale dynamic network rows before rebuilding the Wi-Fi list.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(settings): persist language and theme preferences and offer explicit restart confirmation.
- chore(build): use the lightweight Service Manager base API in the settings app.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(debug): control Utils memory and thread profilers independently from Settings switches.
- feat(debug): update profiler configuration without coupling it to profiler lifecycle.
- feat(gui): control GUI view debug directly through the native app GUI runtime.
- perf(assets): encode opaque app images as high-quality JPEG resources.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.
- fix(gui): honor the configured GUI DOM preload setting.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_app_settings` with settings screens for product configuration and display preferences.
- Provides native app integration with System Core and service-backed settings data.
