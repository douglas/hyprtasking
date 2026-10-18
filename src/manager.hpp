#pragma once

#include <chrono>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <string>
#include <string_view>

#include "compat/runtime_compat.hpp"
#include "logic/interaction_model.hpp"
#include "logic/view_sync_model.hpp"
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
    HTManager() = default;
    ~HTManager();
    HTManager(const HTManager&) = delete;
    HTManager& operator=(const HTManager&) = delete;

    std::vector<PHTVIEW> views;

    PHTVIEW get_view_from_monitor(PHLMONITOR pMonitor);
    PHTVIEW get_view_from_cursor();
    PHTVIEW get_view_from_id(VIEWID view_id);
    HTCursorWorkspaceContext resolve_cursor_workspace();

    void prepare_for_unload();
    void sync_monitor_views(MONITORID removed_monitor_id = MONITOR_INVALID);
    void cancel_window_drag();

    void show_all_views();
    void hide_all_views();
    void show_cursor_view();
    void on_window_set_changed();
    void refresh_runtime_activity();
    PHLWINDOW get_dragged_window();
    bool is_dragged_window(PHLWINDOW window);
    bool runtime_enabled() const;
    void disable_runtime(std::string_view source, std::string_view reason);
    void block_runtime(std::string_view source, std::string_view reason);
    std::string runtime_health_summary() const;

    HTLogic::MouseButtonResult handle_mouse_button(unsigned int button, bool pressed);
    void on_mouse_move();

    void reset_swipe_state();
    bool swipe_update(IPointer::SSwipeUpdateEvent e);
    bool swipe_end(bool cancelled);

    bool has_active_view();
    bool has_interactively_active_view();
    bool cursor_view_active();

  private:
    static constexpr VIEWID INVALID_VIEW_ID = -1;
    enum class mouse_interaction_t {
        HT_MOUSE_NONE,
        HT_MOUSE_DRAG,
        HT_MOUSE_SELECT,
    };
    using enum mouse_interaction_t;

    enum class swipe_state_t {
        HT_SWIPE_OPEN,
        HT_SWIPE_MOVE,
        HT_SWIPE_NONE,
    };
    using enum swipe_state_t;

    swipe_state_t swipe_state = HT_SWIPE_NONE;
    float swipe_amt = 0.F;
    bool swipe_started_open = false;
    VIEWID swipe_view_id = INVALID_VIEW_ID;
    PHLWINDOWREF dragged_window;
    bool runtime_disabled = false;
    bool compat_blocked = false;
    static constexpr std::chrono::milliseconds IDLE_REDRAW_INTERVAL {100};
    std::vector<CHyprSignalListener> surface_commit_listeners;
    SP<CEventLoopTimer> idle_redraw_timer;
    bool redraw_sources_active = false;
    // Workspace block slots by monitor name; kept across hotplug.
    std::vector<std::string> monitor_blocks;
    std::string disabled_reason;
    std::string disabled_source;

    bool has_runtime_view();
    void request_redraw(PHLMONITOR monitor);
    void clear_dragged_window();
    void reset_runtime_controls();
    void reset_drag_state();
    void abort_drag_controller();
    void sync_redraw_sources(bool rebuild);
    void reset_mouse_button_state();
    bool start_window_drag(const HTCursorWorkspaceContext& cursor_context);
    void end_window_drag();
    PHTVIEW get_swipe_view();

    // 0 while no managed button is held; config rejects buttons <= 0.
    unsigned int claimed_mouse_button = 0;
    // HT_MOUSE_DRAG only once a drag started, HT_MOUSE_SELECT while a
    // selection is pending.
    mouse_interaction_t claimed_mouse_interaction = HT_MOUSE_NONE;
    VIEWID claimed_mouse_view_id = INVALID_VIEW_ID;
    WORKSPACEID pending_mouse_selection_workspace_id = WORKSPACE_INVALID;
};
