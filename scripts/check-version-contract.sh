#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "$SCRIPT_DIR/common-hyprctl.sh"
source "$SCRIPT_DIR/support-matrix.sh"

CHECK_FORMAT=${CHECK_FORMAT:-text}
HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-}}
HYPRCTL_PREFIX=()

print_out() {
  if [[ "$CHECK_FORMAT" != "json" ]]; then
    printf '%b' "$1"
  fi
}

print_err() {
  if [[ "$CHECK_FORMAT" != "json" ]]; then
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

emit_json() {
  local status=$1
  local exit_code=$2
  local message=$3

  if [[ "$CHECK_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"exit_code":%s,' "$exit_code"
  printf '"message":%s,' "$(json_string "$message")"
  printf '"supported_targets":%s,' "$(json_string "$SUPPORTED_TARGETS")"
  printf '"supported_versions":%s,' "$(json_string "$SUPPORTED_VERSIONS_TEXT")"
  printf '"package_version":%s,' "$(json_string "${PKG_VERSION:-}")"
  printf '"runtime_version":%s,' "$(json_string "${RUNTIME_VERSION:-}")"
  printf '"source_version":%s,' "$(json_string "${SOURCE_VERSION:-}")"
  printf '"hyprland_source":%s' "$(json_string "$HYPRLAND_SOURCE")"
  printf '}\n'
}

normalize_version() {
  local version=$1
  version="${version#"${version%%[![:space:]]*}"}"
  version="${version%"${version##*[![:space:]]}"}"
  version="${version#v}"
  local semver
  semver="$(hypr_extract_semver "$version" || true)"
  if [[ -n "$semver" ]]; then
    version="$semver"
  fi
  printf '%s' "$version"
}

version_matches_minor() {
  local version normalized supported
  version="$1"
  normalized="$(normalize_version "$version")"
  [[ -z "$normalized" ]] && return 1

  for supported in "${SUPPORTED_VERSIONS[@]}"; do
    if [[ "$normalized" == "$supported" ]]; then
      return 0
    fi
  done

  return 1
}

extract_runtime_version() {
  local output
  output="$("${HYPRCTL_PREFIX[@]}" version 2>/dev/null)" || return 1

  local parsed
  parsed="$(printf '%s\n' "$output" | sed -n 's/^Tag:[[:space:]]*v\{0,1\}\(.*\)$/\1/p' | head -n 1)"
  if [[ -z "$parsed" ]]; then
    parsed="$(printf '%s\n' "$output" | sed -n 's/^Version:[[:space:]]*v\{0,1\}\(.*\)$/\1/p' | head -n 1)"
  fi

  printf '%s' "$(normalize_version "$parsed")"
}

init_hyprctl_prefix() {
  if ! hypr_set_prefix version; then
    print_err "Unable to detect a live Hyprland instance.\n"
    emit_json "missing_runtime" 2 "Unable to detect a live Hyprland instance."
    exit 2
  fi
}

PKG_VERSION="$(normalize_version "$(pkg-config --modversion hyprland)")"

if ! version_matches_minor "$PKG_VERSION"; then
  print_err "Installed hyprland package is outside the supported targets: $PKG_VERSION\n"
  print_err "Supported exact versions: $SUPPORTED_VERSIONS_TEXT\n"
  emit_json "unsupported_package" 3 "Installed hyprland package is outside the supported targets."
  exit 3
fi

init_hyprctl_prefix
if ! RUNTIME_VERSION="$(extract_runtime_version)"; then
  print_err "Unable to query the live Hyprland runtime version.\n"
  emit_json "missing_runtime_version" 2 "Unable to query the live Hyprland runtime version."
  exit 2
fi

if [[ -z "$RUNTIME_VERSION" ]]; then
  print_err "Unable to determine the live Hyprland runtime version.\n"
  emit_json "missing_runtime_version" 2 "Unable to determine the live Hyprland runtime version."
  exit 2
fi

if ! version_matches_minor "$RUNTIME_VERSION"; then
  print_err "Running Hyprland instance is outside the supported targets: $RUNTIME_VERSION\n"
  print_err "Supported exact versions: $SUPPORTED_VERSIONS_TEXT\n"
  emit_json "unsupported_runtime" 3 "Running Hyprland instance is outside the supported targets."
  exit 3
fi

if [[ "$PKG_VERSION" != "$RUNTIME_VERSION" ]]; then
  print_err "Installed hyprland package ($PKG_VERSION) does not match the running Hyprland instance ($RUNTIME_VERSION).\n"
  emit_json "package_runtime_mismatch" 4 "Installed hyprland package does not match the running Hyprland instance."
  exit 4
fi

SOURCE_VERSION=""
if [[ -n "$HYPRLAND_SOURCE" ]]; then
  if [[ ! -f "$HYPRLAND_SOURCE/VERSION" ]]; then
    print_err "Hyprland source tree is missing VERSION: $HYPRLAND_SOURCE\n"
    emit_json "missing_source_version" 5 "Hyprland source tree is missing VERSION."
    exit 5
  fi

  SOURCE_VERSION="$(normalize_version "$(tr -d '[:space:]' < "$HYPRLAND_SOURCE/VERSION")")"

  if ! version_matches_minor "$SOURCE_VERSION"; then
    print_err "Hyprland source tree is outside the supported targets: $SOURCE_VERSION\n"
    print_err "Supported exact versions: $SUPPORTED_VERSIONS_TEXT\n"
    emit_json "unsupported_source" 3 "Hyprland source tree is outside the supported targets."
    exit 3
  fi

  if [[ "$SOURCE_VERSION" != "$PKG_VERSION" ]]; then
    print_err "Hyprland source tree ($SOURCE_VERSION) does not match installed package headers ($PKG_VERSION).\n"
    emit_json "source_package_mismatch" 4 "Hyprland source tree does not match installed package headers."
    exit 4
  fi
fi

print_out "Hyprtasking version contract check\n"
print_out "Supported Hyprland targets: $SUPPORTED_TARGETS\n"
print_out "Supported exact versions: $SUPPORTED_VERSIONS_TEXT\n"
print_out "Installed hyprland package: $PKG_VERSION\n"
print_out "Running Hyprland runtime: $RUNTIME_VERSION\n"
if [[ -n "$HYPRLAND_SOURCE" ]]; then
  print_out "Audited Hyprland source: $HYPRLAND_SOURCE ($SOURCE_VERSION)\n"
  print_out "Build contract: installed package headers only; source tree is audit-only.\n"
fi

emit_json "ok" 0 "Installed package headers, running runtime, and optional audit source are aligned."
