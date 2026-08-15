# Supported Hyprland Versions

Hyprtasking hard-gates runtime activation to explicit, audited versions.

| Version | Status | CI coverage |
| --- | --- | --- |
| `0.54.3` | stable baseline | offline compat gate + generated-contract drift check |
| `0.55.x` | supported minor family | offline compat gate + generated-contract drift check; local runtime rebuild smoke checked on `0.55.2` |
| `0.56.0` | supported | offline compat gate + generated-contract drift check |
| `0.56.2` | supported | offline compat gate + generated-contract drift check; live rendering smoke checked |

## Update Workflow

Use one command when testing a target Hyprland checkout:

```bash
bash scripts/update-supported-hyprland.sh /path/to/Hyprland
```

That command regenerates compat baselines, runs compat, compat-surface,
compat-coverage, boundary, guideline, and config-key audits, and runs
`logic-tests` when a local `build/` directory exists.

Run the offline release gate against both supported source trees before
promoting a compatibility change:

```bash
RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh
RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh
```

For release work that touches input handling or hook policy, also run the hook
sunset checklist once per supported target:

```bash
bash scripts/manual-runtime-check.sh hook-sunset
```
