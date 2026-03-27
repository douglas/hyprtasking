<div align="center">
  <h1>Hyprtasking</h1>
  <p>Powerful workspace management plugin, packed with features.</p>
</div>

> [!Important]
> - Maintained for Hyprland `v0.54.x`.

https://github.com/user-attachments/assets/8d6cdfd2-2b17-4240-a117-1dbd2231ed4e

#### [Jump To Installation](#Installation)

#### [See Configuration](#Configuration)

## Roadmap

- [ ] Modular Layouts
    - [x] Grid layout
    - [x] Linear layout
    - [ ] Minimap layout
- [x] Mouse controls
    - [x] Exit into workspace (hover, click)
    - [x] Drag and drop windows
- [ ] Keyboard controls
    - [x] Switch workspaces with direction
    - [ ] Switch workspaces with absolute number
- [x] Multi-monitor support (tested)
- [x] Monitor scaling support (tested)
- [x] Animation support
- [x] Configurability
    - [x] Overview exit behavior
    - [x] Number of visible workspaces
    - [x] Custom workspace layouts
    - [x] Toggle behavior
    - [x] Toggle keybind
- [ ] Touch and gesture support
- [ ] Overview layers

## Installation

### Hyprpm

```
hyprpm add https://github.com/raybbian/hyprtasking
hyprpm enable hyprtasking
```

### Nix

