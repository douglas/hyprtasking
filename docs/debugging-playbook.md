# Debugging Playbook

This is the maintainer playbook for diagnosing runtime failures in Hyprtasking
on the [supported Hyprland targets](compat-contract.md). Follow it in order.

The shortest useful mental model is:

1. version and binary contract
2. plugin registration and hook startup
3. input/render boundary correctness
4. overview state logic
5. focus and seat aftermath

Classify the bug into the right layer early; fixes get faster and safer. The
runtime shape is diagrammed in [`architecture.md`](architecture.md).

## 1. Establish the Baseline

```bash
git status --short
git log --oneline --decorate -5
hyprctl version
hyprctl plugin list
hyprctl eval 'hl.plugin.hyprtasking.health()'
```

You want to know which local changes are present, which Hyprland is actually
running, and whether Hyprtasking is loaded once, several times, or not at all.
If the plugin is not loaded, do not reason about live behavior yet.

The health call prints the one-line summary described in the
[README](../README.md#dispatchers).

## 2. Run the Fast Local Checks

```bash
meson compile -C build
meson test -C build logic-tests --print-errorlogs
bash scripts/audit-source.sh
```

If the issue could involve version drift or Hyprland internals:

```bash
bash scripts/check-version-contract.sh
bash scripts/audit-compat.sh /path/to/Hyprland
```

- compile or test failure: fix the repo first
- source audit failure: move Hyprland internals back behind `src/compat/`
- version or compat audit failure: a Hyprland contract problem, not a feature
  bug

## 3. Prove the Live Binary

If the user says "same thing" after a patch, verify the loaded binary before
changing anything else:

```bash
hyprctl plugin list
lsof -p "$(pgrep -o Hyprland)" | rg 'hyprtasking|libhyprtasking'
hyprctl plugin unload /absolute/path/to/libhyprtasking.so
hyprctl plugin load /absolute/path/to/libhyprtasking.so
```

If the mapped plugin path is not the file you just built, every later runtime
observation is suspect. `bash scripts/smoke-live.sh load-unload` and
`bash scripts/smoke-live.sh dispatchers` catch stale or partially initialized
instances.

## 4. Classify the Symptom Before Choosing a Fix

### A. Dispatcher or plugin startup failure

Signs: `Invalid dispatcher`, the plugin is listed but does nothing, or
reload/unload/load leaves a stale instance. Check the health line and the
Hyprland log (`$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/hyprland.log`)
for `[Hyprtasking]` lines.

Start in `src/plugin/dispatchers.cpp`, `src/plugin/runtime.cpp`, and
`src/compat/profile.cpp`.

### B. Rendering, recursion, or self-disable failure

Signs: blank or partial overview, the plugin disables itself after
open/reopen/reload, or flicker when reopening quickly. Run the `render-reentry`
manual check and `bash scripts/smoke-live.sh reload-open`.

Start in `src/compat/renderer_compat.cpp`, `src/layout/grid.cpp`, and
`src/overview.cpp`. Config reload always cancels active overview state and
reinitializes grid positions; a half-open overview after reload is a lifecycle
bug.

### C. Input path failure

Signs: drag never starts, right-click selection does nothing, or a logic patch
has no visible effect. Use section 5.

### D. State logic failure

Signs: the plugin gets the right button data but selects the wrong workspace,
drops on the wrong target, or resolves hover wrongly.

Start in `src/input.cpp`, `src/overview.cpp`, `src/layout/`, and
`src/logic/logic.*`.

### E. Focus or seat aftermath failure

Signs: the workspace switches but typing does not land at once, right-click
causes client-side side effects, or focus is wrong after selection. Run the
`typing-focus` manual check.

Start in `src/compat/runtime_compat.cpp`, `src/overview.cpp`, and
`src/input.cpp`.

## 5. Click, Drag, and Selection Bugs

1. Reproduce one action at a time: one failed left-drag, or one failed
   right-click selection. Do not mix symptoms.
2. Check the health line and the Hyprland log right after the action.
3. Run `wev`, click inside its window, and compare button values (`272`, `273`,
   or your configured buttons):
   - `wev` wrong too: input/session/device problem outside Hyprtasking
   - `wev` right, plugin behavior wrong: plugin boundary or hook path
   - both right: overview state logic
4. Compare against a known-good commit by hook path, not only by the
   drag/select helpers: where buttons are intercepted, whether right-click
   selects on press or release, and which path cancels the event.

   ```bash
   git show <known-good-commit>:src/input.cpp
   ```

5. Patch `src/compat/` or hook code when the plugin gets wrong button data or a
   listener path drifted. Patch overview logic when button data is right but
   preconditions, workspace resolution, or drop targets are wrong.

## 6. When to Patch `src/compat/`

Use `src/compat/` for direct Hyprland hooks and original call-throughs, access
to private or drift-prone Hyprland APIs, version-line assumptions, and code that
only translates Hyprland behavior into stable plugin behavior. Keep those
decisions out of unrelated files, and record new sensitive surface in
[`compat-contract.md`](compat-contract.md).

## Manual checks

These need eyes on the compositor. Run all of them before a release, and the
ones matching the changed subsystem after a fix.

| Check | Run after | Steps | Pass | Fail |
| --- | --- | --- | --- | --- |
| drag | click/drag or workspace-target fixes | Open the overview with a tiled window, drag it across two workspace cells, drop it. | The window follows the cursor, renders once, lands on the target workspace. | Duplicate window, large offset, hidden selected workspace, stuck mouse bind mode. |
| typing-focus | right-click, selection, or focus fixes | In grid mode with a terminal or Nautilus open, right-click a workspace, then type at once (or press `Ctrl+L` in Nautilus). | Input lands on the first key; no extra Enter needed. | The workspace shows but ignores the first keys. |
| gesture | swipe or grid-navigation changes | Start the open gesture and stop halfway, then start a move gesture and interrupt it. | The overview closes or recovers; swipe state clears. | Captured input, sticky cursor override, a later gesture acting on the wrong monitor. |
| reload-open | reload-safety changes | `hyprctl eval 'hl.plugin.hyprtasking.toggle("cursor")'`, then `hyprctl reload`. | Plugin reloads, dispatchers stay registered, compositor usable. | `Invalid dispatcher`, half-open overview, sticky cursor override. |
| monitor-remove | monitor or lifecycle changes | With the overview open or a gesture held, disable or unplug that monitor. | Stale views disappear; no drag or gesture state survives. | Crash, frozen input, callbacks on a removed monitor. |
| render-reentry | hook or rendering fixes | Open the overview, select a workspace, reopen at once; repeat, then do a reload-open cycle and repeat once more. | Hyprland stays up, the overview renders, the runtime stays enabled. | Crash, blank overview, self-disable notification, flicker or lockup on reopen. |
| hook-sunset | input or hook-policy changes, once per supported target | Route overview mouse press/release through the cancellable event-bus path instead of the `CInputManager::onMouseButton` hook; run `bash scripts/check-version-contract.sh` and `hyprctl eval 'hl.plugin.hyprtasking.health()'`. | Drag/drop and right-click selection work on every target: the hook can go. | Any target regresses: keep the hook and record which one. |

## 7. Release Gate

`bash scripts/release-check.sh` runs, in order: version contract, compat audit
(when `HYPRLAND_SOURCE` is set), source audit, compile, tests, and live smoke.
`RELEASE_MODE=offline` skips the live stages. The script header lists its
options. Finish with the manual checks above.

## 8. Common Failure Patterns

- "My patch is correct but behavior does not change": wrong binary, stale
  instance, the handler never saw real input, or the fix is at the wrong layer.
- "The overview looks wrong after a Hyprland update": assume compat drift.
- "Right-click switches workspaces, but the client still reacts": check
  cancellation timing and hook level before selection logic.
- "Drag starts, but the window or cursor lands in the wrong place": check
  workspace resolution, pointer warp, and drag-controller state once the button
  path is confirmed.

## 9. Bare Key Bindings and the Submap

Bare key bindings (no modifier) must live in the `hyprtasking` submap. The
plugin enters it through `HTCompat::enter_submap()` when a view becomes
interactively active (`active && !closing`) and leaves it through
`HTCompat::exit_submap()` when no view is, but only if Hyprtasking entered it.
The logic is in `set_runtime_state` in `src/overview.cpp`.

If keypresses drop outside the overview, check for bare binds outside the
submap block, that `hyprctl activesubmap` is empty with the overview closed, and
for an input method module grabbing keys (see
[`troubleshooting.md`](troubleshooting.md)).

## Lessons from the overview input regression

Drag and right-click selection once broke because the input path never
delivered real clicks, so correct handler logic looked ineffective. The
`CInputManager::onMouseButton` hook that fixed it is described in
[Remaining Hooks](compat-contract.md#remaining-hooks).

- Prove the loaded binary before reasoning about behavior.
- When a patch "should" work and does nothing, ask whether the code path ran
  with real input, not which condition is still wrong.
- Compare known-good commits by where input is intercepted, not only by what
  happens afterwards.
- Plugin-boundary, overview-state, and focus-aftermath bugs are different bugs;
  do not give them the same first fix.
