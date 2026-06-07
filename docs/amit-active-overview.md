# Amit's Active Workspace Overview

This fork of Douglas Hyprtasking adds an active-only workspace overview mode
that behaves like the old hyprexpo dynamic grid.

## What It Does

- `SUPER + code:49` (physical backtick key) toggles overview
- Only active or non-empty workspaces are shown
- Grid shape is compact and dynamic (not fixed 3×3)
- Partial final rows are centered
- Tiles preserve monitor aspect ratio

## Config

All features are opt-in (default off for upstream compatibility):

```
plugin {
    hyprtasking {
        active_only = 1
        grid:auto = 1
        grid:center_partial_rows = 1
        show_labels = 1
        wallpaper_bg = 1
    }
}

bindd = SUPER, code:49, Window overview, hyprtasking:toggle, cursor
```

| Option | Default | Description |
|--------|---------|-------------|
| `active_only` | `0` | Show only active/non-empty workspaces |
| `grid:auto` | `0` | Compute compact grid dynamically |
| `grid:center_partial_rows` | `0` | Center the final partial row |
| `show_labels` | `0` | Show workspace number labels on tiles |
| `wallpaper_bg` | `0` | Use wallpaper as overview background |

## Runtime-Only Bind (No Persistent Config Edit)

```bash
hyprctl keyword unbind "SUPER, grave" || true
hyprctl keyword unbind "SUPER, code:49" || true
hyprctl keyword bindd "SUPER, code:49, Window overview, hyprtasking:toggle, cursor"
hyprctl keyword submap hyprtasking
for n in 1 2 3 4 5 6 7 8 9; do
    hyprctl keyword bind ", $n, hyprtasking:select-commit, $n"
done
hyprctl keyword bind ", ESCAPE, hyprtasking:toggle, cursor"
hyprctl keyword submap reset
```

## Navigation Submap

```
submap = hyprtasking
bind = , LEFT, hyprtasking:select, left
bind = , RIGHT, hyprtasking:select, right
bind = , UP, hyprtasking:select, up
bind = , DOWN, hyprtasking:select, down
bind = , RETURN, hyprtasking:commit,
bind = , 1, hyprtasking:select-commit, 1
bind = , 2, hyprtasking:select-commit, 2
bind = , 3, hyprtasking:select-commit, 3
bind = , 4, hyprtasking:select-commit, 4
bind = , 5, hyprtasking:select-commit, 5
bind = , 6, hyprtasking:select-commit, 6
bind = , 7, hyprtasking:select-commit, 7
bind = , 8, hyprtasking:select-commit, 8
bind = , 9, hyprtasking:select-commit, 9
bind = , ESCAPE, hyprtasking:toggle, cursor
submap = reset
```

## Disable Active Overview

```
plugin {
    hyprtasking {
        active_only = 0
        grid:auto = 0
    }
}
```

This restores the original Douglas fixed-grid behavior.
