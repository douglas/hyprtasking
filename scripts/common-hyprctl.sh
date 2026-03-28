#!/usr/bin/env bash

hypr_extract_semver() {
  local value=$1
  if [[ "$value" =~ ([0-9]+\.[0-9]+\.[0-9]+) ]]; then
    printf '%s' "${BASH_REMATCH[1]}"
    return 0
  fi

  return 1
}

hypr_runtime_dir() {
  printf '%s' "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
}

hypr_candidate_signatures() {
  local runtime_dir
  runtime_dir="$(hypr_runtime_dir)"

  if [[ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ]]; then
    printf '%s\n' "${HYPRLAND_INSTANCE_SIGNATURE}"
  fi

  local dir
  for dir in "$runtime_dir"/hypr/*; do
    [[ -d "$dir" ]] || continue
    printf '%s\n' "${dir##*/}"
  done
}

hypr_signature_socket_path() {
  local runtime_dir
  runtime_dir="$(hypr_runtime_dir)"
  printf '%s/hypr/%s/.socket.sock' "$runtime_dir" "$1"
}

hypr_signature_lock_path() {
  local runtime_dir
  runtime_dir="$(hypr_runtime_dir)"
  printf '%s/hypr/%s/hyprland.lock' "$runtime_dir" "$1"
}

hyprtasking_configured_plugin_path() {
  local config_dir="${HOME}/.config/hypr"
  [[ -d "$config_dir" ]] || return 1

  local matches
  matches="$(
    rg -N --no-filename --glob '*.conf' '^[[:space:]]*plugin[[:space:]]*=[[:space:]]*(.+libhyprtasking\.so)[[:space:]]*$' "$config_dir" -r '$1'
  )" || true

  matches="$(printf '%s\n' "$matches" | sed '/^$/d')"
  [[ -n "$matches" ]] || return 1

  local first
  first="$(printf '%s\n' "$matches" | head -n 1)"
  printf '%s' "$first"
}

hypr_signature_pid() {
  local lock_path
  lock_path="$(hypr_signature_lock_path "$1")"
  [[ -f "$lock_path" ]] || return 1

  local pid
  pid="$(sed -n '1p' "$lock_path")"
  [[ "$pid" =~ ^[0-9]+$ ]] || return 1
  printf '%s' "$pid"
}

hypr_signature_pid_alive() {
  local pid
  pid="$(hypr_signature_pid "$1")" || return 0
  [[ -d "/proc/$pid" ]]
}

hypr_set_prefix() {
  local probe_args=("$@")
  local -A seen=()
  local signature

  HYPRCTL_PREFIX=()

  while IFS= read -r signature; do
    [[ -n "$signature" ]] || continue
    if [[ -n "${seen[$signature]+x}" ]]; then
      continue
    fi
    seen[$signature]=1

    local socket_path
    socket_path="$(hypr_signature_socket_path "$signature")"
    [[ -S "$socket_path" ]] || continue
    hypr_signature_pid_alive "$signature" || continue

    if env HYPRLAND_INSTANCE_SIGNATURE="$signature" hyprctl "${probe_args[@]}" >/dev/null 2>&1; then
      HYPRCTL_PREFIX=(env "HYPRLAND_INSTANCE_SIGNATURE=$signature" hyprctl)
      return 0
    fi
  done < <(hypr_candidate_signatures)

  return 1
}
