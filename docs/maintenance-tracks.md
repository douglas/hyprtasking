# Maintenance Tracks

Hyprtasking uses two tracks to keep updates predictable while protecting runtime stability:

1. `main` (stable): current supported Hyprland line (`0.54.x`).
2. `preview/hyprland-next` (preview): next Hyprland line adaptation work.

## Operating Rules

- Stable stays fail-closed: if runtime checks fail, plugin runtime is disabled for the session.
- Preview is where compat drift gets patched first.
- Stable only receives promoted preview changes after:
  - `scripts/audit-compat.sh` passes
  - `scripts/audit-compat-surface.sh` passes
  - `scripts/audit-compat-coverage.sh` passes
  - `scripts/audit-boundary.sh` passes
  - release checks are clean in the intended mode

## CI Roles

- `.github/workflows/offline-compat-gate.yml`
  - runs on PRs and pushes to `main`
  - enforces shell syntax checks + offline compat audits
- `.github/workflows/nightly-compat-preview.yml`
  - runs nightly on stable and preview Hyprland refs
  - publishes JSON artifacts for drift triage
  - fails strictly on stable drift, reports preview drift without blocking stable
