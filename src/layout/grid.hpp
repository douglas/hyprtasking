#pragma once

#include <string>

#include <hyprland/src/helpers/AnimatedVariable.hpp>

#include "../types.hpp"
#include "layout_base.hpp"

class HTLayoutGrid: public HTLayoutBase {
  private:
    PHLANIMVAR<float> scale;
    PHLANIMVAR<Vector2D> offset;
    WORKSPACEID close_render_workspace_id = WORKSPACE_INVALID;
    int effective_rows = 3;
    int effective_cols = 3;

    std::vector<WORKSPACEID> collect_visible_workspaces(PHLMONITOR monitor);
    void compute_effective_shape(const std::vector<WORKSPACEID>& visible_ids, PHLMONITOR monitor);
    void place_workspaces(const std::vector<WORKSPACEID>& visible_ids, PHLMONITOR monitor, HTViewStage stage);

  public:
    HTLayoutGrid(VIEWID view_id);
    ~HTLayoutGrid() = default;

    CBox calculate_ws_box(double x, double y, HTViewStage stage);

    void close_open_lerp(float perc);
    void on_show(CallbackFun on_complete = nullptr);
    void on_hide(WORKSPACEID target_workspace_id = WORKSPACE_INVALID, CallbackFun on_complete = nullptr);
    void on_move(WORKSPACEID old_id, WORKSPACEID new_id, CallbackFun on_complete = nullptr);
    void on_move_swipe(Vector2D delta);
    WORKSPACEID on_move_swipe_end();

    WORKSPACEID get_ws_id_in_direction(int x, int y, std::string& direction);

    bool should_render_window(PHLWINDOW window);
    float drag_window_scale();
    void init_position();
    void build_overview_layout(HTViewStage stage);
    void render();
    void cancel_animation_callbacks();
};
