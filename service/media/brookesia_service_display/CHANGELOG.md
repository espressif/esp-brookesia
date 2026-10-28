# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(dataflow): register a provider for display output routing and visual frame operations.
- chore(build): split display lifecycle, rendering, touch, and DataFlow implementation units.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(service): register a description for Manager metadata queries.
- feat(service): register the component version for centralized Manager service queries.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_service_display` with display-source arbitration for LCD outputs.
- Provides touch and gesture event routing for system UI flows.
