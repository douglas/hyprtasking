# Changelog

## 0.6.1 - 2026-10-18

### Fixed

- Snap a window dropped outside every cell to the nearest point of its
  workspace's cell instead of a shrunken box near the cell's top-left.
- Abort a drop when its target cell cannot be found instead of warping the
  pointer and window to the monitor's corner.
- Keep the slide animation after a touchpad move-swipe instead of snapping.
- Let the mouse hover and select cells of workspaces that do not exist yet;
  selecting one creates and switches to it.
- Stop a lost mouse release from freezing hover until the next selection.
- Update the Nix flake: pin Hyprland to `v0.56.2` (it tracked a pre-0.55
  commit), refresh the lock from that release, use the default stdenv and
  report the project version.

### Changed

- Re-read the config once per gesture and per dispatch instead of on every
  swipe event; runtime config changes do not fire Hyprland's reload event.
- Remove about 1,200 more lines of unreachable checks, duplicate state and
  single-use helpers, merge the logic library into one file and the layout
  base class into the grid, and run every smoke stage in `stress` mode.

## 0.6.0 - 2026-10-18

### Removed

- Hyprland 0.54 support. Supported versions are now `0.55.x`, `0.56.0`, and
  `0.56.2`.
- The `debug:trace` config option and the `HYPRTASKING_TRACE` environment
  variable.
- JSON health output. `hyprtasking:health` and
  `hl.plugin.hyprtasking.health()` now return one summary line.

### Fixed

- Guard overview animation-end callbacks and the remaining hook and deferred
  callback paths.
- Ignore a second managed mouse button pressed while a drag is held.
- Reset the submap only if Hyprtasking entered it.
- Hide only views that are open.
- Keep a hook's pointer when removing the hook fails.
- Hit-test keyboard selection against the open overview.
- Start moves with the overview closed from the current workspace.
- Keep exceptions from cleanup code out of destructors, where they would
  terminate Hyprland.
- Return cancelled swipes to where they started instead of committing them.
- Drop a removed monitor's overview right away on Hyprland 0.55, which
  reports the removal before updating its monitor list.
- Stop a bogus render-scope warning when the plugin disables itself
  mid-frame.

### Changed

- Stop copying texture data on every draw and the window list on every
  overview frame.
- Reject grid gaps that leave no room for workspace cells.
- Test the grid geometry, coordinate mapping and navigation directly, and
  run clang-tidy and the source audit in CI.

## 0.5.4 - 2026-10-16

### Changed

- Assign each monitor's block of workspaces by monitor name and keep it
  across hotplug, so disabling one monitor no longer shifts another onto
  different workspaces.
- Reject invalid config values with a notification and a `config_error`
  health field while keeping the previous values, instead of disabling the
  plugin for the session.
- Redraw the overview only while something changes, plus a 100 ms idle
  refresh, instead of at full refresh rate.
- Build optimized binaries by default, including hyprpm installs.
- Write traces to `$XDG_RUNTIME_DIR/hyprtasking-trace.log`.
- Report invalid `move` and `select` arguments as failures.

### Fixed

- Run the compatibility gate before touching Hyprland internals.
- Stop moving workspaces between monitors from inside the render hook.
- End window drags when the overview closes, the config reloads, or a drop
  fails.
- Avoid undefined behavior when Lua actions raise errors.
- Null-check every hook call-through, check solitary blocking for the
  queried monitor, and resync views on monitor layout changes.
- Keep the overview active when toggled during a workspace move animation.
- Cancel deferred callbacks and timers before unload.
- Build against Hyprland 0.54.3 again (Lua bindings are now 0.55+ only).
- Fix the offline compat gate on all supported tracks and add a build,
  sanitizer test, and formatting CI job.

## 0.5.3 - 2026-08-15

### Added

- Support Hyprland `0.56.0` and `0.56.2`, including matching hyprpm pins.

### Fixed

- Render all workspace thumbnails during overview transforms on Hyprland 0.56,
  including multi-window grids, blur, and borders.
- Use Hyprland's Lua plugin API for live smoke checks on Hyprland 0.56+.

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
