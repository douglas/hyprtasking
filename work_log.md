
## Feature: Harden hyprtasking stability and hyprland boundary
- **Status:** closed
- **Started:** 2026-05-11T22:13:52+00:00
- **Closed:** 2026-05-11T22:23:25+00:00
- **Description:** harden hyprtasking stability and hyprland boundary

### Summary
- Hardened Hyprland boundary handling by moving remaining direct
  `logicalBox()`, `getWindowMainSurfaceBox()`, `workspaceID()`, and
  `monitorID()` calls behind `src/compat/renderer_compat.*`.
- Centralized plugin config registration and runtime config reads in
  `src/config.cpp`, with `addConfigValueV2` for Hyprland 0.55.0 and legacy
  `addConfigValue` for 0.54.3.
- Added typed runtime config validation for grid dimensions, mouse-button
  conflicts, gesture finger conflicts, and gesture distances.
- Replaced raw hook cleanup with `HyprlandAPI::removeFunctionHook` and added
  RAII cleanup for renderer hint pass reset.
- Removed manual blur flag poking and kept the custom pass
  `needsPrecomputeBlur()` path.

### Benchmarks
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.55.0-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.55.0-src`
- `RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`
- `PRINT_MANUAL_FOLLOW_UP=0 bash scripts/smoke-live.sh all`
- `git diff --check`

### Kept Changes
- Kept current public dispatchers and config keys.
- Kept the solitary/fullscreen hook because overview grid rendering still
  needs fullscreen and solitary workspaces to be renderable.
- Kept runtime fail-closed behavior: invalid safety-sensitive config disables
  Hyprtasking for the session.
- Kept transient `hyprctl keyword` safety validation by refreshing the typed
  snapshot through central runtime readiness checks.

### Rejected Attempts
- Tried making safety validation reload-only, but live smoke showed
  `hyprctl keyword` values are transient and `hyprctl reload` restores file
  config before the invalid value can be tested.

### Lessons Learned
- Hyprland runtime keywords need central validation at runtime readiness
  checks, even with a typed config snapshot, because they are not equivalent
  to config reloads.
- The custom overview pass `needsPrecomputeBlur()` is enough for the current
  automated and live smoke coverage; direct blur flag mutation is no longer
  needed.

### Durable Docs Updated
- `README.md`
- `docs/architecture.md`
- `docs/compat-contract.md`

### Open Follow-ups
- Manual eyes-on checks remain for fullscreen-window grid open/close,
  physical mouse drag/drop, right-click select, keyboard select/commit, and
  physical three-/four-finger touchpad gestures.

## Feature: Audit scripts and docs for hyprtasking hardening changes
- **Status:** closed
- **Started:** 2026-05-11T22:36:59+00:00
- **Closed:** 2026-05-11T22:41:33+00:00
- **Description:** audit scripts and docs for hyprtasking hardening changes

### Summary
- Brought the audit scripts up to date with the hardening work by covering the
  newly wrapped Hyprland methods and hook-removal API.
- Extended the supported-Hyprland update path so it runs guideline and
  config-key audits along with compat, compat-surface, compat-coverage, and
  boundary checks.
- Expanded live safety smoke coverage for open gesture distance, mouse button
  conflicts, and gesture finger conflicts.
- Updated README and docs to match the current audit/release workflow and the
  remaining hook contract.

### Benchmarks
- `bash -n scripts/*.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/update-supported-hyprland.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/update-supported-hyprland.sh /tmp/hyprland-v0.55.0-src`
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `RELEASE_MODE=offline HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`
- `PRINT_MANUAL_FOLLOW_UP=0 bash scripts/smoke-live.sh safety`
- `PRINT_MANUAL_FOLLOW_UP=0 bash scripts/smoke-live.sh all`
- `git diff --check`

### Kept Changes
- Kept generated compat-surface contracts in sync with the manifest after
  adding the hook-removal contract.
- Kept script coverage focused on drift-prone Hyprland APIs and repo-local
  release gates instead of broad shell lint churn.
- Kept docs scoped to workflows that changed: README, compat contract,
  supported-version update docs, and the debugging playbook.

### Rejected Attempts
- The first expanded live safety smoke assumed safe defaults after reload and
  failed on the gesture-finger conflict probe. The smoke now restores safe
  options before probing and sets both sides of each conflict explicitly.

### Lessons Learned
- Runtime keyword values can reflect the user session, so safety smoke needs
  to establish its own known-safe baseline before checking invalid cases.
- `update-supported-hyprland.sh` is the right place to keep release-adjacent
  script coverage aligned as more audits become mandatory.

### Durable Docs Updated
- `README.md`
- `docs/compat-contract.md`
- `docs/debugging-playbook.md`
- `docs/supported-hyprland.md`

### Open Follow-ups
- None from the script/doc audit. Manual input and fullscreen checks remain
  tracked by the hardening feature's follow-up list.

## Feature: Hyprtasking stability hardening
- **Status:** closed
- **Started:** 2026-05-12T00:29:40+00:00
- **Closed:** 2026-05-12T02:44:32+00:00
- **Description:** hyprtasking stability hardening

### Summary
- Trimmed `hyprpm.toml` to the active supported targets only and added the
  Hyprland `v0.55.0` release pin.
- Removed the Hyprland 0.55 runtime config read path through deprecated
  `HyprlandAPI::getConfigValue` by keeping owned `addConfigValueV2` handles.
- Made function lookup fail closed unless `findFunctionsByName` returns the
  exact expected audited signature.
- Added x86_64 hook-architecture gating in Meson and the runtime compatibility
  check.
- Moved render border color config reads into `src/compat/renderer_compat.*`
  with fallback colors and extended the boundary audit to prevent future direct
  `CConfigValue` use outside compat.
- Added `override` to the custom pass element and fixed the Hyprland 0.55 pass
  `draw()` signature exposed by that check.
- Made `release-check.sh` fall back to `build-055` when the canonical `build`
  directory is absent.
- Added a `hook-sunset` manual checklist scenario and docs so mouse-hook
  necessity is re-tested per supported release.

### Benchmarks
- `git clang-format --diff HEAD -- src/config.cpp src/config.hpp src/compat/profile.cpp src/compat/renderer_compat.cpp src/compat/renderer_compat.hpp src/layout/grid.cpp src/pass/pass_element.cpp src/pass/pass_element.hpp`
- `bash -n scripts/*.sh`
- `git diff --check`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash scripts/manual-runtime-check.sh hook-sunset`
- `CHECKLIST_FORMAT=json bash scripts/manual-runtime-check.sh hook-sunset`
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`

### Kept Changes
- Kept the direct mouse-button hook, but added an explicit release checklist
  to re-test whether the event-bus path can replace it on every supported
  target.
- Kept legacy `HyprlandAPI::getConfigValue` only in the Hyprland 0.54 compile
  branch.
- Kept the canonical `build` directory as the release-check default when it
  exists, with `build-055` only as a local fallback.

### Rejected Attempts
- `clang-format --dry-run` on whole touched files reported broad existing
  style drift, so changed-line formatting was checked with `git clang-format`
  instead.

### Lessons Learned
- Adding `override` to the pass element immediately caught the 0.55
  `IPassElement::draw()` signature change.
- `scripts/audit-config-keys.sh` needed to follow the new config reader names
  after the 0.55 config handle refactor.

### Durable Docs Updated
- `README.md`
- `docs/compat-contract.md`
- `docs/maintenance-tracks.md`
- `docs/supported-hyprland.md`

### Open Follow-ups
- Manual hook-sunset testing still needs to be performed on live Hyprland
  `0.54.3` and `0.55.0` sessions before a release that changes mouse hook
  behavior.

## Feature: Hyprtasking stability reference hardening
- **Status:** closed
- **Started:** 2026-05-12T17:36:16+00:00
- **Closed:** 2026-05-12T17:51:18+00:00
- **Description:** hyprtasking stability reference hardening

### Summary
- Hardened function-hook installation and teardown behind `src/compat/profile.*`
  so hook setup now verifies exact symbol matches, refuses duplicate installs,
  checks original call-through, and removes failed hooks before continuing.
- Added runtime fail-closed paths for missing mouse/render originals, invalid
  grid dimensions, invalid monitor geometry, and invalid render scale.
- Reduced drift-prone Hyprland includes outside `src/compat/` and expanded
  audits for deprecated plugin APIs and implementation include boundaries.
- Added hook method/signature diagnostics to `hyprtasking:health`.

### Benchmarks
- `clang-format --dry-run --Werror ...` on touched C++ files.
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash -n` on touched shell scripts.
- `git diff --check`
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`
- `bash scripts/manual-runtime-check.sh hook-sunset`
- `CHECKLIST_FORMAT=json bash scripts/manual-runtime-check.sh hook-sunset`

### Kept Changes
- Direct mouse hook remains installed because the cancellable event-bus-only
  replacement still needs live validation on every supported Hyprland target.
- Deprecated `getFunctionAddressFromSignature` remains only as an audited
  emergency fallback after `findFunctionsByName` has no matches.
- Config `getConfigValue` / `addConfigValue` compatibility remains isolated to
  the 0.54-era config registration path.

### Rejected Attempts
- Did not remove the direct mouse-button hook without live hook-sunset evidence.
- Did not add compatibility shims for mismatched symbol lookup results; exact
  lookup mismatches now fail closed instead of falling back to looser matching.

### Lessons Learned
- Hook creation/removal is safer as one audited compat primitive than repeated
  runtime-specific setup code.
- `findFunctionsByName` should be treated as authoritative when it finds a
  symbol family but not the expected exact signature.
- Grid/render validation should disable the plugin before Hyprland sees invalid
  geometry or scale values.

### Durable Docs Updated
- `docs/compat-contract.md`

### Open Follow-ups
- Manual hook-sunset testing still needs a live 0.54.3 and 0.55.0 session before
  removing the direct mouse-button hook.

## Feature: Hyprtasking reference stability cleanup
- **Status:** closed
- **Started:** 2026-05-12T18:30:51+00:00
- **Closed:** 2026-05-12T18:36:39+00:00
- **Description:** hyprtasking reference stability cleanup

### Summary
- Removed the deprecated `getFunctionAddressFromSignature` hook fallback from
  source, generated contracts, docs, and compat coverage token tracking.
- Kept hook resolution on `findFunctionsByName` with exact signature matching
  and fail-closed behavior when symbols are missing or ambiguous.
- Hardened remaining hook original call-throughs for `shouldRenderWindow` and
  `isSolitaryBlocked` with null/original checks and Hyprland-preserving
  fallbacks.
- Removed duplicate `isSolitaryBlocked` lookup during renderer hook setup.
- Updated CI matrices and `hyprpm.toml` pins to the supported `0.54.3` /
  `0.55.0` surface and latest published main commit.
- Expanded guideline audits for hook original access, stale workflow refs, and
  stale/missing hyprpm support pins.

### Benchmarks
- `clang-format --dry-run --Werror ...` on touched C++ files.
- `bash -n` on touched shell scripts.
- `git diff --check`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.55.0-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.55.0-src`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`
- `bash scripts/manual-runtime-check.sh hook-sunset`
- `CHECKLIST_FORMAT=json bash scripts/manual-runtime-check.sh hook-sunset`

### Kept Changes
- Direct mouse hook remains installed until the event-bus-only branch passes
  live hook-sunset testing on every supported target.
- Legacy `getConfigValue` / `addConfigValue` remains allowed only for the
  audited 0.54 config compatibility path.

### Rejected Attempts
- Did not keep deprecated symbol fallback as a rescue path; missing hook symbols
  now disable runtime instead of guessing.
- Did not remove the mouse hook without live evidence.

### Lessons Learned
- Exact `findFunctionsByName` matches are enough for the supported targets and
  make failure mode clearer than deprecated signature fallback.
- CI and hyprpm metadata need the same support-matrix discipline as runtime
  gates, otherwise release tooling drifts even when source code is hardened.

### Durable Docs Updated
- `README.md`
- `docs/compat-contract.md`
- `docs/maintenance-tracks.md`
- `scripts/contracts/core.generated.tsv`

### Open Follow-ups
- Manual hook-sunset testing still needs a live 0.54.3 and 0.55.0 session before
  removing the direct mouse-button hook.

## Feature: Hyprtasking listener lifecycle stability hardening
- **Status:** closed
- **Started:** 2026-05-12T22:04:43+00:00
- **Closed:** 2026-05-12T22:30:12+00:00
- **Description:** hyprtasking listener lifecycle stability hardening

### Summary
- Made Event::bus listener installation status-returning and fail-closed during
  startup if any required listener cannot be registered.
- Added listener readiness diagnostics to `hyprtasking:health json` and the
  plain health output.
- Changed deferred callbacks to fail closed when Hyprland's event loop manager
  is unavailable instead of invoking callbacks synchronously.
- Made submap enter/exit status-returning and disabled the runtime if
  interactive state cannot enter or leave the `hyprtasking` submap.
- Centralized runtime cleanup so disable/reset/monitor-removal paths unwind
  render guards, selection, claimed mouse buttons, drag state, swipe state,
  submap state, and cursor override.
- Added a guideline audit guard against reintroducing synchronous delayed
  callback fallbacks.
- Updated health/troubleshooting docs and removed the stale fallback-signature
  wording from the compat contract.

### Benchmarks
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash -n scripts/*.sh`
- `git diff --check`
- `git clang-format --diff HEAD -- src/compat/runtime_compat.cpp src/compat/runtime_compat.hpp src/globals.hpp src/plugin/runtime.cpp src/overview.cpp src/manager.cpp src/manager.hpp`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.55.0-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.55.0-src`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`
- `bash scripts/manual-runtime-check.sh hook-sunset`
- `CHECKLIST_FORMAT=json bash scripts/manual-runtime-check.sh hook-sunset`

### Kept Changes
- Kept the direct mouse-button hook because this phase did not perform the
  required live event-bus-only hook-sunset validation.
- Kept existing dispatcher/config public surfaces unchanged.
- Kept listener readiness as health diagnostics and startup gating rather than
  adding new user-facing configuration.

### Rejected Attempts
- Full-file `clang-format --dry-run` still reports pre-existing style drift in
  `src/manager.hpp`; changed-line formatting was validated with
  `git clang-format --diff` instead.

### Lessons Learned
- Listener registration was previously silent on Event::bus absence; treating
  that as a startup failure gives a clearer and safer failure mode.
- The delayed-callback helper should never run callbacks synchronously, because
  that can re-enter runtime state from hook/callback paths.

### Durable Docs Updated
- `README.md`
- `docs/compat-contract.md`
- `docs/troubleshooting.md`

### Open Follow-ups
- Manual hook-sunset testing still needs a live 0.54.3 and 0.55.0 session before
  removing the direct mouse-button hook.

## Feature: Document Event bus listener lifecycle in Hyprtasking diagrams
- **Status:** closed
- **Started:** 2026-05-12T23:49:11+00:00
- **Closed:** 2026-05-12T23:49:36+00:00
- **Description:** document Event bus listener lifecycle in Hyprtasking diagrams

### Summary
- Updated the README and architecture runtime diagrams to show the Event::bus
  listener path alongside the direct mouse hook.
- Documented that Event::bus listener registration is fail-closed and reported
  by `hyprtasking:health json`.

### Benchmarks
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-boundary.sh`
- `git diff --check`

### Kept Changes
- Kept the diagrams high-level; they now show dispatchers, Event::bus listeners,
  the direct mouse hook, manager/view/layout, and compat ownership without
  expanding into every individual listener.

### Rejected Attempts
- None.

### Lessons Learned
- The prior diagrams were broadly correct but did not reflect the new
  listener-lifecycle hardening as a first-class runtime path.

### Durable Docs Updated
- `README.md`
- `docs/architecture.md`

### Open Follow-ups
- None.

## Feature: Rewrite Hyprtasking release commits since last tag
- **Status:** closed
- **Started:** 2026-05-13T00:40:47+00:00
- **Closed:** 2026-05-13T02:30:17+00:00
- **Description:** rewrite Hyprtasking release commits since last tag

### Summary
- Rewrote the post-`v0.4` release history into three release-focused commits:
  runtime implementation, release compatibility gates, and docs/work-log.
- Included the uncommitted README and architecture diagram updates in the docs
  layer so the rewritten branch has no leftover local-only release notes.
- Preserved a backup ref before rewriting: `backup/release-rewrite-prep-c8a023a`.

### Benchmarks
- `git diff --cached --check` for each rewritten commit layer
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash -n scripts/*.sh`
- `git diff --check`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.55.0-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.55.0-src`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`

### Kept Changes
- Kept the direct mouse-button hook and documented hook-sunset as the remaining
  live validation requirement before removal.
- Kept release history split by reviewer intent rather than by every
  incremental implementation step.

### Rejected Attempts
- Did not force-push the rewritten history during the rewrite phase.

### Lessons Learned
- The old post-tag history mixed implementation, release gates, docs, and
  follow-up hardening; a three-commit release stack is easier to review and tag.

### Durable Docs Updated
- `README.md`
- `docs/architecture.md`
- `docs/compat-contract.md`
- `docs/debugging-playbook.md`
- `docs/maintenance-tracks.md`
- `docs/overview-input-regression.md`
- `docs/supported-hyprland.md`
- `docs/troubleshooting.md`
- `work_log.md`

### Open Follow-ups
- Run the full release gate on the rewritten history before tagging.
- Force-push only after explicitly deciding to replace the remote post-`v0.4`
  history.

## Feature: Prepare Hyprtasking 0.5 release
- **Status:** closed
- **Started:** 2026-05-13T02:35:02+00:00
- **Closed:** 2026-05-13T02:36:33+00:00
- **Description:** prepare Hyprtasking 0.5 release

### Summary
- Performed a final README/docs/diagram drift check for the `0.5` release.
- Added `CHANGELOG.md` with the release summary since `v0.4`.
- Bumped the Meson project version to `0.5` before tagging.

### Benchmarks
- `meson compile -C build`
- `meson test -C build --print-errorlogs`
- `meson compile -C build-055`
- `meson test -C build-055 --print-errorlogs`
- `bash scripts/audit-boundary.sh`
- `bash scripts/audit-guidelines.sh`
- `bash scripts/audit-config-keys.sh`
- `bash scripts/audit-compat-coverage.sh`
- `bash -n scripts/*.sh`
- `git diff --check`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat.sh /tmp/hyprland-v0.55.0-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.54.3-src`
- `bash scripts/audit-compat-surface.sh /tmp/hyprland-v0.55.0-src`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.54.3-src bash scripts/release-check.sh`
- `BUILD_DIR=/home/douglas/src/hyprtasking/build-055 RELEASE_MODE=offline PRINT_MANUAL_CHECKLIST=0 HYPRLAND_SOURCE=/tmp/hyprland-v0.55.0-src bash scripts/release-check.sh`

### Kept Changes
- Kept old dispatcher names only in migration guidance.
- Kept the direct mouse-button hook documented as intentionally retained until
  hook-sunset live testing passes across supported targets.

### Rejected Attempts
- None.

### Lessons Learned
- The docs, README, and diagrams were aligned after the Event::bus diagram
  update; `CHANGELOG.md` was the missing release artifact.

### Durable Docs Updated
- `CHANGELOG.md`
- `work_log.md`

### Open Follow-ups
- Force-push rewritten `main` and push `v0.5` after final validation.
