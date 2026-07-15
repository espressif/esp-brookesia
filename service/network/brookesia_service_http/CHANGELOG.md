# ChangeLog

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(service): register a description for Manager metadata queries.
- feat(service): register the component version for centralized Manager service queries.
- chore(scheduler): remove the Worker suffix from default thread names on ESP and PC.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_service_http` with HTTP/HTTPS requests with cancellation, progress, and retry support.
- Provides response delivery to memory or storage-backed files.
