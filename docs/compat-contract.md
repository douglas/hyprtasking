# Compat Contract

Hyprtasking is maintained for Hyprland `0.54.x` only.

The update workflow is:

1. Run `HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh`.
2. If `scripts/audit-compat.sh` fails, patch the relevant file under `src/compat/`.
3. Re-run the same command until the audit, build, tests, smoke checks, and manual scenario output are clean.

## Ownership

These files are the plugin-side compatibility boundary:

- `src/compat/profile.cpp`
  - Supported Hyprland line gating
  - Hook/symbol contract resolution
  - Plugin API version compatibility assumptions
- `src/compat/renderer_compat.cpp`
  - Renderer hook wrappers
  - Workspace/window/monitor render-time access
  - Damage and render-pass helpers
- `src/compat/runtime_compat.cpp`
  - Focus, cursor, seat, drag-controller, and compositor action wrappers

## Audited Contracts

The compat audit currently checks these Hyprland-side contracts:

| Contract | Hyprland location | Likely plugin touchpoint |
| --- | --- | --- |
| public version API | `src/plugins/PluginAPI.hpp` | `src/compat/profile.cpp` |
| version API implementation | `src/plugins/PluginAPI.cpp` | `src/compat/profile.cpp` |
| `renderWorkspace` symbol | `src/render/Renderer.cpp` | `src/compat/profile.cpp`, `src/compat/renderer_compat.cpp` |
| `shouldRenderWindow` symbol | `src/render/Renderer.cpp` | `src/compat/profile.cpp`, `src/compat/renderer_compat.cpp` |
| `renderWindow` symbol | `src/render/Renderer.cpp` | `src/compat/profile.cpp`, `src/compat/renderer_compat.cpp` |
| `isSolitaryBlocked` symbol | `src/helpers/Monitor.hpp` | `src/compat/profile.cpp` |
| plugin API version check | `src/plugins/PluginSystem.cpp` | `src/compat/profile.cpp` |

## Non-Audited Compat Surface

Not every update-sensitive path is covered by `scripts/audit-compat.sh`.

These usually require inspection after a Hyprland bump even if the audit passes:

- focus and cursor handling in `src/compat/runtime_compat.cpp`
- drag-controller access in `src/compat/runtime_compat.cpp`
- render-pass and blur helpers in `src/compat/renderer_compat.cpp`
- monitor/workspace/window accessors in `src/compat/renderer_compat.cpp`

If a Hyprland update breaks runtime behavior without failing the audit, start with those files.
