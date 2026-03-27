#!/usr/bin/env bash
set -euo pipefail

MODE=${1:-all}
CHECKLIST_FORMAT=${CHECKLIST_FORMAT:-text}

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

json_array_from_lines() {
  local lines=$1
  local first=1
  local line

  printf '['
  while IFS= read -r line; do
    [[ -z "$line" ]] && continue
    if ((first == 0)); then
      printf ','
    fi
    printf '%s' "$(json_string "$line")"
    first=0
  done <<< "$lines"
  printf ']'
}

emit_text_scenario() {
  local number=$1
  local title=$2
  local setup=$3
  local action=$4
  local commands=$5
  local pass=$6
  local fail=$7

  printf '%s. %s\n' "$number" "$title"
  printf '   Setup: %s\n' "$setup"
  if [[ -n "$action" ]]; then
    printf '   Action: %s\n' "$action"
  fi
  if [[ -n "$commands" ]]; then
    printf '   Commands:\n'
    while IFS= read -r line; do
      [[ -z "$line" ]] && continue
      printf '     %s\n' "$line"
    done <<< "$commands"
  fi
  printf '   Pass: %s\n' "$pass"
  printf '   Fail: %s\n' "$fail"
}

emit_json_scenario() {
  local id=$1
  local number=$2
  local title=$3
  local setup=$4
  local action=$5
  local commands=$6
  local pass=$7
  local fail=$8

  printf '{'
  printf '"id":%s,' "$(json_string "$id")"
  printf '"number":%s,' "$number"
  printf '"title":%s,' "$(json_string "$title")"
  printf '"setup":%s,' "$(json_string "$setup")"
  printf '"action":%s,' "$(json_string "$action")"
  printf '"commands":%s,' "$(json_array_from_lines "$commands")"
  printf '"pass_criteria":%s,' "$(json_string "$pass")"
  printf '"fail_criteria":%s' "$(json_string "$fail")"
  printf '}'
}

print_drag() {
  emit_text_scenario \
    1 \
    'Drag a tiled window across overview workspaces.' \
    'open the overview on a monitor with at least one tiled window.' \
    'drag the window across at least two workspace targets and drop it.' \
    '' \
    'the dragged window stays attached to the cursor, renders once, and drops on the expected workspace.' \
    'duplicate dragged windows, large drag offset, hidden selected workspace, or stuck mouse bind mode.'
}

print_movewindow() {
  emit_text_scenario \
    3 \
    'Run hyprtasking:movewindow in each direction from an occupied workspace.' \
    'hover a normal tiled window in the overview.' \
    '' \
    $'hyprctl dispatch hyprtasking:movewindow right\nhyprctl dispatch hyprtasking:movewindow left\nhyprctl dispatch hyprtasking:movewindow up\nhyprctl dispatch hyprtasking:movewindow down' \
    'the hovered window moves to the target workspace, focus follows the move, and cursor/workspace state stays aligned.' \
    'cursor ends up on the old workspace, the wrong window moves, or the target workspace transition is visually wrong.'
}

print_gesture() {
  emit_text_scenario \
    4 \
    'Open and interrupt gesture navigation mid-open and mid-move.' \
    'ensure gestures are enabled in plugin config.' \
    'begin the open gesture, stop halfway, then begin a move gesture and interrupt it before completion.' \
    '' \
    'the owning overview closes or recovers cleanly, and swipe state does not stay stuck.' \
    'input feels captured, cursor override sticks, or a later gesture acts on the wrong monitor/view.'
}

print_reload_open() {
  emit_text_scenario \
    5 \
    'Reload Hyprland while the overview is open.' \
    '' \
    '' \
    $'hyprctl dispatch hyprtasking:toggle cursor\nhyprctl reload' \
    'Hyprtasking reloads cleanly, dispatchers remain registered, and the compositor stays usable.' \
    'invalid dispatcher, overview remains half-open, cursor override persists, or Hyprland becomes unstable.'
}

print_monitor_remove() {
  emit_text_scenario \
    6 \
    'If available, remove or disable a monitor while the overview or a gesture is active.' \
    'open the overview or hold an in-progress gesture on the monitor you plan to remove.' \
    'disconnect the monitor or disable it through your normal Hyprland monitor workflow.' \
    '' \
    'stale views disappear cleanly and no drag/gesture state survives the topology change.' \
    'crash, frozen input, or callbacks acting on a removed monitor.'
}

print_typing_focus() {
  emit_text_scenario \
    2 \
    'Right-click a workspace in grid mode, then type immediately in the selected workspace.' \
    'open the overview in grid mode on a workspace with a terminal or Nautilus already open.' \
    'right-click to select the target workspace, then immediately type in the terminal or press Ctrl+L in Nautilus.' \
    '' \
    'typing works immediately with no extra Enter, and Nautilus path editing activates on the first Ctrl+L.' \
    'the selected workspace is visible but does not accept immediate typing, or an extra Enter is needed before input lands.'
}

print_header() {
  printf 'Manual runtime checklist:\n'
  printf '\n'
}

