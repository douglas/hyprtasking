# Architecture

Hyprtasking is intentionally narrow on the
[supported Hyprland targets](compat-contract.md).
The public surface is the overview itself, mouse interaction, gestures, the
core dispatchers, runtime health, and audited compat wrappers.

## Runtime Surface

```mermaid
flowchart LR
    config[Hyprland config]
    binds[Key binds and submap]
    gestures[Touchpad gestures]
    mouse[Mouse buttons]

    dispatchers[Plugin dispatchers]
    listeners[Event::bus listeners]
    input[Input hook]
    manager[HTManager]
    view[HTView]
    grid[HTLayoutGrid]
    compat[src/compat]
    hyprland[Hyprland runtime]

    config --> dispatchers
    binds --> dispatchers
    gestures --> listeners
    mouse --> input
    listeners --> manager
    input --> manager
    dispatchers --> manager
    manager --> view
    view --> grid
    manager --> compat
    view --> compat
    grid --> compat
    compat --> listeners
    compat --> input
    compat --> hyprland
```

Supported dispatchers:

- `hyprtasking:toggle`
- `hyprtasking:move`
- `hyprtasking:select`
- `hyprtasking:commit`
- `hyprtasking:health`

Bare arrow keys and Enter are supported only through the `hyprtasking` submap.
The plugin enters that submap when a view becomes interactively active and
returns to the default submap when all views leave that state.

Event::bus listeners are installed through `src/compat/runtime_compat.cpp` for
mouse movement, gestures, config reloads, and monitor changes. Missing listener
registration is treated as a startup failure and shows up in the
`disable_reason` and `failure_source` fields of `hyprtasking:health`.

## Reload Lifecycle

```mermaid
flowchart TD
    reload[Hyprland config reload]
    validate[Refresh and validate runtime config snapshot]
    manager{Runtime enabled?}
    monitors[Sync monitor views]
    each[For each view]
    active{Runtime activity?}
    cancel[Cancel animations and runtime state]
    position[Reinitialize grid position]
    damage[Damage monitor and schedule frame]

    reload --> validate
    validate --> manager
    manager -- no --> done[Stop]
    manager -- yes --> monitors
    monitors --> each
    each --> active
    active -- yes --> cancel
    active -- no --> position
    cancel --> position
    position --> damage
```

Reload behavior is deliberately not configurable. A reload always leaves each
view in a clean grid position, and any active, closing, or navigating overview
state is canceled before rendering continues.

Config is read into a typed runtime snapshot at startup, config reload, and
runtime readiness checks that can observe transient `hyprctl keyword` changes.
Invalid values are handled as described in the
[README](../README.md#config-options).

The release gate (`scripts/release-check.sh`) is described in the
[debugging playbook](debugging-playbook.md#7-release-gate).
