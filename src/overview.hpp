#pragma once

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/macros.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "layout/layout_base.hpp"

typedef long VIEWID;

class HTView {
  public:
    bool closing;
    bool active;
    bool navigating;
    bool hover_active = false;
    WORKSPACEID hovered_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID keyboard_workspace_id = WORKSPACE_INVALID;
    WORKSPACEID selected_workspace_id = WORKSPACE_INVALID;

    HTView(MONITORID in_monitor_id);

    void change_layout(const std::string& layout_name);

    MONITORID monitor_id;

    SP<HTLayoutBase> layout;

    void do_exit_behavior(bool exit_on_mouse);
    void warp_window(Hyprlang::INT warp, PHLWINDOW window);

    PHLMONITOR get_monitor();

    void show();
    void hide(bool exit_on_mouse);
    bool commit_selection();
    void set_runtime_state(bool new_active, bool new_closing, bool new_navigating);
    void cancel_runtime_state();
    bool has_runtime_activity() const;
    void reload_config(bool close_overview_on_reload, const std::string& new_layout);
    void reset_interaction_state();
    void clear_hover_workspace();
    void set_hovered_workspace(WORKSPACEID workspace_id);
    void set_keyboard_workspace(WORKSPACEID workspace_id);
    void set_selected_workspace_id(WORKSPACEID ws_id);
    WORKSPACEID selected_workspace_id_or(WORKSPACEID fallback) const;
    WORKSPACEID keyboard_selection_workspace_id(WORKSPACEID fallback_workspace_id) const;
    WORKSPACEID visual_workspace_id(WORKSPACEID fallback_workspace_id) const;

    void move_id(WORKSPACEID ws_id, bool move_window);
    // arg is up, down, left, right;
    void move(std::string arg, bool move_window);
    bool navigate_selection(const std::string& arg);
};

typedef SP<HTView> PHTVIEW;
typedef WP<HTView> PHTVIEWREF;
