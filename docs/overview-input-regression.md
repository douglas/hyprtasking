# Overview Input Regression Lessons

This document captures the main lessons from the long debugging session that
restored stable overview mouse interaction on the Hyprland `0.54.3` support
target.

It is intentionally distilled. The goal is not to preserve every experiment in
order, but to keep the high-value evidence, the wrong turns that were expensive,
and the rules future maintainers should apply when similar regressions appear.

## Incident Summary

The visible symptoms were:

- left-drag in the overview did nothing
- right-click selection did not switch workspaces cleanly
- repeated logic changes appeared to have no user-visible effect

The final fix was not a layout or selection-state change. The real failure was
the click listener path. Hyprtasking had to intercept mouse buttons through a
direct `CInputManager::onMouseButton` hook instead of relying on the
cancellable event-bus mouse-button listener for this runtime.

## What We Proved Wrong

Several plausible explanations looked convincing but were false.

### "The wrong binary is being tested"

This had to be ruled out first because every later theory depends on it.

The useful checks were:

```bash
hyprctl plugin list
lsof -p "$(pgrep -o Hyprland)" | rg 'hyprtasking|libhyprtasking'
```

The live Hyprland process had the expected
`/home/douglas/src/hyprtasking/build/libhyprtasking.so` mapped. Once that was
confirmed, repeated "same thing" results could no longer be blamed on stale
loads.

### "The regression must be in overview state logic"

We restored the old `bb58a45` mouse semantics more than once:

- left press -> `start_window_drag()`
- left release -> `end_window_drag()`
- right press -> workspace selection

Those changes were logically sound and matched the old branch, but behavior
still did not move. That was the clue that the failure sat below overview state
handling.

### "The motion path is swallowing drag updates"

`on_mouse_move()` returning the wrong cancellation value can absolutely break
dragging, but it was not the root cause here. Restoring the old motion behavior
did not restore drag start.

## Decisive Evidence

Two probes ended the ambiguity.

### 1. Plugin-local trace showed bogus button events

Once file-backed tracing was added, Hyprtasking repeatedly logged mouse-button
events like:

- `button=32767`
- `pressed=false`

while the configured buttons were:

- drag = `272`
- select = `273`

That proved the plugin was not seeing real left/right click data on the path it
was listening to.

### 2. `wev` showed the compositor session was fine

Using `wev` against the same session showed normal Wayland pointer events:

- left = `272`
- right = `273`

That split the problem cleanly:

- the session and seat were fine
- the plugin boundary was not

At that point, changing overview behavior without changing the hook path was
very unlikely to help.

## Actual Root Cause

For the supported runtime, Hyprtasking's click handling could not depend on the
event-bus mouse-button listener. The plugin needed to intercept real mouse
buttons at the input-manager entrypoint and then decide whether to cancel the
original Hyprland flow.

The stable path became:

1. Hook `CInputManager::onMouseButton`
2. Run Hyprtasking's drag/select logic first
3. If Hyprtasking handles the event, stop there
4. Otherwise call the original Hyprland implementation

This restored:

- left-drag start and release handling
- right-click workspace selection
- cancellation semantics that prevent client-side side effects from racing the
  overview interaction

## Why Earlier Fixes Looked Ineffective

The earlier patches were mostly acting at the wrong layer.

- Reverting button semantics to match `bb58` changed the behavior only inside
  Hyprtasking's handlers.
- The broken listener path meant those handlers were never reached with the
  real click events that mattered.
- That made correct logic look ineffective, which is one of the easiest ways to
  waste time on input bugs like this one.

When a patch "should" work and still appears to do nothing, the next question
should be "did the code path run with real input?" not "which condition inside
the handler is still wrong?"

## Maintainer Lessons

### Prove the loaded binary first

Do not speculate about runtime behavior until the loaded plugin path is
verified. If this check is skipped, every later observation is contaminated.

### Compare against a known-good commit by behavior and hook path

A branch diff is only useful if it tracks where input is intercepted, not just
what happens after interception.

For this incident, the important comparison was not only the drag/select
functions, but the fact that the effective click path had to change.

### Separate plugin-boundary bugs from overview-state bugs

Use the evidence to classify the failure:

- real clicks missing or malformed at the plugin boundary -> compat/hook problem
- real clicks present but wrong workspace or drag behavior -> overview/state
  problem
- right workspace selected but typing/focus wrong afterward -> seat/focus
  aftermath problem

Those are different bugs and should not share the same first fix attempt.

### Keep diagnostic tracing in-tree, but gated

The trace work paid for itself. It should stay in the repo, but behind a flag:

```bash
hyprctl keyword plugin:hyprtasking:debug:trace 1
tail -f /tmp/hyprtasking-trace.log
```

That gives maintainers a cheap, repeatable way to inspect live runtime state
without re-adding ad hoc logging every time.

### Treat `src/compat/` as the Hyprland repair zone

If the bug is caused by the plugin seeing the wrong runtime contract, patch
`src/compat/` or the hook boundary first. Do not spread direct Hyprland
internals across the rest of the tree.

## Quick Regression Checklist

Use this condensed checklist when drag, click, or workspace-selection behavior
breaks again:

1. Build and run the repo checks:

```bash
meson compile -C build
meson test -C build logic-tests --print-errorlogs
bash scripts/audit-boundary.sh
```

2. Verify the live plugin path:

```bash
hyprctl plugin list
lsof -p "$(pgrep -o Hyprland)" | rg 'hyprtasking|libhyprtasking'
```

3. Enable trace and reproduce:

```bash
hyprctl keyword plugin:hyprtasking:debug:trace 1
tail -f /tmp/hyprtasking-trace.log
```

4. If button values in the trace are wrong or missing, verify with `wev`.
5. If `wev` is correct but the plugin trace is wrong, inspect the hook/listener
   path before changing overview logic.
6. If the plugin trace is correct, move on to layout/state/focus debugging.

## Related Docs

- [`docs/debugging-playbook.md`](debugging-playbook.md)
- [`docs/architecture.md`](architecture.md)
- [`docs/compat-contract.md`](compat-contract.md)