json_drag() {
  emit_json_scenario \
    "drag" \
    1 \
    'Drag a tiled window across overview workspaces.' \
    'open the overview on a monitor with at least one tiled window.' \
    'drag the window across at least two workspace targets and drop it.' \
    '' \
    'the dragged window stays attached to the cursor, renders once, and drops on the expected workspace.' \
    'duplicate dragged windows, large drag offset, hidden selected workspace, or stuck mouse bind mode.'
}

json_typing_focus() {
  emit_json_scenario \
    "typing-focus" \
    2 \
    'Right-click a workspace in grid mode, then type immediately in the selected workspace.' \
    'open the overview in grid mode on a workspace with a terminal or Nautilus already open.' \
    'right-click to select the target workspace, then immediately type in the terminal or press Ctrl+L in Nautilus.' \
    '' \
    'typing works immediately with no extra Enter, and Nautilus path editing activates on the first Ctrl+L.' \
    'the selected workspace is visible but does not accept immediate typing, or an extra Enter is needed before input lands.'
}

json_movewindow() {
  emit_json_scenario \
    "movewindow" \
    3 \
    'Run hyprtasking:movewindow in each direction from an occupied workspace.' \
    'hover a normal tiled window in the overview.' \
    '' \
    $'hyprctl dispatch hyprtasking:movewindow right\nhyprctl dispatch hyprtasking:movewindow left\nhyprctl dispatch hyprtasking:movewindow up\nhyprctl dispatch hyprtasking:movewindow down' \
    'the hovered window moves to the target workspace, focus follows the move, and cursor/workspace state stays aligned.' \
    'cursor ends up on the old workspace, the wrong window moves, or the target workspace transition is visually wrong.'
}

json_gesture() {
  emit_json_scenario \
    "gesture" \
    4 \
    'Open and interrupt gesture navigation mid-open and mid-move.' \
    'ensure gestures are enabled in plugin config.' \
    'begin the open gesture, stop halfway, then begin a move gesture and interrupt it before completion.' \
    '' \
    'the owning overview closes or recovers cleanly, and swipe state does not stay stuck.' \
    'input feels captured, cursor override sticks, or a later gesture acts on the wrong monitor/view.'
}

json_reload_open() {
  emit_json_scenario \
    "reload-open" \
    5 \
    'Reload Hyprland while the overview is open.' \
    '' \
    '' \
    $'hyprctl dispatch hyprtasking:toggle cursor\nhyprctl reload' \
    'Hyprtasking reloads cleanly, dispatchers remain registered, and the compositor stays usable.' \
    'invalid dispatcher, overview remains half-open, cursor override persists, or Hyprland becomes unstable.'
}

json_monitor_remove() {
  emit_json_scenario \
    "monitor-remove" \
    6 \
    'If available, remove or disable a monitor while the overview or a gesture is active.' \
    'open the overview or hold an in-progress gesture on the monitor you plan to remove.' \
    'disconnect the monitor or disable it through your normal Hyprland monitor workflow.' \
    '' \
    'stale views disappear cleanly and no drag/gesture state survives the topology change.' \
    'crash, frozen input, or callbacks acting on a removed monitor.'
}

emit_json_mode() {
  local scenarios=""

  append_scenario() {
    local scenario_json=$1
    if [[ -n "$scenarios" ]]; then
      scenarios+=","
    fi
    scenarios+="$scenario_json"
  }

  case "$MODE" in
    all)
      append_scenario "$(json_drag)"
      append_scenario "$(json_typing_focus)"
      append_scenario "$(json_movewindow)"
      append_scenario "$(json_gesture)"
      append_scenario "$(json_reload_open)"
      append_scenario "$(json_monitor_remove)"
      ;;
    drag)
      append_scenario "$(json_drag)"
      ;;
    typing-focus)
      append_scenario "$(json_typing_focus)"
      ;;
    movewindow)
      append_scenario "$(json_movewindow)"
      ;;
    gesture)
      append_scenario "$(json_gesture)"
      ;;
    reload-open)
      append_scenario "$(json_reload_open)"
      ;;
    monitor-remove)
      append_scenario "$(json_monitor_remove)"
      ;;
    *)
      printf 'Usage: %s [all|drag|typing-focus|movewindow|gesture|reload-open|monitor-remove]\n' "$0" >&2
      exit 1
      ;;
  esac

  printf '{'
  printf '"scenario":%s,' "$(json_string "$MODE")"
  printf '"scenarios":[%s]' "$scenarios"
  printf '}\n'
}

if [[ "$CHECKLIST_FORMAT" == "json" ]]; then
  emit_json_mode
  exit 0
fi

case "$MODE" in
  all)
    print_header
    print_drag
    printf '\n'
    print_typing_focus
    printf '\n'
    print_movewindow
    printf '\n'
    print_gesture
    printf '\n'
    print_reload_open
    printf '\n'
    print_monitor_remove
    ;;
  drag)
    print_header
    print_drag
    ;;
  typing-focus)
    print_header
    print_typing_focus
    ;;
  movewindow)
    print_header
    print_movewindow
    ;;
  gesture)
    print_header
    print_gesture
    ;;
  reload-open)
    print_header
    print_reload_open
    ;;
  monitor-remove)
    print_header
    print_monitor_remove
    ;;
  *)
    printf 'Usage: %s [all|drag|typing-focus|movewindow|gesture|reload-open|monitor-remove]\n' "$0" >&2
    exit 1
    ;;
esac
