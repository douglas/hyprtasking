# Debugging Playbook

This is the maintainer playbook for diagnosing runtime failures in Hyprtasking.
It is written for the current supported Hyprland line, `0.54.x`, and is meant
to be followed in order.

The shortest useful mental model is:

1. version and binary contract
2. plugin registration and hook startup
3. input/render boundary correctness
4. overview state logic
5. focus and seat aftermath

If you classify the bug into the right layer early, fixes become much faster and
safer.

## 1. Establish the Baseline

Start every investigation by recording the current repo and runtime state.

```bash
git status --short
git log --oneline --decorate -5
hyprctl version
hyprctl plugin list
```

You want to know:

- which local changes are present
- whether the repo is already dirty before you begin
- which Hyprland runtime is actually running
- whether Hyprtasking is loaded once, multiple times, or not at all

If the plugin is not loaded, do not reason about live behavior yet.

## 2. Run the Fast Local Checks

Before live debugging, run the repo-local checks that catch architecture and
compat regressions cheaply.

```bash
meson compile -C build
meson test -C build logic-tests --print-errorlogs
bash scripts/audit-boundary.sh
```

Then, if the issue could involve version drift or Hyprland internals:

```bash
bash scripts/check-version-contract.sh
bash scripts/audit-compat.sh /path/to/Hyprland
bash scripts/audit-compat-surface.sh /path/to/Hyprland
```

Interpretation:

- compile or test failure -> stop and fix the repo first
- boundary failure -> move Hyprland internals back behind `src/compat/`
- version/compat audit failure -> treat as a Hyprland contract problem, not a
  normal feature bug

## 3. Prove the Live Binary

If the user says "same thing" after a patch, verify the loaded binary before
changing anything else.

Use:

```bash
hyprctl plugin list
lsof -p "$(pgrep -o Hyprland)" | rg 'hyprtasking|libhyprtasking'
```

If needed, reload explicitly:

```bash
hyprctl plugin unload /absolute/path/to/libhyprtasking.so
hyprctl plugin load /absolute/path/to/libhyprtasking.so
```

If the mapped plugin path is not the file you just built, every later runtime
observation is suspect.

Also use the smoke script when the problem might really be stale plugin state or
dispatcher registration:

```bash
bash scripts/smoke-live.sh load-unload
bash scripts/smoke-live.sh dispatchers
```

## 4. Classify the Symptom Before Choosing a Fix

### A. Dispatcher or plugin startup failure

Typical signs:

- `hyprctl dispatch hyprtasking:...` says `Invalid dispatcher`
- plugin list shows Hyprtasking, but its behavior is missing
- reload/unload/load leaves a stale or partially initialized instance

Use:

```bash
bash scripts/smoke-live.sh dispatchers
bash scripts/smoke-live.sh load-unload
hyprctl dispatch hyprtasking:health json
```

Start in:

- `src/plugin/dispatchers.cpp`
- `src/plugin/runtime.cpp`
- `src/compat/profile.cpp`

### B. Rendering, recursion, or overview self-disable failure

Typical signs:

- blank or partial overview
- plugin disables itself after open/reopen/reload
- crash or recursion-like flicker when reopening quickly

Use:

```bash
bash scripts/manual-runtime-check.sh render-reentry
bash scripts/smoke-live.sh reload-open
```

Start in:

- `src/compat/renderer_compat.cpp`
- `src/render.cpp`
- `src/overview.cpp`

### C. Input path failure

Typical signs:

- drag never starts
- right-click selection does nothing
- the same logic patch appears to have no visible effect
- trace values do not match actual button presses

Use the dedicated input workflow in the next section.

### D. State logic failure

Typical signs:

- the plugin sees the correct click/button data
- the wrong workspace is selected
- drag starts but drops on the wrong target
- hover selection or workspace resolution is incorrect

Start in:

- `src/input.cpp`
- `src/overview.cpp`
- `src/layout/`
- `src/logic/interaction_model.*`

### E. Focus or seat aftermath failure

Typical signs:

- workspace switches visually, but typing does not land immediately
- right-click causes client-side side effects
- focus is wrong after selection or move

Use:

```bash
bash scripts/manual-runtime-check.sh typing-focus
```

Start in:

- `src/compat/runtime_compat.cpp`
- `src/overview.cpp`
- `src/input.cpp`

## 5. Detailed Workflow for Click, Drag, and Selection Bugs

This is the workflow that fixed the overview input regression and should be the
default playbook for similar bugs.

### Step 1: Enable trace

```bash
hyprctl keyword plugin:hyprtasking:debug:trace 1
tail -f /tmp/hyprtasking-trace.log
```

Notes:

- this enables tracing for the currently loaded plugin instance
- if you want it to survive plugin unload/load or Hyprland restart, put
  `plugin:hyprtasking:debug:trace = 1` in config
- trace is intentionally file-backed because plugin logs may not surface
  reliably through `hyprctl rollinglog` on every setup

### Step 2: Reproduce one action at a time

Do not mix symptoms. Reproduce:

- one failed left-drag
- one failed right-click selection

Read the trace immediately after each action. You want the smallest possible
input sample.

### Step 3: Check whether the plugin sees real input

Look for:

