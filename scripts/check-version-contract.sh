#!/usr/bin/env bash
# Fails unless the installed hyprland package, the running Hyprland, and the
# optional audit source tree are on the same supported version.
# Exit codes: 2 runtime/source missing, 3 unsupported version, 4 mismatch.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$SCRIPT_DIR/common-hyprctl.sh"
source "$SCRIPT_DIR/support-matrix.sh"

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-}}

die() {
  printf '%s\n' "$2" >&2
  exit "$1"
}

normalize_version() {
  local version semver
  version="${1//[[:space:]]/}"
  version="${version#v}"
  if semver="$(hypr_extract_semver "$version")"; then
    version="$semver"
  fi
  printf '%s' "$version"
}

# Prints the SUPPORTED_VERSIONS entry covering $1, or fails.
support_target() {
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

runtime_version() {
  local output parsed
  output="$("${HYPRCTL_PREFIX[@]}" version 2>/dev/null)" || return 1
  parsed="$(printf '%s\n' "$output" | sed -n 's/^Tag:[[:space:]]*v\{0,1\}\(.*\)$/\1/p' | head -n 1)"
  if [[ -z "$parsed" ]]; then
    parsed="$(printf '%s\n' "$output" | sed -n 's/^Version:[[:space:]]*v\{0,1\}\(.*\)$/\1/p' | head -n 1)"
  fi
  normalize_version "$parsed"
}

PKG_VERSION="$(normalize_version "$(pkg-config --modversion hyprland)")"
PKG_TARGET="$(support_target "$PKG_VERSION")" ||
  die 3 "Installed hyprland package is outside the supported targets ($SUPPORTED_TARGETS): $PKG_VERSION"

hypr_set_prefix version || die 2 'Unable to detect a live Hyprland instance.'
RUNTIME_VERSION="$(runtime_version)" || die 2 'Unable to query the live Hyprland runtime version.'
[[ -n "$RUNTIME_VERSION" ]] || die 2 'Unable to determine the live Hyprland runtime version.'
RUNTIME_TARGET="$(support_target "$RUNTIME_VERSION")" ||
  die 3 "Running Hyprland is outside the supported targets ($SUPPORTED_TARGETS): $RUNTIME_VERSION"
[[ "$PKG_TARGET" == "$RUNTIME_TARGET" ]] ||
  die 4 "Installed hyprland package ($PKG_VERSION) does not match the running Hyprland ($RUNTIME_VERSION)."

if [[ -n "$HYPRLAND_SOURCE" ]]; then
  [[ -f "$HYPRLAND_SOURCE/VERSION" ]] ||
    die 2 "Hyprland source tree is missing VERSION: $HYPRLAND_SOURCE"
  SOURCE_VERSION="$(normalize_version "$(< "$HYPRLAND_SOURCE/VERSION")")"
  SOURCE_TARGET="$(support_target "$SOURCE_VERSION")" ||
    die 3 "Hyprland source tree is outside the supported targets ($SUPPORTED_TARGETS): $SOURCE_VERSION"
  [[ "$SOURCE_TARGET" == "$PKG_TARGET" ]] ||
    die 4 "Hyprland source tree ($SOURCE_VERSION) does not match installed package headers ($PKG_VERSION)."
fi

printf 'Hyprtasking version contract check\n'
printf 'Supported targets: %s\n' "$SUPPORTED_TARGETS"
printf 'Installed hyprland package: %s\n' "$PKG_VERSION"
printf 'Running Hyprland: %s\n' "$RUNTIME_VERSION"
if [[ -n "$HYPRLAND_SOURCE" ]]; then
  printf 'Audit source: %s (%s); builds use the installed package headers only.\n' \
    "$HYPRLAND_SOURCE" "$SOURCE_VERSION"
fi
