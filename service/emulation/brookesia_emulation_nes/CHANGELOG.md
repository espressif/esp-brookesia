# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(dataflow): route NES video and audio output through provider-neutral operations.
- chore(build): split emulator lifecycle, frame, audio, and ROM implementation units.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(service): register a description for Manager metadata queries.
- feat(service): register the component version for centralized Manager service queries.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_emulation_nes` with NES emulator control through Brookesia service calls.
- Provides display and audio output integration for product applications.
