#!/usr/bin/env bash
# Sourced by the other scripts.
# shellcheck disable=SC2034

# Entries match a version exactly or as a prefix ("0.55" covers 0.55.x).
SUPPORTED_VERSIONS=("0.55" "0.56.0" "0.56.2")
HYPRPM_REQUIRED_REFS=("v0.55.0" "v0.55.1" "v0.55.2" "v0.55.4" "v0.56.0" "v0.56.2")

# supported_target VERSION: prints the SUPPORTED_VERSIONS entry covering
# VERSION, or fails.
supported_target() {
  local supported
  [[ -n "$1" ]] || return 1
  for supported in "${SUPPORTED_VERSIONS[@]}"; do
    if [[ "$1" == "$supported" || "$1" == "$supported".* ]]; then
      printf '%s' "$supported"
      return 0
    fi
  done
  return 1
}

# Human-readable list, e.g. "0.55.x, 0.56.0, and 0.56.2".
supported_targets_text() {
  local count=${#SUPPORTED_VERSIONS[@]} i target text=""
  for ((i = 0; i < count; i++)); do
    target=${SUPPORTED_VERSIONS[i]}
    [[ "$target" == *.*.* ]] || target+=.x
    if ((i == 0)); then
      text=$target
    elif ((i == count - 1)); then
      text+=", and $target"
    else
      text+=", $target"
    fi
  done
  printf '%s' "$text"
}

SUPPORTED_TARGETS=$(supported_targets_text)
