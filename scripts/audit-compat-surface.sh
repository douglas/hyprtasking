#!/usr/bin/env bash
set -euo pipefail

HYPRLAND_SOURCE=${1:-${HYPRLAND_SOURCE:-/home/douglas/src/Hyprland}}
SUPPORTED_MINOR="0.54.x"
SUPPORTED_PREFIX="0.54."
AUDIT_FORMAT=${AUDIT_FORMAT:-text}
EXIT_MISSING_FILE=2
EXIT_UNSUPPORTED_VERSION=3
EXIT_CONTRACT_DRIFT=4

CHECK_LABELS=()
FAILED_LABELS=()
FAILED_PATHS=()
FAILED_OWNERS=()

print_out() {
  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    printf '%b' "$1"
  fi
}

print_err() {
  if [[ "$AUDIT_FORMAT" != "json" ]]; then
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
  local exit_code=$2
  local message=$3
  local i

  if [[ "$AUDIT_FORMAT" != "json" ]]; then
    return
  fi

  printf '{'
  printf '"status":%s,' "$(json_string "$status")"
  printf '"exit_code":%s,' "$exit_code"
  printf '"source":%s,' "$(json_string "$HYPRLAND_SOURCE")"
  printf '"supported_line":%s,' "$(json_string "$SUPPORTED_MINOR")"
  printf '"target_version":%s,' "$(json_string "${TARGET_VERSION:-}")"
  printf '"message":%s,' "$(json_string "$message")"

  printf '"checked_contracts":['
  for ((i = 0; i < ${#CHECK_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "${CHECK_LABELS[i]}")"
  done
  printf '],'

  printf '"failed_contracts":['
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    if ((i > 0)); then
      printf ','
    fi
    printf '{'
    printf '"label":%s,' "$(json_string "${FAILED_LABELS[i]}")"
    printf '"source_path":%s,' "$(json_string "${FAILED_PATHS[i]}")"
    printf '"suggested_touchpoint":%s' "$(json_string "${FAILED_OWNERS[i]}")"
    printf '}'
  done
  printf ']'
  printf '}\n'
}

require_file() {
  local path=$1
  if [[ ! -f "$path" ]]; then
    print_err "Missing required file: $path\n"
    emit_json_result "missing_file" "$EXIT_MISSING_FILE" "Missing required file: $path"
    exit "$EXIT_MISSING_FILE"
  fi
}

record_failure() {
  local label=$1
  local path=$2
  local owner=$3
  FAILED_LABELS+=("$label")
  FAILED_PATHS+=("$path")
  FAILED_OWNERS+=("$owner")
}

check_contract_group() {
  local path=$1
  local label=$2
  local owner=$3
  shift 3

  CHECK_LABELS+=("$label")

  local missing=0
  local pattern
  for pattern in "$@"; do
    if ! rg -q --fixed-strings "$pattern" "$path"; then
      missing=1
      print_err "Missing $label pattern in $path: $pattern\n"
    fi
  done

  if ((missing == 0)); then
    print_out "ok: $label\n"
    return
  fi

  record_failure "$label" "$path" "$owner"
}

VERSION_FILE="$HYPRLAND_SOURCE/VERSION"
FOCUS_STATE_HPP="$HYPRLAND_SOURCE/src/desktop/state/FocusState.hpp"
SEAT_MANAGER_HPP="$HYPRLAND_SOURCE/src/managers/SeatManager.hpp"
INPUT_MANAGER_HPP="$HYPRLAND_SOURCE/src/managers/input/InputManager.hpp"
KEYBIND_MANAGER_HPP="$HYPRLAND_SOURCE/src/managers/KeybindManager.hpp"
CURSOR_OVERRIDE_HPP="$HYPRLAND_SOURCE/src/managers/cursor/CursorShapeOverrideController.hpp"
LAYOUT_MANAGER_HPP="$HYPRLAND_SOURCE/src/layout/LayoutManager.hpp"
DRAG_CONTROLLER_HPP="$HYPRLAND_SOURCE/src/layout/supplementary/DragController.hpp"
POINTER_MANAGER_HPP="$HYPRLAND_SOURCE/src/managers/PointerManager.hpp"
COMPOSITOR_HPP="$HYPRLAND_SOURCE/src/Compositor.hpp"
MONITOR_HPP="$HYPRLAND_SOURCE/src/helpers/Monitor.hpp"
WORKSPACE_HPP="$HYPRLAND_SOURCE/src/desktop/Workspace.hpp"
WINDOW_HPP="$HYPRLAND_SOURCE/src/desktop/view/Window.hpp"
PASS_HPP="$HYPRLAND_SOURCE/src/render/pass/Pass.hpp"

require_file "$VERSION_FILE"
require_file "$FOCUS_STATE_HPP"
require_file "$SEAT_MANAGER_HPP"
require_file "$INPUT_MANAGER_HPP"
require_file "$KEYBIND_MANAGER_HPP"
require_file "$CURSOR_OVERRIDE_HPP"
require_file "$LAYOUT_MANAGER_HPP"
require_file "$DRAG_CONTROLLER_HPP"
require_file "$POINTER_MANAGER_HPP"
require_file "$COMPOSITOR_HPP"
require_file "$MONITOR_HPP"
require_file "$WORKSPACE_HPP"
require_file "$WINDOW_HPP"
require_file "$PASS_HPP"

TARGET_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

print_out "Hyprtasking compat surface audit\n"
print_out "Source: $HYPRLAND_SOURCE\n"
print_out "Supported Hyprland line: $SUPPORTED_MINOR\n"
print_out "Detected target version: $TARGET_VERSION\n"

if [[ "$TARGET_VERSION" != ${SUPPORTED_PREFIX}* ]]; then
  print_err "Unsupported Hyprland version for this plugin line: $TARGET_VERSION\n"
  print_err "Expected supported line: $SUPPORTED_MINOR\n"
  print_err "Update the plugin compat layer before attempting to load against this tree.\n"
  emit_json_result "unsupported_version" "$EXIT_UNSUPPORTED_VERSION" "Unsupported Hyprland version for this plugin line: $TARGET_VERSION"
  exit "$EXIT_UNSUPPORTED_VERSION"
fi

check_contract_group \
  "$FOCUS_STATE_HPP" \
  "focus-state APIs" \
  "src/compat/runtime_compat.cpp" \
  "void                   fullWindowFocus(PHLWINDOW w, eFocusReason reason" \
  "void                   rawMonitorFocus(PHLMONITOR m);" \
  "PHLMONITOR             monitor();"

check_contract_group \
  "$SEAT_MANAGER_HPP" \
  "seat pointer focus state" \
  "src/compat/runtime_compat.cpp" \
  "WP<CWLSurfaceResource> pointerFocus;" \
  "WP<CWLSeatResource>    pointerFocusResource;"

check_contract_group \
  "$INPUT_MANAGER_HPP" \
  "input mouse helpers" \
  "src/compat/runtime_compat.cpp" \
  "Vector2D           getMouseCoordsInternal();" \
  "void               simulateMouseMovement();"

check_contract_group \
  "$KEYBIND_MANAGER_HPP" \
  "mouse bind mode API" \
  "src/compat/runtime_compat.cpp" \
  "static SDispatchResult                         changeMouseBindMode(const eMouseBindMode mode);"

check_contract_group \
  "$CURSOR_OVERRIDE_HPP" \
  "cursor override controller" \
  "src/compat/runtime_compat.cpp" \
  "void setOverride(const std::string& name, eCursorShapeOverrideGroup group);" \
  "void unsetOverride(eCursorShapeOverrideGroup group);" \
  "inline UP<CShapeOverrideController> overrideController = makeUnique<CShapeOverrideController>();"

check_contract_group \
  "$LAYOUT_MANAGER_HPP" \
  "layout drag controller entrypoint" \
  "src/compat/runtime_compat.cpp" \
  "const UP<Supplementary::CDragStateController>& dragController();"

check_contract_group \
  "$DRAG_CONTROLLER_HPP" \
  "drag controller state accessors" \
  "src/compat/runtime_compat.cpp" \
  "void           dragEnd();" \
  "eMouseBindMode mode() const;" \
  "bool           draggingTiled() const;" \
  "SP<ITarget> target() const;"

check_contract_group \
  "$POINTER_MANAGER_HPP" \
  "pointer warp API" \
  "src/compat/renderer_compat.cpp" \
  "void warpTo(const Vector2D& logical);"

check_contract_group \
  "$COMPOSITOR_HPP" \
  "compositor lookup and workspace APIs" \
  "src/compat/runtime_compat.cpp, src/compat/renderer_compat.cpp" \
  "PHLMONITOR             getMonitorFromID(const MONITORID&);" \
  "PHLMONITOR             getMonitorFromCursor();" \
  "PHLWINDOW              vectorToWindowUnified(const Vector2D&, uint8_t properties, PHLWINDOW pIgnoreWindow = nullptr);" \
  "PHLWORKSPACE           getWorkspaceByID(const WORKSPACEID&);" \
  "std::vector<PHLWORKSPACE> getWorkspacesCopy();" \
  "void                   moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);" \
  "void                   scheduleFrameForMonitor(PHLMONITOR, Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);" \
  "void                   closeWindow(PHLWINDOW);" \
  "[[nodiscard]] PHLWORKSPACE          createNewWorkspace(const WORKSPACEID&, const MONITORID&," \
  "void                                moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);"

check_contract_group \
  "$MONITOR_HPP" \
  "monitor focus and render fields" \
  "src/compat/renderer_compat.cpp" \
  "PHLWORKSPACE                m_activeWorkspace        = nullptr;" \
  "std::string                 m_description      = \"\";" \
  "void        changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal = false, bool noMouseMove = false, bool noFocus = false);" \
  "bool                                m_blurFBShouldRender = false;"

check_contract_group \
  "$WORKSPACE_HPP" \
  "workspace render state fields" \
  "src/compat/renderer_compat.cpp" \
  "PHLMONITORREF   m_monitor;" \
  "PHLANIMVAR<Vector2D>       m_renderOffset;" \
  "bool m_visible = false;" \
  "bool m_isSpecialWorkspace = false;"

check_contract_group \
  "$WINDOW_HPP" \
  "window animation and workspace fields" \
  "src/compat/renderer_compat.cpp, src/compat/runtime_compat.cpp" \
  "PHLANIMVAR<Vector2D> m_realPosition;" \
  "PHLANIMVAR<Vector2D> m_realSize;" \
  "PHLWORKSPACE     m_workspace;" \
  "PHLMONITORREF    m_monitor, m_prevMonitor;" \
  "PHLANIMVAR<float> m_movingFromWorkspaceAlpha;" \
  "PHLANIMVAR<float> m_movingToWorkspaceAlpha;" \
  "void                       warpCursor(bool force = false);"

check_contract_group \
  "$PASS_HPP" \
  "render pass clear API" \
  "src/compat/renderer_compat.cpp" \
  "void    removeAllOfType(const std::string& type);"

print_out "\nChecked compat surface groups (${#CHECK_LABELS[@]}):\n"
for label in "${CHECK_LABELS[@]}"; do
  print_out " - $label\n"
done

if (( ${#FAILED_LABELS[@]} > 0 )); then
  print_err "\nCompat surface audit failed with ${#FAILED_LABELS[@]} issue(s).\n"
  print_err "Missing or changed runtime-sensitive contracts:\n"
  for ((i = 0; i < ${#FAILED_LABELS[@]}; i++)); do
    print_err " - ${FAILED_LABELS[i]} (${FAILED_PATHS[i]})\n"
    print_err "   Suggested plugin touchpoint: ${FAILED_OWNERS[i]}\n"
  done
  print_err "Compat contract reference: docs/compat-contract.md\n"
  print_err "Rerun with HYPRLAND_SOURCE set to the target Hyprland checkout after patching.\n"
  emit_json_result "contract_drift" "$EXIT_CONTRACT_DRIFT" "One or more audited Hyprland runtime compat contracts drifted on a supported line."
  exit "$EXIT_CONTRACT_DRIFT"
fi

print_out "\nCompat surface audit passed for Hyprland $TARGET_VERSION on supported line $SUPPORTED_MINOR.\n"
emit_json_result "ok" 0 "Compat surface audit passed."
