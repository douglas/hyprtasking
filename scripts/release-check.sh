#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$REPO_DIR/build"}
SMOKE_MODE=${SMOKE_MODE:-stress}
PRINT_MANUAL_CHECKLIST=${PRINT_MANUAL_CHECKLIST:-1}
HYPRLAND_SOURCE=${HYPRLAND_SOURCE:-}
MANUAL_SCENARIO=${MANUAL_SCENARIO:-all}
RELEASE_CHECK_FORMAT=${RELEASE_CHECK_FORMAT:-text}
STAGE_NAMES=()
STAGE_EXIT_CODES=()
FAILED_STAGE=""
FAILED_STAGE_EXIT_CODE=0
FAILED_STAGE_OUTPUT=""
AUDIT_JSON="null"
AUDIT_SURFACE_JSON="null"
BOUNDARY_JSON="null"
MANUAL_CHECKLIST_JSON="null"

run() {
  if [[ "$RELEASE_CHECK_FORMAT" != "json" ]]; then
    printf '$ %s\n' "$*"
    "$@"
    return
  fi

  "$@" >/tmp/release-check.out 2>&1 || {
    cat /tmp/release-check.out
    return 1
  }
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

record_stage() {
  local name=$1
  local exit_code=$2
  STAGE_NAMES+=("$name")
  STAGE_EXIT_CODES+=("$exit_code")
}

run_stage() {
  local name=$1
  shift

  if [[ "$RELEASE_CHECK_FORMAT" != "json" ]]; then
    run "$@"
    record_stage "$name" 0
    return
  fi

  local output
  local exit_code=0
  output="$("$@" 2>&1)" || exit_code=$?
  record_stage "$name" "$exit_code"

  if ((exit_code != 0)); then
    FAILED_STAGE="$name"
    FAILED_STAGE_EXIT_CODE=$exit_code
    FAILED_STAGE_OUTPUT="$output"
    emit_json_result "failed"
    exit "$exit_code"
  fi
}

emit_json_result() {
  local status=$1
  local i

  if [[ "$RELEASE_CHECK_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"build_dir":%s,' "$(json_string "$BUILD_DIR")"
  printf '"smoke_mode":%s,' "$(json_string "$SMOKE_MODE")"
  printf '"manual_scenario":%s,' "$(json_string "$MANUAL_SCENARIO")"
  printf '"manual_requested":%s,' "$([[ "$PRINT_MANUAL_CHECKLIST" == "1" ]] && printf true || printf false)"
  printf '"hyprland_source":%s,' "$(json_string "$HYPRLAND_SOURCE")"
  printf '"failed_stage":%s,' "$(json_string "$FAILED_STAGE")"
  printf '"failed_stage_exit_code":%s,' "$FAILED_STAGE_EXIT_CODE"
  printf '"failed_stage_output":%s,' "$(json_string "$FAILED_STAGE_OUTPUT")"
  printf '"audit_results":{'
  printf '"compat":%s,' "$AUDIT_JSON"
  printf '"compat_surface":%s,' "$AUDIT_SURFACE_JSON"
  printf '"boundary":%s' "$BOUNDARY_JSON"
  printf '},'
  printf '"manual_checklist":%s,' "$MANUAL_CHECKLIST_JSON"
  printf '"stages":['
  for ((i = 0; i < ${#STAGE_NAMES[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '{'
    printf '"name":%s,' "$(json_string "${STAGE_NAMES[i]}")"
    printf '"exit_code":%s' "${STAGE_EXIT_CODES[i]}"
    printf '}'
  done
  printf ']'
  printf '}\n'
}

if [[ ! -d "$BUILD_DIR" ]]; then
  if [[ "$RELEASE_CHECK_FORMAT" == "json" ]]; then
    FAILED_STAGE="build_dir"
    FAILED_STAGE_EXIT_CODE=1
    FAILED_STAGE_OUTPUT="Build directory does not exist: $BUILD_DIR"
    emit_json_result "failed"
  else
    printf 'Build directory does not exist: %s\n' "$BUILD_DIR" >&2
  fi
  exit 1
fi

if [[ -n "$HYPRLAND_SOURCE" ]]; then
  if [[ "$RELEASE_CHECK_FORMAT" == "json" ]]; then
    AUDIT_JSON="$(
      env AUDIT_FORMAT=json bash "$SCRIPT_DIR/audit-compat.sh" "$HYPRLAND_SOURCE"
    )"
    AUDIT_SURFACE_JSON="$(
      env AUDIT_FORMAT=json bash "$SCRIPT_DIR/audit-compat-surface.sh" "$HYPRLAND_SOURCE"
    )"
    run_stage audit env "AUDIT_FORMAT=json" bash "$SCRIPT_DIR/audit-compat.sh" "$HYPRLAND_SOURCE"
    run_stage audit-surface env "AUDIT_FORMAT=json" bash "$SCRIPT_DIR/audit-compat-surface.sh" "$HYPRLAND_SOURCE"
  else
    run_stage audit bash "$SCRIPT_DIR/audit-compat.sh" "$HYPRLAND_SOURCE"
    run_stage audit-surface bash "$SCRIPT_DIR/audit-compat-surface.sh" "$HYPRLAND_SOURCE"
  fi
fi

if [[ "$RELEASE_CHECK_FORMAT" == "json" ]]; then
  BOUNDARY_JSON="$(
    env AUDIT_BOUNDARY_FORMAT=json bash "$SCRIPT_DIR/audit-boundary.sh"
  )"
  run_stage boundary env "AUDIT_BOUNDARY_FORMAT=json" bash "$SCRIPT_DIR/audit-boundary.sh"
else
  run_stage boundary bash "$SCRIPT_DIR/audit-boundary.sh"
fi
run_stage compile meson compile -C "$BUILD_DIR"
run_stage test meson test -C "$BUILD_DIR"
run_stage dispatchers bash "$SCRIPT_DIR/smoke-live.sh" dispatchers
run_stage smoke env "PRINT_MANUAL_FOLLOW_UP=0" bash "$SCRIPT_DIR/smoke-live.sh" "$SMOKE_MODE"

if [[ "$PRINT_MANUAL_CHECKLIST" == "1" ]]; then
  if [[ "$RELEASE_CHECK_FORMAT" == "json" ]]; then
    MANUAL_CHECKLIST_JSON="$(
      env CHECKLIST_FORMAT=json bash "$SCRIPT_DIR/manual-runtime-check.sh" "$MANUAL_SCENARIO"
    )"
  fi
  run_stage manual bash "$SCRIPT_DIR/manual-runtime-check.sh" "$MANUAL_SCENARIO"
fi

emit_json_result "ok"
