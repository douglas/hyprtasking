<div align="center">
  <h1>Hyprtasking</h1>
  <p>Powerful workspace management plugin, packed with features.</p>
</div>

> [!Important]
> - Maintained for Hyprland `0.55.x`, `0.56.0`, and `0.56.2`.

## This Fork

This repository started from the original Hyprtasking project and has diverged
into a narrower, maintenance-first branch.

The main differences from the upstream fork source are:

- this branch is maintained for explicit audited Hyprland targets instead of
  trying to span a wide range of compositor releases
- Hyprland-sensitive behavior is pushed behind `src/compat/`, with explicit
  compat ownership, runtime wrappers, and audited hook contracts
- the public runtime surface is intentionally small: grid overview, mouse
  trigger/selection/drag/drop, gestures, directional movement, submap-backed
  keyboard selection, and a health check
- release validation is stricter, with version-contract checks, compat and
  source audits, smoke tests, and manual runtime checks
- overview stability has been hardened around renderer re-entry, focus handoff,
  hover and keyboard selection, and runtime lifecycle edge cases
- overview mouse interaction uses a direct `CInputManager::onMouseButton` hook,
  which keeps drag and right-click selection reliable
- bare key bindings for overview navigation use a Hyprland submap that the
  plugin enters/exits automatically, which eliminates keypress interference
  during normal workspace use

If you want the original wider-compatibility project posture, read the upstream
repository. If you want the branch that tracks the current maintenance work,
testing, and debugging workflow described in this README, use this repository.

https://github.com/user-attachments/assets/8d6cdfd2-2b17-4240-a117-1dbd2231ed4e

#### [Jump To Installation](#Installation)

#### [See Configuration](#Configuration)

## Roadmap

- [x] Grid overview
- [x] Mouse controls
    - [x] Exit into workspace (hover, click)
    - [x] Drag and drop windows
- [x] Keyboard controls
    - [x] Switch workspaces with direction
- [x] Multi-monitor support (tested)
- [x] Monitor scaling support (tested)
- [x] Animation support
- [x] Configurability
    - [x] Number of visible workspaces
    - [x] Toggle behavior
    - [x] Toggle keybind
- [x] Touch and gesture support

## Installation

### AUR

```
yay -S hyprtasking
paru -S hyprtasking
```

### Hyprpm

```
hyprpm add https://github.com/douglas/hyprtasking
hyprpm enable hyprtasking
```

### Nix

Add hyprtasking to your flake inputs and pin Hyprland to the supported
target you run. This example uses `v0.56.2`; `0.55.x` and `v0.56.0` are also
supported.
```nix
# flake.nix
{
  inputs = {
    hyprland.url = "github:hyprwm/Hyprland/v0.56.2";

    hyprtasking = {
      url = "github:douglas/hyprtasking";
      inputs.hyprland.follows = "hyprland";
    };
  };
  # ...
}

```

Include the plugin in the hyprland home manager options

```nix
# home.nix
{ inputs, ... }:
{
  wayland.windowManager.hyprland = {
    plugins = [
      inputs.hyprtasking.packages.${pkgs.system}.hyprtasking
    ];
  }
}
```

### Manual

To build, have hyprland headers installed on the system and then:

```
meson setup build
cd build && meson compile
```

Then use `hyprctl plugin load` to load the absolute path to the `.so` file:

```
hyprctl plugin load "$(realpath libhyprtasking.so)"
```

`hyprctl reload` only reloads configuration and does not reload the plugin
binary. After rebuilding the plugin, unload/reload the `.so` explicitly.

## Maintainer Docs

Maintenance docs live in `docs/`: the
[debugging playbook](docs/debugging-playbook.md) (manual checks, release gate),
the [compat contract](docs/compat-contract.md) (supported versions, update
workflow), [architecture](docs/architecture.md), and
[troubleshooting](docs/troubleshooting.md). Scripts in `scripts/` document their
options in their header comments.

## Usage

