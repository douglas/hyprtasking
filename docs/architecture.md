# Architecture

Hyprtasking is intentionally narrow on the supported `0.54.3` and `0.55.0`
Hyprland targets.
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
registration is treated as a startup failure and is visible in
`hyprtasking:health json`.

## Reload Lifecycle

```mermaid
flowchart TD
    reload[Hyprland config reload]
    validate[Refresh and validate runtime config snapshot]
    trace[Refresh trace config]
    manager{Runtime enabled?}
    monitors[Sync monitor views]
    each[For each view]
    active{Runtime activity?}
    cancel[Cancel animations and runtime state]
    position[Reinitialize grid position]
    damage[Damage monitor and schedule frame]

    reload --> validate
    validate --> trace
    trace --> manager
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
Invalid safety-sensitive values, including grid dimensions, duplicate mouse
buttons, duplicate gesture finger counts, and non-positive gesture distances,
disable Hyprtasking for the session instead of trying to continue with
ambiguous input or render behavior.

## Release Gate

```mermaid
flowchart LR
    version[Version contract]
    compat[Compat audits]
    coverage[Compat coverage]
    boundary[Boundary audit]
    guidelines[Guideline audit]
    config[Config-key audit]
    build[Build]
    tests[Logic tests]
    live[Live smoke]
    manual[Manual checklist]

    version --> compat
    compat --> coverage
    coverage --> boundary
    boundary --> guidelines
    guidelines --> config
    config --> build
    build --> tests
    tests --> live
    live --> manual
```

`RELEASE_MODE=offline` runs the version-independent local stages and the
Hyprland source audits without touching a live compositor. The full mode adds
load/unload, dispatcher, live smoke, and manual checklist stages.
