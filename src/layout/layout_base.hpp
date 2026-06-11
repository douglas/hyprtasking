#pragma once

#include <optional>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprutils/math/Box.hpp>
#include <unordered_map>

#include "../types.hpp"

enum HTViewStage {
    HT_VIEW_ANIMATING,
    HT_VIEW_OPENED,
    HT_VIEW_CLOSED,
};

class HTLayoutBase {
  public:
    struct HTWorkspace {
        int x;
        int y;
        CBox box;
    };

  protected:
    // Same as monitor_id of the parent view
    VIEWID view_id;

  public:
    using CallbackFun = Hyprutils::Animation::CBaseAnimatedVariable::CallbackFun;

    HTLayoutBase(VIEWID new_view_id);
    ~HTLayoutBase() = default;

    std::unordered_map<WORKSPACEID, HTWorkspace> overview_layout;

    // Called assuming that at least one overview is active (not nec on this monitor)
    bool should_render_window(PHLWINDOW window);
    void begin_render();
    void render_workspace_with_optional_state(
        PHLMONITOR monitor,
        PHLWORKSPACE workspace,
        const Time::steady_tp& time,
        const CBox& render_box
    );

    // Prevent simplification from happening in the plugin, remove all clear pass objects
    void post_render();

    PHLMONITOR get_monitor();
    const HTWorkspace* find_layout_workspace(WORKSPACEID workspace_id) const;
    WORKSPACEID get_ws_id_from_global(Vector2D pos);
    WORKSPACEID get_ws_id_from_xy(int x, int y);
    CBox get_global_window_box(PHLWINDOW window, WORKSPACEID workspace_id);
    CBox get_global_ws_box(WORKSPACEID workspace_id);

    Vector2D global_to_local_ws_unscaled(Vector2D pos, WORKSPACEID workspace_id);
    std::optional<Vector2D> global_to_workspace_monitor_coords(Vector2D pos, WORKSPACEID workspace_id);
    Vector2D local_ws_scaled_to_global(Vector2D pos, WORKSPACEID workspace_id);
    Vector2D local_ws_unscaled_to_global(Vector2D pos, WORKSPACEID workspace_id);

    struct HTBackgroundDropInfo {
        bool        can_drop = false;
        WORKSPACEID next_workspace_id = WORKSPACE_INVALID;
    };
    virtual HTBackgroundDropInfo background_drop_info() { return {}; }
    virtual void force_include_workspace(WORKSPACEID workspace_id) {}
};
