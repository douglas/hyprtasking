#pragma once

#include <string>
#include <string_view>

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>

#include "logic/reload_model.hpp"
#include "overview.hpp"

struct HTCursorWorkspaceContext {
    PHLMONITOR  monitor = nullptr;
    PHTVIEW     view = nullptr;
    Vector2D    mouse_coords;
    WORKSPACEID workspace_id = WORKSPACE_INVALID;
    PHLWORKSPACE workspace = nullptr;
};

class HTManager {
  public:
    static constexpr VIEWID INVALID_VIEW_ID = -1;

    HTManager();

    std::vector<PHTVIEW> views;

    PHTVIEW get_view_from_monitor(PHLMONITOR pMonitor);
    PHTVIEW get_view_from_cursor();
    PHTVIEW get_view_from_id(VIEWID view_id);
    HTCursorWorkspaceContext resolve_cursor_workspace(bool create_if_missing);

    PHLWINDOW get_window_from_cursor(bool return_focused = true);

    void reset();
    void sync_monitor_views();
    void reset_drag_state();

    void show_all_views();
    void hide_all_views();
    void show_cursor_view();
    bool has_runtime_view();
    void refresh_cursor_override();
    PHLWINDOW get_dragged_window();
    bool is_dragged_window(PHLWINDOW window);
    void set_dragged_window(PHLWINDOW window);
    void clear_dragged_window();
    bool runtime_enabled() const;
    void disable_runtime(std::string_view source, std::string_view reason);
    std::string runtime_disable_reason() const;

    bool start_window_drag();
    bool end_window_drag();
    bool exit_to_workspace();
    bool on_mouse_move();
    bool on_mouse_axis(double delta);

    enum swipe_state_t {
        HT_SWIPE_OPEN,
        HT_SWIPE_MOVE,
        HT_SWIPE_NONE,
    };

    swipe_state_t swipe_state;
    float swipe_amt;
    VIEWID swipe_view_id;
    PHLWINDOWREF dragged_window;
    bool runtime_disabled = false;
    std::string disabled_reason;
    PHTVIEW get_swipe_view();
    void reset_swipe_state();
    void swipe_start();
    bool swipe_update(IPointer::SSwipeUpdateEvent e);
    bool swipe_end();

    bool has_active_view();
    bool cursor_view_active();
};
