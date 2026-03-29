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
bind = , LEFT, hyprtasking:navigate, left
bind = , RIGHT, hyprtasking:navigate, right
bind = , UP, hyprtasking:navigate, up
bind = , DOWN, hyprtasking:navigate, down
bind = , RETURN, hyprtasking:commit_selection,
bind = , ESCAPE, hyprtasking:toggle, cursor
submap = reset
```

The older `hyprtasking:if_active` pattern for bare keys is still supported but
causes this problem. If your config uses `bind = , RETURN, hyprtasking:if_active, ...`
or similar, migrate to the submap approach above.

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

## Plugin disables itself at runtime

If you see the notification "Disabled for this session after an internal runtime
failure", check the Hyprland log for the `[Hyprtasking] runtime disabled`
message which includes the reason and a state snapshot.

```bash
hyprctl dispatch hyprtasking:health json
```

This returns the current runtime health state including whether the plugin is
enabled, how many views are active, and which hooks are registered.

Common causes:

- Hyprland version mismatch (check with `bash scripts/check-version-contract.sh`)
- Plugin loaded multiple times (check with `hyprctl plugin list`)

See [`debugging-playbook.md`](debugging-playbook.md) for the full maintainer
debugging workflow.

## Related docs

- [`debugging-playbook.md`](debugging-playbook.md): maintainer debugging workflow
- [`overview-input-regression.md`](overview-input-regression.md): lessons from
  the overview click/drag regression
