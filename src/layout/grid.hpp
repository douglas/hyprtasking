#pragma once

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprutils/math/Box.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "../logic/logic.hpp"
#include "../types.hpp"

enum class HTViewStage {
    HT_VIEW_ANIMATING,
    HT_VIEW_OPENED,
    HT_VIEW_CLOSED,
};
using enum HTViewStage;

class HTLayoutGrid {
  public:
    struct HTWorkspace {
        int x;
        int y;
        CBox box;
    };

    using CallbackFun = Hyprutils::Animation::CBaseAnimatedVariable::CallbackFun;

    explicit HTLayoutGrid(VIEWID view_id);

    std::unordered_map<WORKSPACEID, HTWorkspace> overview_layout;

    PHLMONITOR get_monitor();
    const HTWorkspace* find_layout_workspace(WORKSPACEID workspace_id) const;
    WORKSPACEID get_ws_id_from_global(Vector2D pos);
    CBox get_global_ws_box(WORKSPACEID workspace_id);
    Vector2D global_to_local_ws_unscaled(Vector2D pos, WORKSPACEID workspace_id);
    std::optional<Vector2D>
    global_to_workspace_monitor_coords(Vector2D pos, WORKSPACEID workspace_id);
    Vector2D local_ws_unscaled_to_global(Vector2D pos, WORKSPACEID workspace_id);

    void close_open_lerp(float perc);
    void on_show(CallbackFun on_complete = nullptr);
    void
    on_hide(WORKSPACEID target_workspace_id = WORKSPACE_INVALID, CallbackFun on_complete = nullptr);
    void on_move(WORKSPACEID old_id, WORKSPACEID new_id, CallbackFun on_complete = nullptr);
    void on_move_swipe(Vector2D delta);
    WORKSPACEID on_move_swipe_end();

    WORKSPACEID get_ws_id_in_direction(int x, int y, std::string_view direction);

    bool should_render_window(PHLWINDOW window, bool original_result);
    float drag_window_scale();
    void init_position();
    void set_grid_block(int block);
    void place_workspaces();
    void build_overview_layout(HTViewStage stage);
    void render();
    // Draw at least one more overview frame on this monitor.
    void request_redraw();
    bool has_pending_motion() const;
    void cancel_animation_callbacks();

  private:
    struct GridGeometry {
        Vector2D transformed_size;
        HTLogic::GridFit fit;
    };

    struct LayoutShape {
        int block = -1;
        int rows = 0;
        int cols = 0;
        bool operator==(const LayoutShape&) const = default;
    };

    // Everything the cell boxes depend on; unchanged inputs skip the rebuild.
    struct LayoutInputs {
        LayoutShape shape;
        HTViewStage stage = HT_VIEW_CLOSED;
        float scale = 0.F;
        Vector2D offset;
        Vector2D transformed_size;
        double gap = 0;
        bool operator==(const LayoutInputs&) const = default;
    };

    std::optional<GridGeometry> grid_geometry();
    CBox cell_box(const GridGeometry& geometry, int x, int y, HTViewStage stage) const;
    CBox calculate_ws_box(int x, int y, HTViewStage stage);
    void set_offset_end_callback(CallbackFun on_complete);
    std::optional<HTLogic::CellMapping> cell_mapping(WORKSPACEID workspace_id);
    Vector2D local_ws_scaled_to_global(Vector2D pos, WORKSPACEID workspace_id);
    WORKSPACEID get_ws_id_from_xy(int x, int y);

    // Same as monitor_id of the parent view
    VIEWID view_id;
    std::optional<LayoutInputs> layout_inputs;
    PHLANIMVAR<float> scale;
    PHLANIMVAR<Vector2D> offset;
    WORKSPACEID close_render_workspace_id = WORKSPACE_INVALID;
    int grid_block = -1;
};
