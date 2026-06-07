#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
state_dir="$repo/.nested"
config="${1:-$repo/test/nested-hyprtasking.conf}"
plugin="${2:-$repo/build/libhyprtasking.so}"
log="$state_dir/hyprtasking-smoke.log"

mkdir -p "$state_dir"
: >"$log"

if [[ ! -r "$config" ]]; then
  echo "missing nested config: $config" >&2
  exit 2
fi
if [[ ! -r "$plugin" ]]; then
  echo "missing plugin: $plugin" >&2
  exit 2
fi

before_file="$state_dir/before-instances"
find "/run/user/$(id -u)/hypr" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | sort >"$before_file" || true

(
  cd "$repo"
  exec Hyprland --config "$config"
) >>"$log" 2>&1 &
pid=$!

cleanup() {
  set +e
  if [[ -n "${sig:-}" ]]; then
    HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl dispatch exit >/dev/null 2>&1 || true
  fi
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" >/dev/null 2>&1 || true
    sleep 0.5
  fi
  if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

sig=""
for _ in {1..80}; do
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "Nested Hyprland exited early. Log:" >&2
    tail -160 "$log" >&2 || true
    exit 3
  fi

  while IFS= read -r candidate; do
    [[ -z "$candidate" ]] && continue
    if ! grep -qxF "$candidate" "$before_file" && [[ -S "/run/user/$(id -u)/hypr/$candidate/.socket.sock" ]]; then
      sig="$candidate"
      break
    fi
  done < <(find "/run/user/$(id -u)/hypr" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | sort)

  [[ -n "$sig" ]] && break
  sleep 0.25
done

if [[ -z "$sig" ]]; then
  echo "Timed out waiting for nested Hyprland socket. Log:" >&2
  tail -160 "$log" >&2 || true
  exit 4
fi

hc() {
  HYPRLAND_INSTANCE_SIGNATURE="$sig" hyprctl "$@" 2>&1
}

expect_ok() {
  local label="$1"
  shift
  local out
  out="$(hc "$@")"
  printf '%s: %s\n' "$label" "$out" | tee -a "$log"
  if [[ "$out" != "ok" && "$out" != *$'\nok' ]]; then
    echo "expected ok from $label" >&2
    echo "health:" >&2
    hc dispatch hyprtasking:health print 2>&1 >&2 || true
    echo "health-json:" >&2
    hc dispatch hyprtasking:health print-json 2>&1 >&2 || true
    exit 5
  fi
}

expect_empty_configerrors() {
  local out
  out="$(hc configerrors)"
  printf 'configerrors: %s\n' "$out" | tee -a "$log"
  if [[ -n "$out" ]]; then
    echo "nested config errors present" >&2
    exit 6
  fi
}

expect_empty_configerrors
expect_ok "plugin-load" plugin load "$plugin"
expect_ok "set-active-only" keyword plugin:hyprtasking:active_only 1
expect_ok "set-grid-auto" keyword plugin:hyprtasking:grid:auto 1
expect_ok "set-center-partial" keyword plugin:hyprtasking:grid:center_partial_rows 1
expect_ok "define-submap" keyword submap hyprtasking
for n in {1..9}; do
  expect_ok "bind-number-$n" keyword bind ", $n, hyprtasking:select-commit, $n"
done
expect_ok "bind-escape" keyword bind ", ESCAPE, hyprtasking:toggle, cursor"
expect_ok "reset-submap" keyword submap reset
expect_empty_configerrors

expect_ok "toggle-open-select-commit" dispatch hyprtasking:toggle cursor
sleep 0.2
expect_ok "select-commit-1" dispatch hyprtasking:select-commit 1
sleep 0.2

iterations="${ITERATIONS:-10}"
for ((i = 1; i <= iterations; i++)); do
  expect_ok "toggle-open-$i" dispatch hyprtasking:toggle cursor
  sleep 0.2
  expect_ok "toggle-close-$i" dispatch hyprtasking:toggle cursor
  sleep 0.2
done
expect_empty_configerrors

echo "nested hyprtasking smoke passed iterations=$iterations (signature=$sig log=$log)"
