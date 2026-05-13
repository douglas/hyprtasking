# Maintenance Tracks

Hyprtasking uses two tracks to keep updates predictable while protecting runtime stability:

1. `main` (stable): current supported Hyprland targets (`0.54.3` and `0.55.0`).
2. `preview/hyprland-next` (preview): future Hyprland adaptation work.

## Operating Rules

- Stable stays fail-closed: if runtime checks fail, plugin runtime is disabled for the session.
- Stable documentation describes only the active exact support targets.
- Preview is where compat drift gets patched first.
- Stable only receives promoted preview changes after:
  - `scripts/audit-compat.sh` passes
  - `scripts/audit-compat-surface.sh` passes
  - `scripts/audit-compat-coverage.sh` passes
  - `scripts/audit-boundary.sh` passes
  - `scripts/manual-runtime-check.sh hook-sunset` has been reviewed for each
    supported target when mouse hook behavior is touched
  - release checks are clean in the intended mode

## CI Roles

- `.github/workflows/offline-compat-gate.yml`
  - runs on PRs and pushes to `main`
  - enforces shell syntax checks + offline compat audits
  - regenerates contract baselines on the latest stable matrix target and fails on drift
- `.github/workflows/nightly-compat-preview.yml`
  - runs nightly on stable and preview Hyprland refs
  - publishes JSON artifacts for drift triage
  - fails strictly on supported-version drift (including contract baseline drift), reports preview drift without blocking stable
