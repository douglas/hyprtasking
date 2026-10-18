#!/usr/bin/env bash
# Source-tree audits for Hyprtasking: compat boundary, Hyprland plugin
# guidelines, config key registration, and compat contract coverage.
# Needs only rg; no Hyprland checkout.
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
SRC="$REPO_DIR/src"
source "$SCRIPT_DIR/support-matrix.sh"

failures=0

# check LABEL MATCHES: passes when MATCHES is empty.
check() {
  if [[ -z "$2" ]]; then
    printf 'ok: %s\n' "$1"
    return
  fi
  printf 'error: %s\n%s\n' "$1" "$2" >&2
  failures=$((failures + 1))
}

# src_matches PATTERN [ALLOWED]: rg hits in src/, minus lines matching ALLOWED.
src_matches() {
  local matches
  matches=$(rg -n -- "$1" "$SRC" || true)
  if [[ -n "${2:-}" && -n "$matches" ]]; then
    matches=$(printf '%s\n' "$matches" | rg -v -- "$2" || true)
  fi
  printf '%s' "$matches"
}

printf 'Hyprtasking source audit: %s\n' "$REPO_DIR"

# Boundary: direct Hyprland internals stay behind src/compat/.
COMPAT='/src/compat/'
check 'Hyprland globals outside compat' "$(src_matches 'g_p[A-Za-z0-9_]+' "$COMPAT")"
check 'raw m_ field access outside compat' "$(src_matches '->m_[A-Za-z0-9_]+' "$COMPAT")"
check 'Event bus access outside compat' "$(src_matches 'Event::bus\(\)' "$COMPAT")"
check 'monitor logical box access outside compat' "$(src_matches '->logicalBox\(\)' "$COMPAT")"
check 'window main surface box access outside compat' \
  "$(src_matches '->getWindowMainSurfaceBox\(\)' "$COMPAT")"
check 'window workspace id access outside compat' "$(src_matches '->workspaceID\(\)' "$COMPAT")"
check 'workspace monitor id access outside compat' "$(src_matches '->monitorID\(\)' "$COMPAT")"
check 'config value access outside compat' "$(src_matches 'CConfigValue<' "$COMPAT")"
check 'drift-prone Hyprland includes outside compat' "$(src_matches \
  '#include <hyprland/src/(config/ConfigManager|config/ConfigValue|layout/LayoutManager|managers/KeybindManager|managers/PointerManager|managers/animation/DesktopAnimationManager|managers/input/InputManager)\.hpp>' \
  "$COMPAT")"
check 'monitor focus mutation outside compat' \
  "$(src_matches 'HTCompat::focus_monitor\(|rawMonitorFocus\(' "$COMPAT")"
check 'render stage hooks outside compat' "$(src_matches 'on_render_stage|renderStage' "$COMPAT")"

# Hyprland plugin guidelines.
HOOK_FILES='src/compat/profile\.cpp|src/compat/renderer_compat\.cpp|src/plugin/runtime\.cpp'
check 'no thread primitives' "$(src_matches 'std::thread|std::async|pthread_')"
check 'function hooks created only in approved files' \
  "$(src_matches 'createFunctionHook\(' "$HOOK_FILES")"
check 'function hooks removed only in approved files' \
  "$(src_matches 'removeFunctionHook\(' "$HOOK_FILES")"
check 'no deprecated Hyprland plugin APIs' "$(src_matches \
  'HyprlandAPI::(addConfigKeyword|registerCallbackDynamic|unregisterCallback|addLayout|removeLayout|addDispatcher|getConfigValue|getFunctionAddressFromSignature|addConfigValue)\(' \
  'src/config\.cpp:.*HyprlandAPI::(getConfigValue|addConfigValue)\(')"
check 'hook originals read only in compat wrappers' "$(src_matches '->m_original' \
  'src/compat/profile\.(cpp|hpp)|src/compat/renderer_compat\.cpp|src/compat/runtime_compat\.cpp')"
check 'deferred callbacks never run synchronously' \
  "$(rg -n 'callback\(\);' "$SRC/compat/runtime_compat.cpp" || true)"

# Support matrix: CI and hyprpm pins track only supported Hyprland refs.
check 'no stale Hyprland refs in workflows' \
  "$(rg -n 'v0\.54\.[0-2]' "$REPO_DIR/.github/workflows" || true)"
check 'no stale Hyprland refs in hyprpm pins' \
  "$(rg -n 'v0\.(4[0-9]|5[0-3]|54\.[0-2])' "$REPO_DIR/hyprpm.toml" || true)"
missing_pins=""
for ref in "${HYPRPM_REQUIRED_REFS[@]}"; do
  rg -q "#${ref}$" "$REPO_DIR/hyprpm.toml" || missing_pins+="$ref "
done
check 'hyprpm pins cover supported Hyprland refs' "$missing_pins"

# Config keys: every key read through HTConfig is registered.
used_keys=$(rg -o --no-filename --pcre2 \
  '(?:(?:HTConfig::)?value(?:_or)?<[^>]+>|(?:int|float)_config_or)\("([^"]+)"' -r '$1' \
  "$SRC" --glob '!config.hpp' | sort -u)
registered_keys=$( {
  rg -o --no-filename 'plugin:hyprtasking:[^"]+' "$SRC" | sed 's/^plugin:hyprtasking://' || true
  rg -o --no-filename --pcre2 'add_(?:int|float)_config\("([^"]+)"' -r '$1' "$SRC/config.cpp" || true
} | sort -u)
if [[ -z "$used_keys" ]]; then
  check 'config keys detected' 'no HTConfig key usage found'
else
  check 'used config keys are registered' \
    "$(comm -23 <(printf '%s\n' "$used_keys") <(printf '%s\n' "$registered_keys"))"
fi

# Coverage: compat-sensitive symbols in src/compat/ appear in the contracts.
TOKENS='g_p[A-Za-z0-9_]+|\bm_[A-Za-z0-9_]+\b|Event::bus|changeWorkspace|dragController|removeAllOfType|onMouseButton|findFunctionsByName|removeFunctionHook|logicalBox|getWindowMainSurfaceBox|workspaceID|monitorID'
source_symbols=$(rg -o --no-filename -N -- "$TOKENS" "$SRC/compat" | sort -u)
contract_symbols=$(rg -v '^#' "$SCRIPT_DIR/compat-contracts.tsv" | cut -f4 | tr '|' '\n' |
  rg -o --no-filename -N -- "$TOKENS" | sort -u)
check 'compat symbols are covered by scripts/compat-contracts.tsv' \
  "$(comm -23 <(printf '%s\n' "$source_symbols") <(printf '%s\n' "$contract_symbols"))"

if ((failures > 0)); then
  printf '\nSource audit failed with %d issue(s).\n' "$failures" >&2
  exit 1
fi
printf '\nSource audit passed.\n'
