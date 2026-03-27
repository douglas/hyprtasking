#pragma once

#include <optional>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprutils/math/Box.hpp>

#include "types.hpp"

struct HTDraggedWindowSnapshot {
    PHLWINDOW window = nullptr;
    CBox      box;
};

struct HTRenderSnapshot {
    PHLMONITOR                         monitor = nullptr;
    Time::steady_tp                    time;
    std::optional<HTDraggedWindowSnapshot> dragged_window;
};

std::optional<HTRenderSnapshot> capture_render_snapshot(VIEWID view_id, float drag_window_scale);
void render_dragged_window_snapshot(const HTRenderSnapshot& snapshot);
