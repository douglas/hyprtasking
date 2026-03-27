#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
AUDIT_BOUNDARY_FORMAT=${AUDIT_BOUNDARY_FORMAT:-text}

failures=0
CHECK_LABELS=()
FAILED_LABELS=()
FAILED_MATCHES=()

print_out() {
  if [[ "$AUDIT_BOUNDARY_FORMAT" != "json" ]]; then
    printf '%b' "$1"
  fi
}

print_err() {
  if [[ "$AUDIT_BOUNDARY_FORMAT" != "json" ]]; then
    printf '%b' "$1" >&2
  fi
}

json_escape() {
  local value=$1
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  value=${value//$'\n'/\\n}
  value=${value//$'\r'/\\r}
  value=${value//$'\t'/\\t}
  printf '%s' "$value"
}

json_string() {
  printf '"%s"' "$(json_escape "$1")"
}

emit_json_result() {
  local status=$1
  local i

  if [[ "$AUDIT_BOUNDARY_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"repo":%s,' "$(json_string "$REPO_DIR")"
  printf '"allowed_boundary":%s,' "$(json_string "src/compat/")"
  printf '"failure_count":%s,' "$failures"
  printf '"checked_rules":['
  for ((i = 0; i < ${#CHECK_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "${CHECK_LABELS[i]}")"
  done
  printf '],'
  printf '"failed_rules":['
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '{'
    printf '"label":%s,' "$(json_string "${FAILED_LABELS[i]}")"
    printf '"matches":%s' "$(json_string "${FAILED_MATCHES[i]}")"
    printf '}'
  done
  printf ']'
  printf '}\n'
}

run_check() {
  local label=$1
  local pattern=$2
  local matches
  CHECK_LABELS+=("$label")

  matches=$(rg -n --glob '!src/compat/*' -- "$pattern" "$REPO_DIR/src" || true)
  if [[ -z "$matches" ]]; then
    print_out "ok: $label\n"
    return
  fi

  failures=$((failures + 1))
  FAILED_LABELS+=("$label")
  FAILED_MATCHES+=("$matches")
  print_err "forbidden: $label\n"
  print_err "$matches\n"
}

print_out 'Hyprtasking compat boundary audit\n'
print_out "Repo: $REPO_DIR\n"
print_out 'Allowed direct Hyprland internals location: src/compat/\n'

run_check 'Hyprland globals outside compat' 'g_p[A-Za-z0-9_]+'
run_check 'raw m_ field access outside compat' '->m_[A-Za-z0-9_]+'
run_check 'Event bus access outside compat' 'Event::bus\(\)'
run_check 'monitor focus mutation outside compat' 'HTCompat::focus_monitor\(|rawMonitorFocus\('

if ((failures > 0)); then
  print_err "\nBoundary audit failed with $failures issue(s).\n"
  print_err 'Move direct Hyprland internals behind src/compat/ before release.\n'
  emit_json_result "failed"
  exit 1
fi

print_out '\nBoundary audit passed.\n'
emit_json_result "ok"
