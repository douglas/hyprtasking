#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
source "$SCRIPT_DIR/support-matrix.sh"

printf 'Hyprtasking Hyprland guideline audit\n'
printf 'Repo: %s\n' "$REPO_DIR"

EXIT_CODE=0

THREAD_MATCHES=$(rg -n --glob '!build/**' 'std::thread|std::async|pthread_' "$REPO_DIR/src" || true)
if [[ -n "$THREAD_MATCHES" ]]; then
  printf 'error: thread primitives found in plugin runtime code:\n%s\n' "$THREAD_MATCHES" >&2
  EXIT_CODE=1
else
  printf 'ok: no thread primitives in src/\n'
fi

HOOK_MATCHES=$(rg -n --glob '!build/**' 'createFunctionHook\(' "$REPO_DIR/src" || true)
if [[ -n "$HOOK_MATCHES" ]]; then
  DISALLOWED_HOOKS=$(printf '%s\n' "$HOOK_MATCHES" | rg -v 'src/compat/profile\.cpp|src/compat/renderer_compat\.cpp|src/plugin/runtime\.cpp' || true)
  if [[ -n "$DISALLOWED_HOOKS" ]]; then
    printf 'error: disallowed function hook creation outside approved compat/runtime files:\n%s\n' "$DISALLOWED_HOOKS" >&2
    EXIT_CODE=1
  else
    printf 'ok: function hook creation limited to approved files\n'
  fi
else
  printf 'ok: no function hook creation sites found\n'
fi

HOOK_REMOVAL_MATCHES=$(rg -n --glob '!build/**' 'removeFunctionHook\(' "$REPO_DIR/src" || true)
if [[ -n "$HOOK_REMOVAL_MATCHES" ]]; then
  DISALLOWED_HOOK_REMOVALS=$(printf '%s\n' "$HOOK_REMOVAL_MATCHES" | rg -v 'src/compat/profile\.cpp|src/compat/renderer_compat\.cpp|src/plugin/runtime\.cpp' || true)
  if [[ -n "$DISALLOWED_HOOK_REMOVALS" ]]; then
    printf 'error: disallowed function hook removal outside approved compat/runtime files:\n%s\n' "$DISALLOWED_HOOK_REMOVALS" >&2
    EXIT_CODE=1
  else
    printf 'ok: function hook removal limited to approved files\n'
  fi
else
  printf 'ok: no function hook removal sites found\n'
fi

DEPRECATED_API_MATCHES=$(rg -n --glob '!build/**' \
  'HyprlandAPI::(addConfigKeyword|registerCallbackDynamic|unregisterCallback|addLayout|removeLayout|addDispatcher|getConfigValue|getFunctionAddressFromSignature|addConfigValue)\(' \
  "$REPO_DIR/src" || true)
if [[ -n "$DEPRECATED_API_MATCHES" ]]; then
  DISALLOWED_DEPRECATED_APIS=$(printf '%s\n' "$DEPRECATED_API_MATCHES" |
    rg -v 'src/config\.cpp:.*HyprlandAPI::(getConfigValue|addConfigValue)\(' || true)
  if [[ -n "$DISALLOWED_DEPRECATED_APIS" ]]; then
    printf 'error: disallowed deprecated Hyprland plugin APIs found:\n%s\n' "$DISALLOWED_DEPRECATED_APIS" >&2
    EXIT_CODE=1
  else
    printf 'ok: deprecated Hyprland plugin APIs limited to audited compatibility fallbacks\n'
  fi
else
  printf 'ok: no deprecated Hyprland plugin APIs found\n'
fi

ORIGINAL_DEREF_MATCHES=$(rg -n --glob '!build/**' -- '->m_original' "$REPO_DIR/src" || true)
if [[ -n "$ORIGINAL_DEREF_MATCHES" ]]; then
  DISALLOWED_ORIGINAL_DEREFS=$(printf '%s\n' "$ORIGINAL_DEREF_MATCHES" |
    rg -v 'src/compat/profile\.cpp|src/compat/renderer_compat\.cpp|src/compat/runtime_compat\.cpp' || true)
  if [[ -n "$DISALLOWED_ORIGINAL_DEREFS" ]]; then
    printf 'error: hook original access outside approved wrappers:\n%s\n' "$DISALLOWED_ORIGINAL_DEREFS" >&2
    EXIT_CODE=1
  else
    printf 'ok: hook original access limited to approved wrappers\n'
  fi
else
  printf 'ok: no hook original access found\n'
fi

DO_LATER_SYNC_FALLBACK=$(rg -n --glob '!build/**' 'callback\(\);' "$REPO_DIR/src/compat/runtime_compat.cpp" || true)
if [[ -n "$DO_LATER_SYNC_FALLBACK" ]]; then
  printf 'error: delayed callbacks must fail closed instead of running synchronously:\n%s\n' "$DO_LATER_SYNC_FALLBACK" >&2
  EXIT_CODE=1
else
  printf 'ok: delayed callback helper does not run synchronous fallbacks\n'
fi

STALE_WORKFLOW_REFS=$(rg -n 'v0\.54\.[0-2]' "$REPO_DIR/.github/workflows" || true)
if [[ -n "$STALE_WORKFLOW_REFS" ]]; then
  printf 'error: workflow matrix contains unsupported stale Hyprland refs:\n%s\n' "$STALE_WORKFLOW_REFS" >&2
  EXIT_CODE=1
else
  printf 'ok: workflow matrix does not include stale Hyprland refs\n'
fi

STALE_HYPRPM_REFS=$(rg -n 'v0\.(4[0-9]|5[0-3]|54\.[0-2])' "$REPO_DIR/hyprpm.toml" || true)
if [[ -n "$STALE_HYPRPM_REFS" ]]; then
  printf 'error: hyprpm pins contain unsupported stale Hyprland refs:\n%s\n' "$STALE_HYPRPM_REFS" >&2
  EXIT_CODE=1
else
  printf 'ok: hyprpm pins do not include stale Hyprland refs\n'
fi

for supported_ref in "${HYPRPM_REQUIRED_REFS[@]}"; do
  if ! rg -q "#${supported_ref}$" "$REPO_DIR/hyprpm.toml"; then
    printf 'error: hyprpm pins missing supported ref %s\n' "$supported_ref" >&2
    EXIT_CODE=1
  fi
done
if [[ "$EXIT_CODE" -eq 0 ]]; then
  printf 'ok: hyprpm pins cover supported Hyprland refs\n'
fi

if [[ "$EXIT_CODE" -ne 0 ]]; then
  exit "$EXIT_CODE"
fi

printf 'Guideline audit passed.\n'