Add hyprtasking to your flake inputs
```nix
# flake.nix
{
  inputs = {
    hyprland.url = "github:hyprwm/Hyprland/v0.49.0";

    hyprtasking = {
      url = "github:raybbian/hyprtasking";
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

For a quick live smoke check after the plugin is loaded:

```
bash scripts/smoke-live.sh all
```

Before updating the plugin to a new Hyprland checkout, run the compat audit against the target
source tree:

```
bash scripts/audit-compat.sh /path/to/Hyprland
bash scripts/audit-compat-surface.sh /path/to/Hyprland
```

To verify the repo still keeps direct Hyprland internals behind `src/compat/`:

```
bash scripts/audit-boundary.sh
```

Supported smoke subcommands:

- `all`: unload/load the plugin, verify dispatcher registration, run toggle/move probes, then reload Hyprland
- `stress`: run a heavier live cycle with repeated unload/load, toggle/move, reload, and reload-open checks
- `dispatchers`: verify that `toggle`, `move`, and `movewindow` dispatchers are registered and still return the expected probe responses without changing overview state
- `load-unload`: validate one or more live unload/load cycles for `build/libhyprtasking.so`
- `toggle`: validate toggle and directional move dispatchers
- `reload`: reload Hyprland and wait for the plugin to become ready again
- `reload-open`: reload Hyprland while the overview is open, then verify the plugin still responds
- `manual`: print the manual compositor checks that still need eyes on the result

Optional environment:

- `HYPRLAND_INSTANCE_SIGNATURE`: target a specific live Hyprland session. If unset, the script picks the first active instance from `hyprctl instances`.
- `PLUGIN_PATH`: override the plugin path used by `load-unload`
- `LOAD_UNLOAD_CYCLES`: number of unload/load cycles to run for `load-unload`
- `TOGGLE_CYCLES`: number of open/move/close toggle cycles to run for `toggle` or `all`
- `RELOAD_CYCLES`: number of reload cycles to run for `reload`, `reload-open`, or `all`
- `PRINT_MANUAL_FOLLOW_UP`: set to `0` to suppress the checklist print at the end of `all` or `stress`

`stress` uses stronger defaults when the cycle variables are left at `1`:
- `LOAD_UNLOAD_CYCLES=3`
- `TOGGLE_CYCLES=3`
- `RELOAD_CYCLES=2`

## Updating Hyprland Compatibility

Hyprtasking is maintained for one Hyprland minor line at a time. The current supported line is
`0.54.x`.

Update workflow:

1. Point `scripts/audit-compat.sh` and `scripts/audit-compat-surface.sh` at the target Hyprland source tree.
2. If either audit fails, first confirm that the target tree is still on the supported `0.54.x` line, then update only the compat layer in `src/compat/`.
3. Rebuild and run:

```
meson compile -C build
meson test -C build
bash scripts/smoke-live.sh all
```

4. Finish with the manual compositor checks printed by `bash scripts/smoke-live.sh manual`.

For the same flow through one command:

```
HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh
```

For a single local release-prep command, run:

```
bash scripts/release-check.sh
```

`release-check.sh` defaults to the heavier `stress` smoke mode. Override with
`SMOKE_MODE=all` if you want the lighter path, or set `PRINT_MANUAL_CHECKLIST=0`
to skip printing the manual compositor checklist. The script suppresses the
intermediate smoke-script reminder so the manual checklist is printed once. It
also runs a repo-local compat boundary audit plus a separate `dispatchers` probe
first so architectural regressions and hook/registration failures are
easier to distinguish from later runtime smoke failures. If `HYPRLAND_SOURCE`
is set, it runs `scripts/audit-compat.sh` and `scripts/audit-compat-surface.sh`
first and then continues with the boundary audit, normal build, test, smoke,
and manual-check flow.

To print only one manual scenario after the automated checks, set
`MANUAL_SCENARIO`:

```
MANUAL_SCENARIO=movewindow bash scripts/release-check.sh
```

For tooling, request one machine-readable result for the full release gate:

```
RELEASE_CHECK_FORMAT=json HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh
```

`audit-compat.sh` prints the detected Hyprland version, the accepted support
line, and the required hook/API contracts it verified so version drift is easier
to diagnose during updates. On failure, it also prints the missing contract list
and the first place to patch. The full ownership map lives in
`docs/compat-contract.md`.

`audit-compat-surface.sh` does the same for the runtime-sensitive wrapper
surface under `src/compat/`, covering focus, cursor, drag-controller,
workspace/window/monitor accessor, and render-pass contracts that previously
required only manual review after a Hyprland bump.

`audit-boundary.sh` also enforces that direct Hyprland focus mutation stays out
of layout/render code, so overview rendering cannot silently start reasserting
monitor focus again.

Audit exit codes:

- `2`: required Hyprland source file missing
- `3`: unsupported Hyprland minor line for this plugin branch
- `4`: supported line, but one or more audited contracts drifted

For tooling or CI wrappers, request machine-readable output:

```
AUDIT_FORMAT=json bash scripts/audit-compat.sh /path/to/Hyprland
AUDIT_FORMAT=json bash scripts/audit-compat-surface.sh /path/to/Hyprland
```

The manual compositor checklist is also available directly:

```
bash scripts/manual-runtime-check.sh
```

Supported checklist scenarios:

- `all`: print the full manual checklist
- `drag`: focus on overview drag/drop behavior
- `typing-focus`: focus on immediate typing and `Ctrl+L` after right-click workspace selection
- `movewindow`: focus on the `hyprtasking:movewindow` path and exact commands to run
- `gesture`: focus on gesture interruption and recovery
- `reload-open`: focus on reload while the overview is visible
- `monitor-remove`: focus on monitor removal while runtime state is active

## Usage

### Opening Overview

- Bind `hyprtasking:toggle, all` to a keybind to open/close the overlay on all monitors.
- Bind `hyprtasking:toggle, cursor` to a keybind to open the overlay on one monitor and close on all monitors.
- Swipe up/down on a touchpad device to open/close the overlay on one monitor.
- See [below](#Configuration) for configuration options.

### Interaction

- Workspace Transitioning:
    - Open the overlay, then use **right click** to switch to a workspace
    - Use the directional dispatchers `hyprtasking:move` to switch to a workspace
- Window management:
    - **Left click** to drag and drop windows around

## Configuration

Example below:

```
bind = SUPER, tab, hyprtasking:toggle, cursor
bind = SUPER, space, hyprtasking:toggle, all
# NOTE: the lack of a comma after hyprtasking:toggle!
bind = , escape, hyprtasking:if_active, hyprtasking:toggle cursor


bind = SUPER, X, hyprtasking:killhovered

bind = SUPER, H, hyprtasking:move, left
bind = SUPER, J, hyprtasking:move, down
bind = SUPER, K, hyprtasking:move, up
bind = SUPER, L, hyprtasking:move, right

