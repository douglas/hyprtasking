# Compat Contract

Hyprtasking is maintained for Hyprland `0.54.x` only.

The update workflow is:

1. Run `HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh`.
2. If `scripts/audit-compat.sh` or `scripts/audit-compat-surface.sh` fails, patch the relevant file under `src/compat/`.
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

Not every update-sensitive path is covered by the current audits.

`scripts/audit-compat-surface.sh` now also checks these Hyprland-side contracts:

| Contract | Hyprland location | Likely plugin touchpoint |
| --- | --- | --- |
| focus-state APIs | `src/desktop/state/FocusState.hpp` | `src/compat/runtime_compat.cpp` |
| seat pointer focus state | `src/managers/SeatManager.hpp` | `src/compat/runtime_compat.cpp` |
| input mouse helpers | `src/managers/input/InputManager.hpp` | `src/compat/runtime_compat.cpp` |
| mouse bind mode API | `src/managers/KeybindManager.hpp` | `src/compat/runtime_compat.cpp` |
| cursor override controller | `src/managers/cursor/CursorShapeOverrideController.hpp` | `src/compat/runtime_compat.cpp` |
| layout drag controller entrypoint | `src/layout/LayoutManager.hpp` | `src/compat/runtime_compat.cpp` |
| drag controller state accessors | `src/layout/supplementary/DragController.hpp` | `src/compat/runtime_compat.cpp` |
| pointer warp API | `src/managers/PointerManager.hpp` | `src/compat/renderer_compat.cpp` |
| compositor lookup and workspace APIs | `src/Compositor.hpp` | `src/compat/runtime_compat.cpp`, `src/compat/renderer_compat.cpp` |
| monitor focus and render fields | `src/helpers/Monitor.hpp` | `src/compat/renderer_compat.cpp` |
| workspace render state fields | `src/desktop/Workspace.hpp` | `src/compat/renderer_compat.cpp` |
| window animation and workspace fields | `src/desktop/view/Window.hpp` | `src/compat/runtime_compat.cpp`, `src/compat/renderer_compat.cpp` |
| render pass clear API | `src/render/pass/Pass.hpp` | `src/compat/renderer_compat.cpp` |

These still usually require inspection after a Hyprland bump even if both audits pass:

- event bus listener wiring in `src/compat/runtime_compat.cpp`
- animation/config-backed helper wiring in `src/compat/runtime_compat.cpp`
- renderer hook behavior in `src/compat/renderer_compat.cpp`
- `changeWorkspace` semantics and visibility animation behavior in `src/compat/renderer_compat.cpp`

If a Hyprland update breaks runtime behavior without failing the audit, start with those files.
