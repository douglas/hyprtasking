# Changelog

## 0.5.2 - 2026-05-27

### Changed

- Treat Hyprland `0.55.x` as a supported minor family for runtime and audit
  compatibility checks.
- Add `v0.55.0`, `v0.55.1`, and `v0.55.2` hyprpm rows for known 0.55
  releases.
- Keep the release and compatibility docs aligned with the `0.55.x` support
  target.

### Fixed

- Harden plugin unload by quiescing overview state before hook teardown and
  preventing cleanup exceptions from escaping `PLUGIN_EXIT`.
- Remove Hyprtasking-owned render-pass elements before unloading so Hyprland
  cannot retain plugin vtables after `dlclose`.

## 0.5.1 - 2026-05-21

### Changed

- Allow `hyprtasking:select` to accept positive numeric workspace IDs again,
  selecting the matching overview workspace when it exists.
- Keep overview swipe-close animations repainting during the closing gesture.
- Let interactive dispatchers fall back to another active overview when the
  cursor is not currently over one.

## 0.5 - 2026-05-13

### Changed

- Support Hyprland `0.54.3` and `0.55.0` as the active compatibility targets.
- Simplify Hyprtasking to the grid overview surface and remove non-core layout
  and legacy dispatcher paths.
- Keep keyboard selection on `hyprtasking:select` and `hyprtasking:commit`
  through the managed `hyprtasking` submap.
- Route Hyprland internals through audited `src/compat` wrappers and exact
  `findFunctionsByName` signature checks.
- Add fail-closed runtime behavior for invalid config, missing hooks, missing
  Event::bus listeners, submap failures, unsafe delayed callbacks, and invalid
  geometry or render scale.
- Extend `hyprtasking:health json` with hook and Event::bus listener readiness
  diagnostics.

### Added

- Offline release checks for both supported Hyprland source trees.
- Compatibility contract generation and coverage audits for core and runtime
  Hyprland surfaces.
- Guideline, boundary, and config-key audits for release gating.
- Live smoke and manual runtime checklist scenarios, including `hook-sunset`.
- Architecture, compatibility, troubleshooting, supported-version, and
  maintainer debugging documentation.

### Removed

- Linear overview implementation and non-grid overview support.
- Numeric slot selection and old public dispatchers that duplicated the current
  directional selection/commit flow.
- Deprecated hook lookup fallback through `getFunctionAddressFromSignature`.

### Notes

- The direct mouse-button hook remains intentionally enabled until
  `hook-sunset` live testing passes on every supported Hyprland target.
