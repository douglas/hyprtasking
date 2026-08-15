# Troubleshooting

User-facing guide for diagnosing input problems that can appear alongside
Hyprtasking or on Wayland compositors in general.

## Keys need to be pressed twice

If keypresses are dropped or need a second press to register, two common causes
are documented below. They can occur independently or together.

### Cause 1: Bare key bindings without a submap

**Symptom:** After opening and closing the overview, the next keypress in the
focused application is swallowed. For example, pressing Enter in a menu does
nothing on the first press but works on the second.

**Why it happens:** Hyprland's keybind system intercepts every press of a bare
key binding (a binding with no modifier). Even when the dispatcher returns
`passEvent = true` to forward the event, the intercept-then-reinject round trip
causes the next keypress to be dropped.

**Fix:** Move bare key bindings into a Hyprland submap. The plugin enters the
`hyprtasking` submap automatically when the overview opens and exits it when the
overview closes, so bare keys are only bound while the overview is visible.

Correct configuration:

```
submap = hyprtasking
bind = , LEFT, hyprtasking:select, left
bind = , RIGHT, hyprtasking:select, right
bind = , UP, hyprtasking:select, up
bind = , DOWN, hyprtasking:select, down
bind = , RETURN, hyprtasking:commit,
bind = , ESCAPE, hyprtasking:toggle, cursor
submap = reset
```

**Quick diagnostic:** Temporarily unbind the suspected bare key and test:

```bash
hyprctl keyword unbind , RETURN
```

If the double-press problem disappears, the bare binding was the cause.

**Verifying the submap is active:**

```bash
# Check which submap is currently active
hyprctl activesubmap
```

When the overview is open, this should show `hyprtasking`. When the overview is
closed, it should show the default submap (empty or your normal submap).

If `hyprctl activesubmap` is unavailable on your runtime, verify that your bind
table contains exactly one `hyprtasking` submap block:

```bash
hyprctl binds | rg 'submap: hyprtasking|hyprtasking:select|hyprtasking:commit'
```

## `hyprctl` says it cannot connect to `.socket.sock`

If `hyprctl` prints `Couldn't connect to .../.socket.sock`, your shell is
usually targeting a stale Hyprland instance signature.

Set `HYPRLAND_INSTANCE_SIGNATURE` from the newest live socket:

```bash
unset HYPRLAND_INSTANCE_SIGNATURE
export HYPRLAND_INSTANCE_SIGNATURE="$(
  ls -1t /run/user/$UID/hypr/*/.socket.sock \
  | head -n1 \
  | xargs dirname \
  | xargs basename
)"
hyprctl instances
```

After this, retry your command (for example `hyprctl eval 'hl.plugin.hyprtasking.health()'` on Hyprland 0.56+).

### Cause 2: fcitx5 in-process input method module

**Symptom:** The first keypress after switching window focus is dropped,
especially in GTK applications. This happens even when the overview is not
involved.

**Why it happens:** Setting `GTK_IM_MODULE=fcitx` loads the fcitx5 input method
module directly into each GTK application's process. When focus changes, the
in-process module re-establishes its connection state and can consume the first
keypress during that handshake.

**Fix:** Remove `GTK_IM_MODULE=fcitx` (and `QT_IM_MODULE=fcitx`) from your
environment. On Wayland, GTK and Qt applications communicate with fcitx5
through the compositor's text-input-v3 protocol, which does not have this
focus-change interception problem.

Keep only:

```
env = XMODIFIERS,@im=fcitx
env = SDL_IM_MODULE,fcitx
```

Check all places where input method environment variables might be set:

- Hyprland config files (`hyprland.conf`, sourced files)
- `~/.config/environment.d/*.conf` (systemd user environment)
- Shell profile files (`~/.zshenv`, `~/.bashrc`, `~/.profile`)

**Quick diagnostic:** Kill fcitx5 temporarily and test:

```bash
killall fcitx5
```

If the double-press problem disappears, fcitx5's in-process module was the
cause. After confirming, restart fcitx5 and remove the problematic env vars
instead of keeping it killed.

**References:**

- Mozilla Bug 1742039: text-input-v3 interaction with fcitx5
- Hyprland Discussion #10726: fcitx5 keypress issues on Wayland
- fcitx5 Issue #954: first keypress dropped after focus change

## Overview keyboard navigation does not work

If arrow keys and Enter do not navigate while the overview is open:

1. Verify the submap bindings are in your config (see the submap block above)
2. Verify the plugin is loaded: `hyprctl plugin list`
3. Check that the submap activates: open the overview, then run
   `hyprctl activesubmap` — it should say `hyprtasking`
4. If the submap does not activate, reload the plugin binary:

