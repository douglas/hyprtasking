#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <string_view>

#include "layout/grid.hpp"

class HTView {
  public:
    bool closing = false;
    bool active = false;
    bool navigating = false;
    // WORKSPACE_INVALID while nothing is hovered or suppressed.
    WORKSPACEID hovered_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID hover_suppressed_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID keyboard_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID selected_workspace_id = WORKSPACE_INVALID;

    explicit HTView(MONITORID in_monitor_id);

    MONITORID monitor_id;

    SP<HTLayoutGrid> layout;

  private:
    PHLMONITORREF cached_monitor;
    bool warned_missing_monitor = false;

  public:
    PHLMONITOR get_monitor();
    // Re-resolve the monitor from monitor_id; call after monitor changes.
    void refresh_monitor();

    void show();
    void hide();
    bool commit_selection();
    bool commit_mouse_selection(WORKSPACEID target_workspace_id);
    void set_runtime_state(bool new_active, bool new_closing, bool new_navigating);
    void cancel_runtime_state();
    bool has_runtime_activity() const;
    void reload_config();
    void reset_interaction_state();
    void clear_hover_workspace();
    void clear_hover_suppression();
    void suppress_hover_workspace(WORKSPACEID workspace_id);
    void hover_workspace(WORKSPACEID workspace_id);
    void set_keyboard_workspace(WORKSPACEID workspace_id);
    WORKSPACEID keyboard_selection_workspace_id(WORKSPACEID fallback_workspace_id) const;
    WORKSPACEID visual_workspace_id(WORKSPACEID fallback_workspace_id) const;

    void move_id(WORKSPACEID ws_id);
    // arg is up, down, left, right;
    void move(std::string_view arg);
    bool navigate_selection(const std::string& arg);
};

using PHTVIEW = SP<HTView>;