plugin {
    hyprtasking {
        layout = grid

        gap_size = 20
        bg_color = 0xff26233a
        border_size = 4
        exit_on_hovered = false
        warp_on_move_window = 1
        close_overview_on_reload = true

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
            gaps_use_aspect_ratio = false
        }

        linear {
            top = false
            height = 400
            scroll_speed = 1.0
            blur = false
        }
    }
}
```

### Dispatchers

- `hyprtasking:if_active, ARG` takes in a dispatch command (one that would be used after `hyprctl dispatch ...`) that will be dispatched only if the cursor overview is active.
    - Allows you to use e.g. `escape` to close the overview when it is active. See the [example config](#configuration) for more info.

- `hyprtasking:if_not_active, ARG` same as above, but if the overview is not active.

- `hyprtasking:toggle, ARG` takes in 1 argument that is either `cursor` or `all`
    - if the argument is `all`, then
        - if all overviews are hidden, then all overviews will be shown
        - otherwise all overviews will be hidden
    - if the argument is `cursor`, then
        - if current monitor's overview is hidden, then it will be shown
        - otherwise all overviews will be hidden

- `hyprtasking:move, ARG` takes in 1 argument that is one of `up`, `down`, `left`, `right`
    - when dispatched, hyprtasking will switch workspaces with a nice animation

- `hyprtasking:movewindow, ARG` takes in 1 argument that is one of `up`, `down`, `left`, `right`
    - when dispatched, hyprtasking will 1. move the hovered window to the workspace in the given direction relative to the window, and 2. switch to that workspace.

- `hyprtasking:killhovered` behaves similarly to the standard `killactive` dispatcher with focus on hover
    - when dispatched, hyprtasking will the currently hovered window, useful when the overview is active.
    - this dispatcher is designed to **replace** killactive, it will work even when the overview is **not active**.

### Config Options

All options should are prefixed with `plugin:hyprtasking:`.

| Option | Type | Description | Default |
| --- | --- | --- | --- |
| `layout` | `Hyprlang::STRING` | The layout to use, either `grid` or `linear` | `grid` |
| `bg_color` | `Hyprlang::INT` | The color of the background of the overlay | `0x000000FF` |
| `gap_size` | `Hyprlang::FLOAT` | The width in logical pixels of the gaps between workspaces | `8.f` |
| `border_size` | `Hyprlang::FLOAT` | The width in logical pixels of the borders around workspaces | `4.f` |
| `exit_on_hovered` | `Hyprlang::INT` | If true, hiding the workspace will exit to the hovered workspace instead of the active workspace. | `false` |
| `warp_on_move_window` | `Hyprlang::INT` | Works the same as `cursor:warp_on_change_workspace` (see [wiki](https://wiki.hypr.land/Configuring/Variables/#cursor)) but with `hyprtasking:movewindow` dispathcer. <br> `cursor:warp_on_change_workspace` works only with `hyprtasking:move` dispathcer | `1` |
| `close_overview_on_reload ` | `Hyprlang::INT` | Whether to close the overview if its type didn't type didn't change after hyprland config reload | `true` |
| `drag_button` | `Hyprlang::INT` | The mouse button to use to drag windows around | `0x110` |
| `select_button` | `Hyprlang::INT` | The mouse button to use to select a workspace | `0x111` |
| `gestures:enabled` | `Hyprlang::INT` | Whether or not to enable gestures | `true` |
| `gestures:move_fingers` | `Hyprlang::INT` | The number of fingers to use for the "move" gesture | `3` |
| `gestures:move_distance` | `Hyprlang::FLOAT` | How large of a swipe on the touchpad corresponds to the width of a workspace | `300.f` |
| `gestures:open_fingers` | `Hyprlang::INT` | The number of fingers to use for the "open" gesture | `4` |
| `gestures:open_distance` | `Hyprlang::FLOAT` | How large of a swipe on the touchpad is needed for the "open" gesture | `300.f` |
| `gestures:open_positive` | `Hyprlang::INT` | `true` if swiping up should open the overlay, `false` otherwise | `true` |
| `grid:rows` | `Hyprlang::INT` | The number of rows to display on the grid overlay | `3` |
| `grid:cols` | `Hyprlang::INT` | The number of columns to display on the grid overlay | `3` |
| `grid:loop` | `Hyprlang::INT` | When enabled, moving right at the far right of the grid will wrap around to the leftmost workspace, etc. | `false` |
| `grid:gaps_use_aspect_ratio` | `Hyprlang::INT` | When enabled, vertical gaps will be scaled to match the monitor's aspect ratio | `false` |
| `linear:top` | `Hyprlang::INT` | Whether or not to position the overview on top of the screen | `false` |
| `linear:blur` | `Hyprlang::INT` | Whether or not to blur the dimmed area | `false` |
| `linear:height` | `Hyprlang::FLOAT` | The height of the linear overlay in logical pixels | `300.f` |
| `linear:scroll_speed` | `Hyprlang::FLOAT` | Scroll speed modifier. Set negative to flip direction | `1.f` |