```bash
hyprctl plugin unload /path/to/libhyprtasking.so
hyprctl plugin load /path/to/libhyprtasking.so
```

`hyprctl reload` only reloads the config, not the plugin binary. If you
rebuilt the plugin, you must unload and reload it.

If arrow navigation skips multiple workspaces per keypress (for example
`1 -> 3` on one `RIGHT` press), your `hyprtasking` binds are likely duplicated.
This usually happens when a config uses repeated runtime sourcing such as:

```bash
exec = hyprctl keyword source /path/to/hyprtasking-binds.conf
```

Use a normal `source = ...` include, or `exec-once = ...` if runtime sourcing
is required.

## `Invalid dispatcher` when sourcing binds

If `hyprctl keyword source ~/.config/hypr/personal-hyprtasking.conf` reports
`invalid dispatcher`, your loaded plugin binary and binding profile likely do
not match.

Quick checks:

```bash
hyprctl plugin list
hyprctl keyword source ~/.config/hypr/personal-hyprtasking.conf
```

On this branch, keyboard selection dispatchers are `hyprtasking:select` and
`hyprtasking:commit`. If your config still uses
`hyprtasking:navigate` or `hyprtasking:commit_selection`, switch them to
`hyprtasking:select` and `hyprtasking:commit`.
If your config used conditional wrapper dispatchers to guard bare arrow keys,
replace that block with the `hyprtasking` submap shown above.

If these are mixed, update either the loaded plugin or the bind profile.
Use the "Binding recipes" snippets in `README.md` to switch safely.

## Gestures open the overview but also move workspaces

If a swipe both toggles the overview and moves workspace selection, your
gesture finger-count settings likely overlap.

Recommended split:

- `plugin:hyprtasking:gestures:open_fingers = 3`
- `plugin:hyprtasking:gestures:move_fingers = 4`

Or invert them if you prefer 4-finger open/close and 3-finger move.

Check active values:

```bash
hyprctl getoption plugin:hyprtasking:gestures:open_fingers
hyprctl getoption plugin:hyprtasking:gestures:move_fingers
hyprctl getoption plugin:hyprtasking:gestures:open_positive
```

If values are correct but behavior is stale after rebuild, unload/reload the
plugin binary; `hyprctl reload` alone does not reload the `.so`.

## Overview closes during config reload

This is expected. Config reload always cancels active overview state and
reinitializes grid positions so the compositor does not keep a stale half-open
overview after options change. Reopen the overview after the reload completes.

## Plugin disables itself at runtime

If you see the notification "Disabled for this session after an internal runtime
failure", check the Hyprland log for the `[Hyprtasking] runtime disabled`
message which includes the reason and a state snapshot.

```bash
hyprctl eval 'hl.plugin.hyprtasking.health("json")'
```

This returns the current runtime health state including whether the plugin is
enabled, how many views are active, which hooks are registered, and which
Event::bus listeners installed successfully.

Common causes:

- Hyprland version mismatch (check with `bash scripts/check-version-contract.sh`)
- Plugin loaded multiple times (check with `hyprctl plugin list`)
- Invalid safety-sensitive config values (for example `grid:rows = 0`,
  `grid:cols = 0`, or `gestures:move_distance = 0`)

If the disable reason reports invalid config values, fix the options and reload
the plugin binary:

```bash
hyprctl plugin unload /path/to/libhyprtasking.so
hyprctl plugin load /path/to/libhyprtasking.so
```

See [`debugging-playbook.md`](debugging-playbook.md) for the full maintainer
debugging workflow.

## Plugin stopped working after Hyprland update

If Hyprland was updated and Hyprtasking suddenly disables itself or fails to
load, the package version and runtime likely drifted out of the supported set.

1. Check package/runtime compatibility:
   `bash scripts/check-version-contract.sh`
2. Check runtime state and disable reason:
   `hyprctl eval 'hl.plugin.hyprtasking.health("json")'`
3. Rebuild/reload the plugin against the currently installed headers:
   `meson setup build --reconfigure --buildtype=release && ninja -C build`
4. Reload the plugin binary (not just config):
   `hyprctl plugin unload /path/to/libhyprtasking.so` then
   `hyprctl plugin load /path/to/libhyprtasking.so`

## Brief border blink during close transition

Some setups show a very short border-color flicker when closing the overview,
especially when switching between empty and non-empty workspaces.

This behavior is also reproducible in the baseline `hyprtasking-yerlotic` v0.1
build, so it is not a regression introduced by the current branch.

## Related docs

- [`debugging-playbook.md`](debugging-playbook.md): maintainer debugging workflow
- [`overview-input-regression.md`](overview-input-regression.md): lessons from
  the overview click/drag regression