### Opening Overview

- Bind `hyprtasking:toggle, all` to a keybind to open/close the overlay on all monitors.
- Bind `hyprtasking:toggle, cursor` to a keybind to open the overlay on one monitor and close on all monitors.
- Swipe up/down on a touchpad device to open/close the overlay on one monitor.
- See [below](#Configuration) for configuration options.

### Interaction

- Workspace Transitioning:
    - Open the overlay, then use **right click** to switch to a workspace
    - Use **arrow keys** to navigate between workspaces and **Enter** to commit (requires submap config, see [Configuration](#configuration))
    - Use the directional dispatchers `hyprtasking:move` to switch to a workspace
- Window management:
    - **Left click** to drag and drop windows around

## Configuration

Example below:

```
bind = SUPER, tab, hyprtasking:toggle, cursor
bind = SUPER, space, hyprtasking:toggle, all

bind = SUPER, H, hyprtasking:move, left
bind = SUPER, J, hyprtasking:move, down
bind = SUPER, K, hyprtasking:move, up
bind = SUPER, L, hyprtasking:move, right

# Bare key bindings (no modifier) must go inside a Hyprland submap so they
# only intercept keypresses while the overview is open. The plugin enters
# the "hyprtasking" submap automatically when the overview opens and exits
# it when the overview closes. Without a submap, bare key bindings cause
# Hyprland to intercept every keypress globally — even when the overview is
# closed — which drops the next keypress in the application.
submap = hyprtasking
bind = , LEFT, hyprtasking:select, left
bind = , RIGHT, hyprtasking:select, right
bind = , UP, hyprtasking:select, up
bind = , DOWN, hyprtasking:select, down
bind = , RETURN, hyprtasking:commit,
bind = , ESCAPE, hyprtasking:toggle, cursor
submap = reset

# Important: define these binds in sourced config files (`source = ...`) or load
# them once (`exec-once = hyprctl keyword source ...`). Re-sourcing the same
# submap block repeatedly at runtime stacks duplicate binds, which can make one
# arrow keypress navigate multiple workspaces.

plugin {
    hyprtasking {
        drag_button = 0x110 # left mouse button
        select_button = 0x111 # right mouse button
        # for other mouse buttons see <linux/input-event-codes.h>

        gestures {
            enabled = true
            move_fingers = 3
            move_distance = 300
            open_fingers = 4
            open_distance = 300
            open_positive = true
        }

        grid {
            rows = 3
            cols = 3
            loop = false
        }
    }
}
```

### Binding recipes

Selection + Enter recipe:

```ini
bind = SUPER, tab, hyprtasking:toggle, cursor
submap = hyprtasking
bind = , LEFT, hyprtasking:select, left
bind = , RIGHT, hyprtasking:select, right
bind = , UP, hyprtasking:select, up
bind = , DOWN, hyprtasking:select, down
bind = , RETURN, hyprtasking:commit,
bind = , ESCAPE, hyprtasking:toggle, cursor
submap = reset
```

Quick mismatch check:

```bash
hyprctl plugin list
hyprctl keyword source ~/.config/hypr/personal-hyprtasking.conf
hyprctl eval 'hl.plugin.hyprtasking.health()'
```

If `keyword source` prints `invalid dispatcher` and your config still uses
`hyprtasking:navigate` or `hyprtasking:commit_selection`, switch those binds to
`hyprtasking:select` and `hyprtasking:commit`.
Configs that depended on conditional wrapper dispatchers should instead use
the `hyprtasking` submap shown above so bare keys only exist while the overview
is open.

### Dispatchers

- `hyprtasking:toggle, ARG` takes in 1 argument that is either `cursor` or `all`
    - if the argument is `all`, then
        - if all overviews are hidden, then all overviews will be shown
        - otherwise all overviews will be hidden
    - if the argument is `cursor`, then
        - if current monitor's overview is hidden, then it will be shown
        - otherwise all overviews will be hidden

- `hyprtasking:move, ARG` takes in 1 argument that is one of `up`, `down`, `left`, `right`
    - when dispatched, hyprtasking will switch workspaces with a nice animation

- `hyprtasking:select, ARG` takes in 1 argument: one of `up`, `down`, `left`, `right`
    - directional selection updates the keyboard highlight; use `hyprtasking:commit` to switch
    - intended for use inside the `hyprtasking` submap so that bare arrow keys work only while the overview is open

- `hyprtasking:commit` switches to the currently selected workspace and closes the overview
    - intended for use inside the `hyprtasking` submap so that bare Enter works only while the overview is open

- `hyprtasking:health` writes a one-line runtime summary to the Hyprland log and
  a notification; `hl.plugin.hyprtasking.health()` returns the same line:

  ```text
  runtime_enabled=true disable_reason="" failure_source="" config_error="" views=2 active_views=0
  ```

Reload behavior is not configurable: `hyprctl reload` cancels active overview
runtime state and reinitializes grid positions so the compositor never keeps a
half-open overview across a config reload.

Invalid config values are rejected and the previous values stay in effect. This
covers non-positive grid dimensions, duplicate drag/select mouse buttons,
duplicate gesture finger counts, and non-positive gesture distances. The error
appears in a notification and in the health line's `config_error` field.

### Gesture recipes

`open_fingers` and `move_fingers` should use different values to avoid one
gesture driving both actions.

- 3-finger swipe up/down toggles overview, 4-finger swipe moves workspaces:
  - `gestures:open_fingers = 3`
  - `gestures:move_fingers = 4`
- 4-finger swipe up/down toggles overview, 3-finger swipe moves workspaces:
  - `gestures:open_fingers = 4`
  - `gestures:move_fingers = 3`

Use `gestures:open_positive` to pick which swipe direction opens the overview:
`1` means positive swipe opens and negative closes, `0` reverses this.

### Config Options

All options are prefixed with `plugin:hyprtasking:`.
Visual styling uses fixed built-in defaults.

| Option | Type | Description | Default |
| --- | --- | --- | --- |
| `drag_button` | `Hyprlang::INT` | The mouse button to use to drag windows around. Must differ from `select_button`; conflicts are rejected and the previous value is kept. | `0x110` |
| `select_button` | `Hyprlang::INT` | The mouse button to use to select a workspace. Must differ from `drag_button`; conflicts are rejected and the previous value is kept. | `0x111` |
| `gestures:enabled` | `Hyprlang::INT` | Whether or not to enable gestures | `true` |
| `gestures:move_fingers` | `Hyprlang::INT` | The number of fingers to use for the "move" gesture. Keep different from `open_fingers` to avoid gesture overlap. | `3` |
| `gestures:move_distance` | `Hyprlang::FLOAT` | How large of a swipe on the touchpad corresponds to the width of a workspace. Must be `> 0`; invalid values are rejected and the previous value is kept. | `300.f` |
| `gestures:open_fingers` | `Hyprlang::INT` | The number of fingers to use for the open/close gesture. Keep different from `move_fingers` to avoid gesture overlap. | `4` |
| `gestures:open_distance` | `Hyprlang::FLOAT` | How large of a swipe on the touchpad is needed for the "open" gesture. Must be `> 0`; invalid values are rejected and the previous value is kept. | `300.f` |
| `gestures:open_positive` | `Hyprlang::INT` | `true` if swiping up should open the overlay, `false` otherwise | `true` |
| `grid:rows` | `Hyprlang::INT` | The number of rows to display on the grid overlay. Must be `>= 1`; invalid values are rejected and the previous value is kept. | `3` |
| `grid:cols` | `Hyprlang::INT` | The number of columns to display on the grid overlay. Must be `>= 1`; invalid values are rejected and the previous value is kept. | `3` |
| `grid:loop` | `Hyprlang::INT` | When enabled, moving right at the far right of the grid will wrap around to the leftmost workspace, etc. | `false` |
