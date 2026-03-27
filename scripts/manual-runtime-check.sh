#!/usr/bin/env bash
set -euo pipefail

MODE=${1:-all}

print_drag() {
  printf '1. Drag a tiled window across overview workspaces.\n'
  printf '   Setup: open the overview on a monitor with at least one tiled window.\n'
  printf '   Action: drag the window across at least two workspace targets and drop it.\n'
  printf '   Pass: the dragged window stays attached to the cursor, renders once, and drops on the expected workspace.\n'
  printf '   Fail: duplicate dragged windows, large drag offset, hidden selected workspace, or stuck mouse bind mode.\n'
}

print_movewindow() {
  printf '3. Run hyprtasking:movewindow in each direction from an occupied workspace.\n'
  printf '   Setup: hover a normal tiled window in the overview.\n'
  printf '   Commands:\n'
  printf '     hyprctl dispatch hyprtasking:movewindow right\n'
  printf '     hyprctl dispatch hyprtasking:movewindow left\n'
  printf '     hyprctl dispatch hyprtasking:movewindow up\n'
  printf '     hyprctl dispatch hyprtasking:movewindow down\n'
  printf '   Pass: the hovered window moves to the target workspace, focus follows the move, and cursor/workspace state stays aligned.\n'
  printf '   Fail: cursor ends up on the old workspace, the wrong window moves, or the target workspace transition is visually wrong.\n'
}

print_gesture() {
  printf '4. Open and interrupt gesture navigation mid-open and mid-move.\n'
  printf '   Setup: ensure gestures are enabled in plugin config.\n'
  printf '   Action: begin the open gesture, stop halfway, then begin a move gesture and interrupt it before completion.\n'
  printf '   Pass: the owning overview closes or recovers cleanly, and swipe state does not stay stuck.\n'
  printf '   Fail: input feels captured, cursor override sticks, or a later gesture acts on the wrong monitor/view.\n'
}

print_reload_open() {
  printf '5. Reload Hyprland while the overview is open.\n'
  printf '   Commands:\n'
  printf '     hyprctl dispatch hyprtasking:toggle cursor\n'
  printf '     hyprctl reload\n'
  printf '   Pass: Hyprtasking reloads cleanly, dispatchers remain registered, and the compositor stays usable.\n'
  printf '   Fail: invalid dispatcher, overview remains half-open, cursor override persists, or Hyprland becomes unstable.\n'
}

print_monitor_remove() {
  printf '6. If available, remove or disable a monitor while the overview or a gesture is active.\n'
  printf '   Setup: open the overview or hold an in-progress gesture on the monitor you plan to remove.\n'
  printf '   Action: disconnect the monitor or disable it through your normal Hyprland monitor workflow.\n'
  printf '   Pass: stale views disappear cleanly and no drag/gesture state survives the topology change.\n'
  printf '   Fail: crash, frozen input, or callbacks acting on a removed monitor.\n'
}

print_typing_focus() {
  printf '2. Right-click a workspace in grid mode, then type immediately in the selected workspace.\n'
  printf '   Setup: open the overview in grid mode on a workspace with a terminal or Nautilus already open.\n'
  printf '   Action: right-click to select the target workspace, then immediately type in the terminal or press Ctrl+L in Nautilus.\n'
  printf '   Pass: typing works immediately with no extra Enter, and Nautilus path editing activates on the first Ctrl+L.\n'
  printf '   Fail: the selected workspace is visible but does not accept immediate typing, or an extra Enter is needed before input lands.\n'
}

print_header() {
  printf 'Manual runtime checklist:\n'
  printf '\n'
}

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
