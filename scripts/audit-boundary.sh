#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

failures=0

run_check() {
  local label=$1
  local pattern=$2
  local matches

  matches=$(rg -n --glob '!src/compat/*' -- "$pattern" "$REPO_DIR/src" || true)
  if [[ -z "$matches" ]]; then
    printf 'ok: %s\n' "$label"
    return
  fi

  failures=$((failures + 1))
  printf 'forbidden: %s\n' "$label" >&2
  printf '%s\n' "$matches" >&2
}

printf 'Hyprtasking compat boundary audit\n'
printf 'Repo: %s\n' "$REPO_DIR"
printf 'Allowed direct Hyprland internals location: src/compat/\n'

run_check 'Hyprland globals outside compat' 'g_p[A-Za-z0-9_]+'
run_check 'raw m_ field access outside compat' '->m_[A-Za-z0-9_]+'
run_check 'Event bus access outside compat' 'Event::bus\(\)'
run_check 'monitor focus mutation outside compat' 'HTCompat::focus_monitor\(|rawMonitorFocus\('

if ((failures > 0)); then
  printf '\nBoundary audit failed with %d issue(s).\n' "$failures" >&2
  printf 'Move direct Hyprland internals behind src/compat/ before release.\n' >&2
  exit 1
fi

printf '\nBoundary audit passed.\n'
