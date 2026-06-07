#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "layout/grid.hpp"

class HTView {
  public:
    bool closing = false;
    bool active = false;
    bool navigating = false;
    bool hover_active = false;
    WORKSPACEID hovered_workspace_id = WORKSPACE_INVALID;
    bool hover_suppressed = false;
    WORKSPACEID hover_suppressed_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID keyboard_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID selected_workspace_id = WORKSPACE_INVALID;

    HTView(MONITORID in_monitor_id);

    MONITORID monitor_id;

    SP<HTLayoutGrid> layout;

    PHLMONITOR get_monitor();

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
    void set_hovered_workspace(WORKSPACEID workspace_id);
    void set_keyboard_workspace(WORKSPACEID workspace_id);
    void set_selected_workspace_id(WORKSPACEID ws_id);
    WORKSPACEID keyboard_selection_workspace_id(WORKSPACEID fallback_workspace_id) const;
    WORKSPACEID visual_workspace_id(WORKSPACEID fallback_workspace_id) const;

    void move_id(WORKSPACEID ws_id);
    // arg is up, down, left, right;
    void move(std::string arg);
    bool select_workspace(WORKSPACEID workspace_id);
    bool navigate_selection(const std::string& arg);
};

typedef SP<HTView> PHTVIEW;