- expected button values (`272`, `273`, or your configured alternatives)
- correct pressed/released state
- whether the plugin enters drag/select logic or leaves the event unhandled

If the trace is malformed or missing, do not patch overview logic yet.

### Step 4: Verify the compositor-side ground truth with `wev`

Run:

```bash
wev
```

Click inside the `wev` window and compare:

- `wev` values
- Hyprtasking trace values

Interpretation:

- `wev` wrong too -> input/session/device problem outside Hyprtasking
- `wev` correct, Hyprtasking wrong -> plugin boundary or hook-path problem
- both correct -> move up into overview state logic

### Step 5: Compare against a known-good commit

When a regression is suspected, compare behavior and hook path against a
known-good revision:

```bash
git show <known-good-commit>:src/main.cpp
git show <known-good-commit>:src/input.cpp
```

Do not compare only the drag/select helpers. Also compare:

- where mouse buttons are intercepted
- whether right-click selects on press or release
- which path is responsible for cancellation

### Step 6: Decide whether to patch compat or overview logic

Patch `src/compat/` or hook code when:

- the plugin receives wrong button data
- the event-bus path is missing or malformed
- Hyprland runtime contract drift changed how a listener path behaves

Patch overview/state logic when:

- the plugin receives correct button data
- preconditions or workspace resolution are wrong
- cancellation behavior is right but target selection or drag/drop logic is
  wrong

## 6. When to Patch `src/compat/`

Use `src/compat/` for:

- direct Hyprland hooks and original call-throughs
- runtime access that depends on Hyprland private or drift-prone APIs
- version-line assumptions
- logic that exists only to translate Hyprland runtime behavior into stable
  plugin behavior

Do not spread those decisions through unrelated plugin files. If a fix depends
on a Hyprland contract, keep it behind the compat boundary and update
`docs/compat-contract.md` with the new sensitive surface.

## 7. Manual Runtime Checks Worth Running

After a fix, use the script scenarios that match the changed subsystem:

```bash
bash scripts/manual-runtime-check.sh drag
bash scripts/manual-runtime-check.sh typing-focus
bash scripts/manual-runtime-check.sh movewindow
bash scripts/manual-runtime-check.sh render-reentry
bash scripts/manual-runtime-check.sh reload-open
```

General guidance:

- use `drag` after click/drag or workspace-target fixes
- use `typing-focus` after right-click or selection/focus fixes
- use `movewindow` after cursor/workspace alignment changes
- use `render-reentry` after hook or rendering fixes
- use `reload-open` after reload-safety changes

## 8. Release and Maintenance Workflow

For a single full maintenance gate:

```bash
bash scripts/release-check.sh
```

For an offline gate that skips live compositor/runtime stages:

```bash
RELEASE_MODE=offline HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh
```

For a release-prep run against a matching Hyprland source tree:

```bash
HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh
```

Use JSON output when integrating with tooling:

```bash
RELEASE_CHECK_FORMAT=json HYPRLAND_SOURCE=/path/to/Hyprland bash scripts/release-check.sh
```

The important point is not just to run the script, but to use its stage order:

1. version contract
2. compat audits
3. compat coverage audit
4. boundary audit
5. compile and tests
6. load/unload and dispatcher smoke
7. live smoke
8. manual checklist

That order makes it much easier to distinguish architecture drift from runtime
interaction regressions.

## 9. Common Failure Patterns

### "My patch is correct but behavior does not change"

Assume one of these first:

- wrong binary loaded
- stale plugin instance
- patched handler never received real input
- fix applied at the wrong layer

### "The overview looks wrong after a Hyprland update"

Assume compat drift first, not a random feature regression.

### "Right-click switches workspaces, but the client still reacts"

Investigate cancellation timing and hook level before changing workspace
selection logic.

### "Drag starts, but the window or cursor lands in the wrong place"

Investigate workspace resolution, pointer warp, and drag-controller state after
you confirm the button path is correct.

## 10. Bare Key Binding and Submap Considerations

Bare key bindings (bindings with no modifier like `bind = , RETURN, ...`) must
use a Hyprland submap to avoid intercepting keypresses during normal use. The
plugin enters the `hyprtasking` submap automatically via
`HTCompat::enter_submap()` when any view becomes interactively active
(`active && !closing`) and exits via `HTCompat::exit_submap()` when all views
leave that state.

If a user reports dropped keypresses outside the overview, check:

1. Whether their config has bare key bindings outside the submap block
2. Whether `hyprctl activesubmap` returns empty when the overview is closed
3. Whether an input method module is intercepting keypresses (see
   [`troubleshooting.md`](troubleshooting.md))

The submap enter/exit is in `src/overview.cpp` (`set_runtime_state`). The compat
helpers are in `src/compat/runtime_compat.cpp`.

## 11. Docs to Keep in Sync

When the debugging workflow or runtime contract changes, update these together:

- [`docs/compat-contract.md`](compat-contract.md)
- this playbook
- [`docs/troubleshooting.md`](troubleshooting.md) if user-facing diagnostic
  steps changed
- [`docs/overview-input-regression.md`](overview-input-regression.md) if the
  lesson materially changes
- README maintenance/config references if user-facing debugging knobs changed
