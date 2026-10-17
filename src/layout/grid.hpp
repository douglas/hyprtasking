#pragma once

#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <optional>
#include <string_view>

#include "../types.hpp"
#include "layout_base.hpp"

class HTLayoutGrid: public HTLayoutBase {
  private:
    struct GridGeometry {
        Vector2D transformed_size;
        Vector2D gaps;
        Vector2D start_offset;
        double opened_scale = 1.;
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
        Vector2D gaps;
        bool operator==(const LayoutInputs&) const = default;
    };

    std::optional<GridGeometry> grid_geometry();
    CBox cell_box(const GridGeometry& geometry, int x, int y, HTViewStage stage) const;

    LayoutShape layout_shape;
    std::optional<LayoutInputs> layout_inputs;
    bool redraw_requested = true;
    PHLANIMVAR<float> scale;
    PHLANIMVAR<Vector2D> offset;
    WORKSPACEID close_render_workspace_id = WORKSPACE_INVALID;
    int grid_block = -1;

  public:
    explicit HTLayoutGrid(VIEWID view_id);
    ~HTLayoutGrid() = default;

    CBox calculate_ws_box(int x, int y, HTViewStage stage);

    void close_open_lerp(float perc);
    void on_show(CallbackFun on_complete = nullptr);
    void on_hide(WORKSPACEID target_workspace_id = WORKSPACE_INVALID, CallbackFun on_complete = nullptr);
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
};
