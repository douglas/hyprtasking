#!/usr/bin/env bash
# Sourced by the other scripts. hypr_set_prefix fills HYPRCTL_PREFIX with the
# hyprctl command for the first live Hyprland instance.

# shellcheck disable=SC2034 # read by the sourcing script
HYPRCTL_PREFIX=()

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

# hypr_set_prefix PROBE_ARGS...: the hyprctl probe is the liveness check, so
# stale sockets left by a dead compositor are skipped.
hypr_set_prefix() {
  local signature
  while IFS= read -r signature; do
    [[ -n "$signature" ]] || continue
    [[ -S "$(hypr_signature_socket_path "$signature")" ]] || continue

    if env HYPRLAND_INSTANCE_SIGNATURE="$signature" hyprctl "$@" >/dev/null 2>&1; then
      HYPRCTL_PREFIX=(env "HYPRLAND_INSTANCE_SIGNATURE=$signature" hyprctl)
      return 0
    fi
  done < <(hypr_candidate_signatures)

  return 1
}
